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
Diagnostics carry precise `path:row:col` locations, and the parser recovers rather than giving up on
the first error.

## 💡 Why?

This is a compact, readable example of a handwritten recursive-descent frontend:

- a UTF-8-aware lexer with a keyword table and one character of lookahead,
- a precedence-climbing expression parser with two tokens of lookahead,
- an arena-allocated AST that owns its nodes and streams itself back to SQL,
- a black-box test suite that holds the parser and the printer to each other.

It is deliberately small enough to read in one sitting.

## ✨ What It Parses

**Statements**

A program is a `;`-separated list of *statements* - not of arbitrary expressions.
A stray `;` is an empty statement and is skipped.

- `CREATE TABLE` - column definitions, and column- and table-level constraints:
  `NOT NULL`, `PRIMARY KEY`, `UNIQUE`, `CHECK`, `DEFAULT`, `REFERENCES`, `FOREIGN KEY`, and named
  `CONSTRAINT`s, with `ON DELETE`/`ON UPDATE` referential actions.
  Also `[GLOBAL|LOCAL] TEMPORARY`, `IF NOT EXISTS`, and `CREATE TABLE ... AS <query>`.
- `CREATE [OR REPLACE] VIEW` with an optional column list and `WITH [CASCADED|LOCAL] CHECK OPTION`,
  `CREATE [UNIQUE] INDEX`, and `CREATE SCHEMA`.
- `ALTER TABLE` - `ADD`/`DROP` a column or a constraint, `ALTER COLUMN` to set or drop a `DEFAULT`,
  a `NOT NULL`, or the data type, and `RENAME` the table or a column.
- `DROP TABLE|VIEW|INDEX|SCHEMA`, with `IF EXISTS` and `CASCADE`/`RESTRICT`; `TRUNCATE TABLE`.
- `SELECT` - `ALL`/`DISTINCT`, aliases with and without `AS`, `WHERE`, `GROUP BY`, `HAVING`,
  `WINDOW`. The `FROM` clause is optional, so `SELECT 1` parses.
- `INSERT INTO` - from a `VALUES` table, from a query, or `DEFAULT VALUES`.
- `UPDATE` / `DELETE` - with an optional correlation name and `WHERE` clause.
- Transaction control: `START TRANSACTION` / `BEGIN`, `COMMIT`, `ROLLBACK [TO SAVEPOINT ...]`,
  `SAVEPOINT`, and `RELEASE SAVEPOINT`.
- Names are qualified wherever a table is named: `s.t`, `cat.sch.tab`.

**Query expressions**

- `WITH [RECURSIVE]` common table expressions, each with an optional column list.
- `UNION`, `INTERSECT`, and `EXCEPT`, each with `ALL`/`DISTINCT`.
  `INTERSECT` binds tighter, and both chains are left-associative.
- A `VALUES` table and the explicit `TABLE <name>` stand on their own as queries.
- `ORDER BY` with `ASC`/`DESC` and `NULLS FIRST`/`NULLS LAST`, plus `OFFSET`, `FETCH`, and `LIMIT`
  in any order and combination.
- Row locking: `FOR UPDATE`, `FOR NO KEY UPDATE`, `FOR SHARE`, and `FOR KEY SHARE`, each with an
  optional `OF <tables>` and a trailing `NOWAIT` or `SKIP LOCKED`.
- `GROUP BY` elements beyond a plain expression: `ROLLUP`, `CUBE`, `GROUPING SETS`, and the empty
  grouping set `()`.
- Subqueries anywhere an expression is allowed, including derived tables in `FROM`, `LATERAL` ones,
  and `UNNEST(...) WITH ORDINALITY`.

**Joins**

- `INNER`, `LEFT`, `RIGHT`, and `FULL` (with optional `OUTER`), plus `CROSS` and `NATURAL`.
- `ON <condition>` and `USING (<columns>)`, in arbitrarily long chains.
- A correlation name binds to one table reference, so both sides of a join can carry their own:
  `a AS x JOIN b AS y ON x.id = y.id`. Parentheses around a table reference merely group.

**Value expressions**

- The usual arithmetic, comparison, and boolean operators, correctly ranked and left-associative,
  plus `||` concatenation and `%`.
- `IS [NOT]`, `IS [NOT] DISTINCT FROM`, `[NOT] IN`, `[NOT] BETWEEN`, and `EXISTS`.
- `[NOT] LIKE`, `[NOT] ILIKE`, and `[NOT] SIMILAR TO`, each with an optional `ESCAPE`.
- Quantified comparisons: `a = ANY (...)`, `a > ALL (...)`, `a <> SOME (...)`.
- `CASE` in both the simple and the searched form, `CAST(... AS <type>)`, and `... COLLATE <name>`.
- Function and aggregate calls, including `COUNT(*)` and `COUNT(DISTINCT x)`, with the trailing
  `WITHIN GROUP (ORDER BY ...)`, `FILTER (WHERE ...)`, and `OVER` clauses.
- Window specifications: `PARTITION BY`, `ORDER BY`, a `ROWS`/`RANGE`/`GROUPS` frame with
  `BETWEEN ... AND ...` and `EXCLUDE`, and references to a window named in the `WINDOW` clause.
- The functions the standard spells with keyword-separated arguments: `EXTRACT(f FROM x)`,
  `SUBSTRING(x FROM a FOR b)`, `TRIM([BOTH] c FROM x)`, `POSITION(a IN b)`,
  `OVERLAY(x PLACING y FROM a FOR b)`.
- Qualified references such as `t.a` and `t.*`, and qualified calls such as `s.f(x)`.
- Array subscripts, `a[i]` and `a[i][j]`, binding tighter than any operator.

**Types**

- `INTEGER`, `INT`, `SMALLINT`, `BIGINT`, `BOOLEAN`, `DATE`, `REAL`, `DOUBLE PRECISION`, `FLOAT`,
  `TIME`, `TIMESTAMP`, `INTERVAL`, `NUMERIC`, `DECIMAL`, `DEC`, `CHAR`, `CHARACTER [VARYING]`,
  `VARCHAR`, `BINARY`, `VARBINARY`, `BLOB`, `CLOB` - with length and precision arguments.
- `[WITHOUT] TIME ZONE`, and an interval qualifier such as `INTERVAL DAY(3) TO SECOND(6)`.
- Any identifier is accepted as a type name too, so vendor types like `text` or `uuid` just work.

**Lexical**

- Keywords are case insensitive and unquoted identifiers fold to lower case.
- Double-quoted delimited identifiers keep their case; a doubled `"` escapes one.
- Single-quoted string literals, where a doubled `'` escapes one.
- Integer literals, and real ones with a fraction and/or an exponent: `1.5`, `.5`, `2.5E-3`.
- Typed literals: `DATE '...'`, `TIME '...'`, `TIMESTAMP '...'`, `INTERVAL '1-2' YEAR TO MONTH`.
- Dynamic parameter markers in all three spellings: `?`, `$1`, and `:name`.
- `--` line comments and `/* ... */` block comments.

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
It is deliberately *ambiguous*: `expr ::= expr '+' expr | expr '*' expr` says which operators exist,
and the [precedence table](#precedence) below says how to read them.
Spelling the levels out as a chain of nonterminals would say the same thing far less legibly, and
the parser does not work that way either - it climbs precedences.

Notation: `[ x ]` is optional, `{ x }` repeats zero or more times, `|` separates alternatives, and
`( x )` groups.
`UPPERCASE` is a keyword, `'x'` is punctuation, and `(* ... *)` is a remark.

### Lexical

```ebnf
ident   ::= ( letter | '_' ) { letter | digit | '_' }   (* folded to lower case *)
          | '"' { char } '"'                       (* delimited: keeps its case, "" escapes one " *)
name    ::= ident | keyword                        (* a reserved word is a fine name - see above *)
qname   ::= name { '.' name }                      (* t, s.t, cat.sch.tab *)
col-list ::= '(' name { ',' name } ')'

integer ::= digit { digit }
real    ::= [ digit { digit } ] '.' digit { digit } [ exponent ] | digit { digit } exponent
string  ::= "'" { char } "'"                       (* '' escapes one ' *)
param   ::= '?' | '$' integer | ':' ident
```

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
