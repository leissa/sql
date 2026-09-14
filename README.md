# SQL

[![Stars](https://img.shields.io/github/stars/leissa/sql)](https://github.com/leissa/sql/stargazers)
[![Forks](https://img.shields.io/github/forks/leissa/sql)](https://github.com/leissa/sql/fork)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue?style=flat-square&logo=cplusplus)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![FE](https://img.shields.io/badge/built%20with-FE-blue?style=flat-square&logo=gitbook&logoColor=white)](https://github.com/leissa/fe)
[![License](https://img.shields.io/github/license/leissa/sql?style=flat-square&color=blue&logo=opensourceinitiative&logoColor=white&label=License)](https://github.com/leissa/sql/blob/main/LICENSE)

[![Linux](https://img.shields.io/github/actions/workflow/status/leissa/sql/linux.yml?style=flat-square&logo=linux&label=Linux&logoColor=white&branch=main)](https://github.com/leissa/sql/actions/workflows/linux.yml?query=branch%3Amain)
[![macOS](https://img.shields.io/github/actions/workflow/status/leissa/sql/macos.yml?style=flat-square&logo=apple&label=macOS&branch=main)](https://github.com/leissa/sql/actions/workflows/macos.yml?query=branch%3Amain)
[![Windows](https://img.shields.io/github/actions/workflow/status/leissa/sql/windows.yml?style=flat-square&label=⊞%20Windows&branch=main)](https://github.com/leissa/sql/actions/workflows/windows.yml?query=branch%3Amain)
[![Format](https://img.shields.io/github/actions/workflow/status/leissa/sql/format.yml?style=flat-square&logo=clang&logoColor=white&label=Format&branch=main)](https://github.com/leissa/sql/actions/workflows/format.yml?query=branch%3Amain)

A small SQL parser, handwritten on top of [**FE**](https://github.com/leissa/fe).

It lexes and parses a substantial subset of SQL into an arena-allocated AST, and can print that AST
back out as SQL.
The [grammar](#-grammar) below spells out exactly which subset.
Diagnostics carry precise `path:row:col` locations, and the parser recovers rather than giving up on
the first error.

## 💡 Why?

This is a compact, readable example of a handwritten recursive-descent frontend:

- a UTF-8-aware lexer with a keyword table and one character of lookahead,
- a precedence-climbing expression parser with two tokens of lookahead,
- an arena-allocated AST that owns its nodes and streams itself back to SQL,
- a black-box test suite that holds the parser and the printer to each other.

It is deliberately small enough to read in one sitting.

## 🧭 Design: Parse Loosely, Check Later

SQL as standardized has a great many idiosyncrasies, and real-world SQL cheerfully ignores a good
number of them.
Rather than encoding every restriction in the grammar, this parser accepts a deliberately wider
language and leaves the rest to a later check over the AST:

- **Reserved words are accepted as identifiers.** The standard reserves several hundred words, far
  more than any real dialect. `SELECT ... AS character` and `FROM aka_title AS at` both parse, as
  does a reference qualified by a reserved word, like `at.movie_id`. The exception is a name
  standing entirely on its own - a lone reference or a type name - where a reserved word would be
  indistinguishable from the clause it starts, so `FROM "table"` has to keep its quotes. The printer
  knows those three places and quotes there, and only there.
- **Statements are expressions.** `Create`, `Select`, `Insert` and friends all derive from `Expr`, so
  a subquery needs no separate node hierarchy. The *grammar*, though, keeps them apart: a statement
  is a schema, data, or transaction statement or a query expression, and a query expression starts
  with `SELECT`, `VALUES`, `TABLE`, `WITH`, or a parenthesis. `1 + 2;` is a fine expression but no
  statement, and nothing can hang an `ORDER BY` off a `CREATE TABLE`.
- **Grouping is not a node.** Parentheses around a scalar expression are pure grouping and are
  dropped; around a query they are structural and are kept, because that is what makes it a subquery.
- **Non-reserved words are recognized by Sym.** `LIMIT`, `CASCADE`, `NULLS`, `VIEW` and the like lex
  as plain identifiers and only mean something in the one place that looks for them, so
  `SELECT limit FROM view` still parses as a query over a table.

The upshot is that some things parse that a conforming implementation would reject.
That is intentional: it keeps the grammar small, and a checking pass has the whole AST to work with.

The trailing `;`, on the other hand, is *not* optional - `<direct SQL statement>` ends in one, and
saying so gives a better diagnostic than running off the end of the file.

## 📜 Grammar

The grammar below is the one this parser actually implements - not the standard's, which is both
larger and stricter.
It is complete: every construct the parser accepts has a production here.
It is deliberately *ambiguous*: `expr ::= expr '+' expr | expr '*' expr` says which operators exist,
and the [precedence table](#precedence) below says how to read them.
Spelling the levels out as a chain of nonterminals would say the same thing far less legibly, and
the parser does not work that way either - it climbs precedences.

Notation: `[ x ]` is optional, `{ x }` repeats zero or more times, `|` separates alternatives, and
`( x )` groups.
`UPPERCASE` is a keyword, `'x'` is punctuation, and `(* ... *)` is a remark.

### Lexical

```ebnf
ident    ::= ( letter | '_' ) { letter | digit | '_' }  (* folded to lower case *)
           | '"' { char } '"'               (* delimited: keeps its case, "" escapes one " *)
name     ::= ident | keyword                (* a reserved word is a fine name - see above *)
qname    ::= name { '.' name }              (* t, s.t, cat.sch.tab *)
col-list ::= '(' name { ',' name } ')'

integer  ::= digit { digit }
real     ::= [ digit { digit } ] '.' digit { digit } [ exponent ] | digit { digit } exponent
string   ::= "'" { char } "'"               (* '' escapes one ' *)
param    ::= '?' | '$' integer | ':' ident
```

Keywords are case insensitive, being folded the same way.
Comments are `-- to end of line` and `/* ... */`.

### Statements

```ebnf
prog ::= { [ stmt ] ';' }                          (* a stray ';' is an empty statement *)

stmt ::= create | alter | drop | truncate | insert | update | delete | transact | query
```

A statement is *not* a value expression: `1 + 2;` parses as an expression but is no statement.

```ebnf
(* OR REPLACE and TEMPORARY are read before it is clear what is being created, so every
   branch accepts them - only the ones they belong to will survive a later check. *)
create ::= CREATE [ OR REPLACE ] [ GLOBAL | LOCAL ] [ TEMPORARY ]
           ( TABLE [ IF NOT EXISTS ] qname ( '(' table-elem { ',' table-elem } ')' | AS query )
           | VIEW qname [ col-list ] AS query [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
           | [ UNIQUE ] INDEX [ IF NOT EXISTS ] name ON qname '(' order { ',' order } ')'
           | SCHEMA [ IF NOT EXISTS ] qname )

table-elem ::= col-def | constraint
col-def    ::= name type { constraint }

constraint ::= [ CONSTRAINT name ]
               ( PRIMARY [ KEY ] [ col-list ]      (* the column list only at table level *)
               | UNIQUE [ col-list ]
               | FOREIGN [ KEY ] col-list REFERENCES qname [ col-list ] { ref-action }
               | REFERENCES qname [ col-list ] { ref-action }
               | CHECK '(' expr ')'
               | DEFAULT expr )
ref-action ::= ON ( DELETE | UPDATE )
               ( NO ACTION | RESTRICT | CASCADE | SET NULL | SET DEFAULT )

alter ::= ALTER TABLE qname
          ( ADD constraint
          | ADD [ COLUMN ] col-def
          | DROP CONSTRAINT name [ behavior ]
          | DROP [ COLUMN ] name [ behavior ]
          | ALTER [ COLUMN ] name ( SET DEFAULT expr | SET NOT NULL | SET DATA TYPE type
                                  | DROP DEFAULT | DROP NOT NULL )
          | RENAME TO name
          | RENAME [ COLUMN ] name TO name )

drop     ::= DROP ( TABLE | VIEW | INDEX | SCHEMA ) [ IF EXISTS ] qname [ behavior ]
truncate ::= TRUNCATE TABLE qname
behavior ::= CASCADE | RESTRICT

insert ::= INSERT INTO qname [ col-list ] ( query | DEFAULT VALUES )
update ::= UPDATE qname [ AS name ] SET assign { ',' assign } [ WHERE expr ]
assign ::= qname '=' expr
delete ::= DELETE FROM qname [ AS name ] [ WHERE expr ]

transact ::= ( BEGIN | START ) [ noise ]
           | COMMIT [ noise ]
           | ROLLBACK [ noise ] [ TO [ SAVEPOINT ] name ]
           | SAVEPOINT name
           | RELEASE [ SAVEPOINT ] name
noise    ::= TRANSACTION | WORK
```

### Query expressions

```ebnf
query ::= [ WITH [ RECURSIVE ] cte { ',' cte } ]
          body
          [ ORDER BY order { ',' order } ]
          { offset | fetch | limit }                (* in any order, each at most once *)
          { lock }

cte    ::= name [ col-list ] AS '(' query ')'
offset ::= OFFSET expr [ ROW | ROWS ]
fetch  ::= FETCH [ FIRST | NEXT ] expr [ ROW | ROWS ] ONLY
limit  ::= LIMIT expr

body ::= body ( UNION | EXCEPT | INTERSECT ) [ ALL | DISTINCT ] body
       | select | values | TABLE qname | '(' query { ',' query } ')'

select ::= SELECT [ ALL | DISTINCT ] ( '*' | select-elem { ',' select-elem } )
           [ FROM table-ref { ',' table-ref } ]
           [ WHERE expr ]
           [ GROUP BY group-elem { ',' group-elem } ]
           [ HAVING expr ]
           [ WINDOW name AS window { ',' name AS window } ]

select-elem ::= expr [ AS ( name | col-list ) | ident ]   (* without AS only a plain ident aliases *)
group-elem  ::= ( ROLLUP | CUBE | GROUPING SETS ) '(' group-elem { ',' group-elem } ')'
              | '(' ')'                                   (* the empty grouping set *)
              | expr

values ::= VALUES row { ',' row }
row    ::= '(' expr { ',' expr } ')' | expr               (* a one-column row needs no parentheses *)

order ::= expr [ ASC | DESC ] [ NULLS ( FIRST | LAST ) ]

lock ::= FOR ( UPDATE | NO KEY UPDATE | SHARE | KEY SHARE )
         [ OF qname { ',' qname } ] [ NOWAIT | SKIP LOCKED ]
```

### Table references

A correlation name binds to one *factor*, which is what makes `a AS x JOIN b AS y ON ...` come out
right; a parenthesized table reference takes none of its own.

```ebnf
table-ref ::= table-ref join-op table-ref [ ON expr | USING col-list ]
            | table-factor
join-op   ::= [ NATURAL ] [ INNER | ( LEFT | RIGHT | FULL ) [ OUTER ] ] JOIN
            | CROSS JOIN

table-factor  ::= [ LATERAL ] table-primary [ WITH ORDINALITY ] [ correlation ]
correlation   ::= ( AS name | ident ) [ col-list ]
table-primary ::= '(' table-ref ')'                (* a '(' that opens no query merely groups *)
                | expr                             (* in practice a qname, a call, or '(' query ')' *)
```

### Value expressions

```ebnf
expr ::= expr ( '+' | '-' | '*' | '/' | '%' ) expr
       | expr '||' expr
       | expr ( '=' | '<>' | '!=' | '<' | '<=' | '>' | '>=' ) expr
       | expr ( '=' | '<>' | '!=' | '<' | '<=' | '>' | '>=' ) ( ALL | ANY | SOME ) '(' query ')'
       | expr AND expr
       | expr OR expr
       | expr IS [ NOT ] expr
       | expr IS [ NOT ] DISTINCT FROM expr
       | expr [ NOT ] IN expr
       | expr [ NOT ] BETWEEN expr AND expr
       | expr [ NOT ] ( LIKE | ILIKE | SIMILAR TO ) expr [ ESCAPE expr ]
       | expr COLLATE qname
       | ( '+' | '-' | NOT | EXISTS ) expr
       | primary

primary ::= literal
          | ref { '[' expr ']' }                   (* array subscripts *)
          | CASE [ expr ] { WHEN expr THEN expr } [ ELSE expr ] END
          | CAST '(' expr AS type ')'
          | special-func
          | '(' query { ',' query } ')'            (* a subquery, or a row: '(' a ',' b ')' *)

ref  ::= qname [ '.' '*' ] | call
call ::= qname '(' [ DISTINCT | ALL ] [ expr { ',' expr } ] ')'
         [ WITHIN GROUP '(' ORDER BY order { ',' order } ')' ]
         [ FILTER '(' WHERE expr ')' ]
         [ OVER window ]

special-func ::= EXTRACT '(' name FROM expr ')'
               | SUBSTRING '(' expr FROM expr [ FOR expr ] ')'
               | TRIM '(' [ LEADING | TRAILING | BOTH ] [ expr FROM | FROM ] expr ')'
               | POSITION '(' expr IN expr ')'
               | OVERLAY '(' expr PLACING expr FROM expr [ FOR expr ] ')'

literal ::= integer | real | string | param
          | TRUE | FALSE | UNKNOWN | NULL | DEFAULT
          | '*'                                    (* what COUNT(*) counts *)
          | ( DATE | TIME | TIMESTAMP | INTERVAL ) string [ interval-qual ]

window ::= name                                    (* one named in the WINDOW clause *)
         | '(' [ ident ] [ PARTITION BY expr { ',' expr } ]
                         [ ORDER BY order { ',' order } ] [ frame ] ')'
frame  ::= ( ROWS | RANGE | GROUPS ) ( bound | BETWEEN bound AND bound )
           [ EXCLUDE ( CURRENT ROW | GROUP | TIES | NO OTHERS ) ]
bound  ::= UNBOUNDED ( PRECEDING | FOLLOWING ) | CURRENT ROW | expr ( PRECEDING | FOLLOWING )
```

### Types

```ebnf
type ::= simple-type | name-type
name-type   ::= ident [ '(' expr { ',' expr } ')' ] [ [ NOT ] NULL ]
simple-type ::= ( INTEGER | INT | SMALLINT | BIGINT | BOOLEAN | DATE | REAL | FLOAT
                | DOUBLE [ PRECISION ] | TIME | TIMESTAMP | INTERVAL | NUMERIC | DECIMAL | DEC
                | CHAR | CHARACTER | VARCHAR | BINARY | VARBINARY | BLOB | CLOB )
                [ LARGE OBJECT ] [ VARYING ] [ '(' expr { ',' expr } ')' ]
                [ interval-qual ] [ ( WITH | WITHOUT ) TIME ZONE ] [ [ NOT ] NULL ]

interval-qual ::= field [ '(' expr ')' ] [ TO field [ '(' expr ')' ] ]
field         ::= YEAR | MONTH | DAY | HOUR | MINUTE | SECOND
```

### Precedence

Loosest first; every binary operator is left-associative.
A `NOT` may precede any binary operator and ranks with it, so `a NOT LIKE b` and `a NOT IN c` need
no rules of their own.

| Precedence | Operators |
| --- | --- |
| `OR` | `a OR b` |
| `AND` | `a AND b` |
| `BETWEEN` | `a BETWEEN lo AND hi` |
| `NOT` | prefix `NOT a` |
| comparison | `= <> != < <= > >=`, `IS`, `IS DISTINCT FROM`, `IN`, `LIKE`, `ILIKE`, `SIMILAR TO` |
| concatenation | `a \|\| b` |
| additive | `a + b`, `a - b` |
| multiplicative | `a * b`, `a / b`, `a % b` |
| unary | prefix `+a`, `-a`, `EXISTS a`, postfix `a COLLATE c` |
| subscript | `a[i]` |

The bounds of a `BETWEEN` parse above `NOT`, so its own `AND` ends the lower bound instead of being
swallowed as a conjunction.
Subscripts bind tighter than the prefix operators: `-a[1]` negates the element, not the array.

Two more chains sit outside the expression grammar.
`INTERSECT` binds tighter than `UNION` and `EXCEPT`, and both chains are left-associative.
`JOIN` chains are left-associative too, so `a JOIN b JOIN c` reads as `(a JOIN b) JOIN c` and only a
right-nested join needs parentheses.

## 🚀 Building

If you have a [GitHub account setup with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), just do this:
```sh
git clone --recurse-submodules git@github.com:leissa/sql.git
```
Otherwise, clone via HTTPS:
```sh
git clone --recurse-submodules https://github.com/leissa/sql.git
```
Then, build with:
```sh
cd sql
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j $(nproc)
```
For a `Release` build simply use `-DCMAKE_BUILD_TYPE=Release`.

This needs a C++23 compiler.
Abseil and FE come along as submodules; nothing else is required.

## 🔧 Usage

```sh
./build/bin/sql -d test/parse/select.sql   # parse and dump the AST back as SQL
./build/bin/sql --help                     # list all options
echo 'SELECT * FROM t;' | ./build/bin/sql -d -
```

Use `-` as the file to read from stdin.
Diagnostics go to stderr and the exit status is non-zero if anything was rejected:

```
$ ./build/bin/sql test/error/missing_from.sql
test/error/missing_from.sql:1:10: error: expected 'FROM', got 't' while parsing SELECT expression
1 error(s) encountered
```

## 🛠️ Testing

The test suite is *black box*: every test runs the `sql` binary and inspects only its exit code, its
dump, and its diagnostics.
Nothing links against the parser.

```sh
ctest --test-dir build --output-on-failure
```

There are four kinds of test, one CTest entry per fixture:

| Test | Fixtures | Asserts |
| --- | --- | --- |
| `parse/parse/<name>` | `test/parse/` | Parses cleanly, and the dump matches the neighboring `.out` golden. |
| `error/error/<name>` | `test/error/` | Is rejected, with the diagnostics matching the neighboring `.out` golden. |
| `reject/reject/<name>` | `test/reject/` | Every query in the corpus, one per line, is rejected. |
| `idempotent/...` | `test/parse/`, `test/job/`, `test/tpch/` | Dumping a dump reproduces it verbatim. |

That last one is the interesting one: it holds the printer and the parser to each other, since
whatever the printer emits, the parser has to read back into the very same AST.
It runs over the curated fixtures and over two real-world corpora that get no goldens of their own:
`test/job/`, the [Join Order Benchmark](https://github.com/gregrahn/join-order-benchmark) - 113
queries plus their schema - and `test/tpch/`, the 22 TPC-H queries.

The corpora under `test/tpch/`, `test/reject/`, and `test/parse/hyrise.sql` come from the
[hyrise/sql-parser](https://github.com/hyrise/sql-parser) test suite; each file says in its header
what was adapted and what was left out.

To run a single test, or one group:
```sh
ctest --test-dir build -R '^parse/parse/expr$' --output-on-failure
ctest --test-dir build -R '^idempotent/job/' --output-on-failure
```

### Benchmarking

`test/bench/` times the parser over a corpus of `.sql` files. It is no CTest entry - a benchmark is
not a pass/fail test - so build it on demand:

```sh
cmake --build build --target bench
./build/bin/bench test/job/*.sql            # a Driver and a Parser per file
./build/bin/bench --once test/job/*.sql     # the whole corpus through a single Parser
./build/bin/bench --lex test/job/*.sql      # lexing alone, with nothing built on top
```

The fixtures are small - a few hundred bytes each - so a run over them measures the per-statement
overheads more than anything else. For a corpus where the arena, the SymPool, and the lexer's
buffers get to amortize, generate one:

```sh
test/bench/gen.py --mb 64 -o /tmp/big.sql   # names out of the JOB and TPC-H vocabulary, reused
test/bench/gen.py --mb 64 --stress-names -o /tmp/big.sql   # every identifier distinct instead
./build/bin/bench --once /tmp/big.sql
```

The two modes answer different questions. `--each` is what an embedding that parses one query at a
time pays, setup included; `--once` pays the setup once and leaves parsing throughput. What is left
between them is registering each source and constructing a Parser - both O(1), since the few hundred
reserved and non-reserved words are interned once per Driver rather than once per Parser.

#### Against hyrise/sql-parser

[hyrise/sql-parser](https://github.com/hyrise/sql-parser) makes a fair yardstick: a bison/flex parser
of comparable scope, and the source of several of the corpora above.
Both built `Release` and pinned to one 5.15 GHz Zen 5 core of a Ryzen AI 9 HX PRO 370, with
`hyperfine` for the wall clock and `perf stat -e instructions` for a figure that does not drift
between runs:

- **Throughput** is *more is better*.
- **Instructions** is *fewer is better*.
- The **winner** of each pair is in **bold**.

<table>
  <thead>
    <tr>
      <th rowspan="2">Corpus</th>
      <th rowspan="2">Mode</th>
      <th colspan="2">Throughput (MB/s) </th>
      <th colspan="2">Instructions (G) </th>
    </tr>
    <tr>
      <th>Ours</th>
      <th>Hyrise</th>
      <th>Ours</th>
      <th>Hyrise</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>JOB, 113 queries</td>
      <td><code>--each</code></td>
      <td><strong>103.7</strong></td>
      <td>59.5</td>
      <td><strong>0.59</strong></td>
      <td>1.04</td>
    </tr>
    <tr>
      <td>JOB</td>
      <td><code>--once</code></td>
      <td><strong>157.8</strong></td>
      <td>68.1</td>
      <td><strong>0.48</strong></td>
      <td>1.05</td>
    </tr>
    <tr>
      <td>TPC-H, 22 queries</td>
      <td><code>--each</code></td>
      <td><strong>67.8</strong></td>
      <td>50.1</td>
      <td><strong>0.21</strong></td>
      <td>0.28</td>
    </tr>
    <tr>
      <td>TPC-H</td>
      <td><code>--once</code></td>
      <td><strong>114.1</strong></td>
      <td>59.3</td>
      <td><strong>0.16</strong></td>
      <td>0.29</td>
    </tr>
    <tr>
      <td>generated, 32 MiB</td>
      <td><code>--once</code></td>
      <td><strong>111.3</strong></td>
      <td>43.9</td>
      <td><strong>13.4</strong></td>
      <td>26.0</td>
    </tr>
    <tr>
      <td>generated, 256 MiB</td>
      <td><code>--once</code></td>
      <td><strong>113.9</strong></td>
      <td>43.1</td>
      <td><strong>39.8</strong></td>
      <td>69.6</td>
    </tr>
  </tbody>
</table>
Lexing alone, against their flex scanner: 290.7 MB/s to 147.2 on JOB, and 204.5 to 114.7 on the
32 MiB corpus.
Peak [resident set size](https://en.wikipedia.org/wiki/Resident_set_size) - the RAM a process has
actually touched, at its high-water mark - over 500k times `SELECT a FROM t;` is 146 MiB against
their 284, or 306 bytes per statement to their 596.
That is the number to watch for an embedding, because the AST *is* the output and is held for as long
as the caller needs it.

Two things worth reading off that table.
Throughput does not fall off as the corpus grows, because the per-statement footprint is small enough
that the working set does not grow with it either.
And the margin is narrowest in `--each`, where registering each source and hashing its path is a
larger share of the work than parsing - that, rather than anything in the parser, is what the two
modes still differ by.

The two do not do quite the same work per byte, in both directions: their scanner recognizes keywords
inside the DFA, where this one interns and looks up every word, but it is also byte-oriented and
never decodes UTF-8, where this one decodes and validates every code point.

After deliberately changing what the parser accepts or how it prints, regenerate the goldens and
review the resulting diff:
```sh
cmake --build build --target bless
```

## 🤝 Coding Style

Use the following coding conventions:
* class/type names in `CamelCase`
* constants as defined in an `enum` or via `static const` in `Camel_Snake_Case`
* macro names in `SNAKE_IN_ALL_CAPS`
* everything else like variables, functions, etc. in `snake_case`
* use a trailing underscore suffix for a `private_or_protected_member_variable_`
* don't do that for a `public_member_variable`
* use `struct` for [plain old data](https://en.cppreference.com/w/cpp/named_req/PODType)
* use `class` for everything else
* visibility groups in this order:
    1. `public`
    2. `protected`
    3. `private`
* prefer `// C++-style comments` over `/* C-style comments */`
* use `/// three slashes for Doxygen` and [group](https://www.doxygen.nl/manual/grouping.html) your methods into logical units if possible
* use [Markdown-style](https://doxygen.nl/manual/markdown.html) Doxygen comments
* methods/functions that return a `bool` should be prefixed with `is_`
* methods/functions that return a `std::optional` or a pointer that may be `nullptr` should be prefixed with `isa_`

For all the other minute details like indentation width etc. use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and the provided `.clang-format` file in the root of the repository.
The `format` workflow checks this on every push:
```sh
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
```
In order to run `clang-format` automatically on all changed files, switch to the provided pre-commit hook:
```sh
git config --local core.hooksPath .githooks/
```
Note that you can [disable clang-format for a piece of code](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code).
In addition, you might want to check out plugins like the [Vim integration](https://clang.llvm.org/docs/ClangFormat.html#vim-integration).

## ⚖️ License

SQL is licensed under the [MIT License](LICENSE).
