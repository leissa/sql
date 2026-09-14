#!/usr/bin/env python3
"""Generates a large SQL file to benchmark against.

The fixtures under `test/job/` and `test/tpch/` are real but small - a few hundred bytes each - so a
run over them measures the per-statement overheads more than anything else. This emits one big file
instead, where the arena, the SymPool, and the lexer's buffers all get to amortize.

Names come from the vocabulary the Join Order Benchmark and TPC-H actually use: short, and reused
constantly. That is what real SQL looks like, and it is also what an interning lexer is built for -
a Sym of at most eight bytes needs no pool allocation at all. `--stress-names` makes every
identifier distinct instead, which is the worst case such a design can be handed.

    test/bench/gen.py --mb 64 -o big.sql
    ./build/bin/bench --once big.sql
    ./build/bin/bench --lex big.sql
"""

import argparse
import pathlib
import random
import sys

# Straight out of test/job/ and test/tpch/.
TABLES = [
    "lineitem", "orders", "customer", "part", "supplier", "nation", "region", "partsupp",
    "title", "movie_info", "cast_info", "company_name", "keyword", "movie_keyword", "aka_title",
    "name", "person_info", "char_name", "role_type", "kind_type", "info_type", "link_type",
]

# Correlation names the way people actually write them: one to three characters.
ALIASES = ["t", "n", "k", "mc", "ci", "cn", "mi", "mk", "an", "at", "pi", "rt", "kt", "it", "lt",
           "l", "o", "c", "p", "s", "ps", "r", "a", "b", "x", "y"]

COLS = [
    "id", "name", "note", "info", "title", "kind_id", "movie_id", "person_id", "keyword",
    "production_year", "company_id", "role_id", "info_type_id", "linked_movie_id", "md5sum",
    "l_orderkey", "l_quantity", "l_extendedprice", "l_discount", "l_tax", "l_shipdate",
    "o_orderkey", "o_totalprice", "o_orderdate", "o_custkey", "c_name", "c_acctbal", "c_nationkey",
    "p_partkey", "p_size", "p_type", "s_name", "s_suppkey", "n_name", "n_regionkey", "r_name",
]

# What people name a computed column.
OUT = ["revenue", "total", "cnt", "num", "avg_price", "sum_qty", "nm", "val", "amount", "score",
       "min_year", "max_year", "movie", "person", "company", "cost", "profit", "rank"]

TYPES = ["integer", "character varying(64)", "decimal(12, 2)", "date", "text", "bigint"]
AGGS = ["sum", "avg", "min", "max", "count"]


class Gen:
    def __init__(self, seed, stress):
        self.rnd = random.Random(seed)
        self.stress = stress
        self.n = 0

    def name(self, pool):
        """One name out of `pool` - or, under --stress-names, one nobody has used yet."""
        base = self.rnd.choice(pool)
        if not self.stress:
            return base
        self.n += 1
        return f"{base}_{self.n}"

    def col(self, alias):
        return f"{alias}.{self.name(COLS)}"

    def expr(self, aliases, depth=0):
        r = self.rnd.random()
        alias = self.rnd.choice(aliases)
        if depth > 2 or r < 0.38:
            return self.col(alias)
        if r < 0.48:
            return str(self.rnd.randint(1, 10000))
        if r < 0.55:
            return f"'{self.rnd.choice(OUT)}'"
        if r < 0.65:
            op = self.rnd.choice(["+", "-", "*", "/"])
            return f"({self.expr(aliases, depth + 1)} {op} {self.expr(aliases, depth + 1)})"
        if r < 0.74:
            return f"{self.rnd.choice(AGGS)}({self.expr(aliases, depth + 1)})"
        if r < 0.82:
            return f"CAST({self.expr(aliases, depth + 1)} AS decimal(12, 2))"
        if r < 0.90:
            return (f"CASE WHEN {self.pred(aliases, depth + 1)} THEN {self.expr(aliases, depth + 1)}"
                    f" ELSE {self.expr(aliases, depth + 1)} END")
        if r < 0.96:
            return f"COALESCE({self.expr(aliases, depth + 1)}, {self.expr(aliases, depth + 1)})"
        return f"SUBSTRING({self.col(alias)}, 2, 5)"

    def pred(self, aliases, depth=0):
        r = self.rnd.random()
        if depth > 2 or r < 0.40:
            op = self.rnd.choice(["=", "<>", "<", "<=", ">", ">="])
            return f"{self.expr(aliases, depth + 1)} {op} {self.expr(aliases, depth + 1)}"
        if r < 0.55:
            return f"({self.pred(aliases, depth + 1)} AND {self.pred(aliases, depth + 1)})"
        if r < 0.68:
            return f"({self.pred(aliases, depth + 1)} OR {self.pred(aliases, depth + 1)})"
        if r < 0.76:
            return (f"{self.expr(aliases, depth + 1)} BETWEEN {self.rnd.randint(1, 100)}"
                    f" AND {self.rnd.randint(101, 999)}")
        if r < 0.84:
            vals = ", ".join(f"'{self.rnd.choice(OUT)}'" for _ in range(self.rnd.randint(2, 6)))
            return f"{self.expr(aliases, depth + 1)} IN ({vals})"
        if r < 0.92:
            return f"{self.col(self.rnd.choice(aliases))} LIKE '%{self.rnd.choice(OUT)}%'"
        return f"NOT ({self.pred(aliases, depth + 1)})"

    def select(self, depth=0):
        # Correlation names stay distinct within one query, the way a real one keeps them apart.
        n = self.rnd.randint(2, 5)
        aliases = [self.name(ALIASES) for _ in range(n)] if self.stress else self.rnd.sample(ALIASES, n)

        froms = f"{self.name(TABLES)} AS {aliases[0]}"
        for alias in aliases[1:]:
            kind = self.rnd.choice(["INNER", "LEFT", "RIGHT"])
            froms += f" {kind} JOIN {self.name(TABLES)} AS {alias} ON {self.col(aliases[0])} = {self.col(alias)}"

        elems = ", ".join(f"{self.expr(aliases)} AS {self.name(OUT)}" for _ in range(self.rnd.randint(3, 9)))
        sql = f"SELECT {elems} FROM {froms} WHERE {self.pred(aliases)}"

        if self.rnd.random() < 0.5:
            groups = ", ".join(self.col(self.rnd.choice(aliases)) for _ in range(self.rnd.randint(1, 3)))
            sql += f" GROUP BY {groups}"
            if self.rnd.random() < 0.5:
                sql += f" HAVING {self.rnd.choice(AGGS)}({self.col(aliases[0])}) > {self.rnd.randint(1, 999)}"
        if depth == 0:
            if self.rnd.random() < 0.4:
                sql += f" ORDER BY {self.col(aliases[0])} DESC"
            if self.rnd.random() < 0.3:
                sql += f" LIMIT {self.rnd.randint(1, 100)}"
        return sql

    def stmt(self):
        r = self.rnd.random()
        if r < 0.62:
            return self.select()
        if r < 0.72:
            return f"{self.select(1)} UNION ALL {self.select(1)}"
        if r < 0.80:
            cols = ", ".join(f"{self.name(COLS)} {self.rnd.choice(TYPES)}"
                             for _ in range(self.rnd.randint(4, 12)))
            return f"CREATE TABLE {self.name(TABLES)} ({cols})"
        if r < 0.88:
            # One row per INSERT, so other parsers can chew on the same corpus for comparison.
            row = ", ".join(str(self.rnd.randint(1, 9999)) for _ in range(4))
            return f"INSERT INTO {self.name(TABLES)} (id, name, note, info) VALUES ({row})"
        if r < 0.94:
            table = self.name(TABLES)
            sets = ", ".join(f"{self.name(COLS)} = {self.rnd.randint(1, 999)}"
                             for _ in range(self.rnd.randint(1, 4)))
            return f"UPDATE {table} SET {sets} WHERE {self.pred([table])}"
        table = self.name(TABLES)
        return f"DELETE FROM {table} WHERE {self.pred([table])}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mb", type=float, default=64, help="target size in MiB (default: 64)")
    parser.add_argument("--stress-names", action="store_true",
                        help="give every identifier a distinct suffix - the worst case for interning")
    parser.add_argument("--seed", type=int, default=42, help="so the corpus is reproducible")
    parser.add_argument("-o", "--out", type=pathlib.Path, required=True, help="where to write it")
    args = parser.parse_args()

    gen = Gen(args.seed, args.stress_names)
    target = int(args.mb * 1024 * 1024)
    written = 0

    with args.out.open("w") as f:
        f.write("-- Generated by test/bench/gen.py - see there. Do not edit, do not commit.\n")
        while written < target:
            chunk = "".join(f"{gen.stmt()};\n" for _ in range(256))
            f.write(chunk)
            written += len(chunk)

    kind = "distinct" if args.stress_names else "reused"
    print(f"{args.out}: {written / 1024 / 1024:.1f} MiB, {kind} names", file=sys.stderr)


if __name__ == "__main__":
    main()
