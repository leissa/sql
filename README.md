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

A fast SQL parser, handwritten in C++23 on top of [**FE**](https://leissa.github.io/fe/).

It lexes and parses a substantial subset of SQL into an arena-allocated AST, and can print that AST
back out as SQL.
The [grammar](GRAMMAR.md) spells out exactly which subset.
Diagnostics carry precise `path:row:col` locations, and the parser recovers rather than giving up on
the first error.

It sustains 95-185 MB/s - 1.3x to 2.8x [hyrise/sql-parser](https://github.com/hyrise/sql-parser), a
bison/flex parser of comparable scope - at roughly half its instructions per byte and half its
memory per statement; the [numbers](#-performance) are below, with the corpora and the method to
reproduce them.
Every commit parses the Join Order Benchmark, TPC-H, and the SQL [hyrise](https://github.com/hyrise/hyrise)
itself runs, and round-trips all of it through the printer.

Just want to parse SQL in your own C++ project?
Jump to [Using It as a Library](#-using-it-as-a-library).

## 💡 Why This One

- **It is quick.** A hand-rolled UTF-8 lexer and a precedence-climbing parser over an arena, with the
  few hundred reserved and non-reserved words interned once per `Driver`, so constructing a `Parser`
  is O(1) and parsing one query costs no setup worth the name. See [Performance](#-performance).
- **It round-trips.** Whatever the printer emits, the parser reads back into the same AST - enforced
  on every commit over 500 real-world queries, not just over curated fixtures.
- **It survives real SQL.** Reserved words show up as identifiers everywhere in the wild, and this
  parser takes them (see [Design](#-design-parse-loosely-check-later)) instead of rejecting queries
  that every database accepts.
- **It embeds cleanly.** One header, one call, `find_package(sql)`, and an AST whose lifetime is a
  single object you keep. No globals, no build-system archaeology.
- **It is readable.** ~5.5k lines for lexer, parser, 64 AST classes and printer - a handwritten
  recursive-descent frontend you can actually follow, and change.

## ⚡ Performance

[hyrise/sql-parser](https://github.com/hyrise/sql-parser) makes a fair yardstick: a bison/flex parser
of comparable scope, and the source of several of the corpora this parser is tested on.
Both built `Release` with the same compiler and pinned to one 5.15 GHz Zen 5 core of a Ryzen AI 9
HX PRO 370, the machine otherwise idle.
The wall clock is the best of five in-process timings over a corpus read up front, so no file system
is in it; `perf stat -e instructions` gives the figure that does not drift between runs, normalized
per input byte so that it does not depend on an iteration count either.
On every corpus below both parsers accept every file and report the same number of statements, so
they really are handed the same work.

- **Throughput** is *more is better*.
- **Instructions per byte** is *fewer is better*.
- The **winner** of each pair is in **bold**.

<table>
  <thead>
    <tr>
      <th rowspan="2">Corpus</th>
      <th rowspan="2">Mode</th>
      <th colspan="2">Throughput (MB/s) </th>
      <th colspan="2">Instructions / byte </th>
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
      <td><strong>116.6</strong></td>
      <td>66.6</td>
      <td><strong>132.5</strong></td>
      <td>253.1</td>
    </tr>
    <tr>
      <td>JOB</td>
      <td><code>--once</code></td>
      <td><strong>183.2</strong></td>
      <td>74.3</td>
      <td><strong>105.4</strong></td>
      <td>255.5</td>
    </tr>
    <tr>
      <td>TPC-H, 22 queries</td>
      <td><code>--each</code></td>
      <td><strong>94.6</strong></td>
      <td>71.6</td>
      <td><strong>144.4</strong></td>
      <td>209.2</td>
    </tr>
    <tr>
      <td>TPC-H</td>
      <td><code>--once</code></td>
      <td><strong>165.5</strong></td>
      <td>82.7</td>
      <td><strong>107.6</strong></td>
      <td>214.4</td>
    </tr>
    <tr>
      <td>generated, 32 MiB</td>
      <td><code>--once</code></td>
      <td><strong>120.9</strong></td>
      <td>46.3</td>
      <td><strong>125.1</strong></td>
      <td>257.6</td>
    </tr>
    <tr>
      <td>generated, 256 MiB</td>
      <td><code>--once</code></td>
      <td><strong>116.4</strong></td>
      <td>41.5</td>
      <td><strong>140.8</strong></td>
      <td>259.4</td>
    </tr>
    <tr>
      <td>generated, 32 MiB, <code>--stress-names</code></td>
      <td><code>--once</code></td>
      <td><strong>109.8</strong></td>
      <td>66.1</td>
      <td><strong>99.4</strong></td>
      <td>173.3</td>
    </tr>
  </tbody>
</table>

Lexing alone, against their flex scanner: 341.9 MB/s to 176.9 on JOB, and 228.9 to 136.8 on the
32 MiB corpus.
That lead is won on instructions per cycle rather than on instruction count - their scanner retires
a comparable number of instructions per byte, in fact fewer on three of the five corpora, yet
wherever this one leads it does so at an IPC of 3.4 to 4.6 against their 2.5 to 3.2, a flex table
walk being a chain of dependent loads the machine cannot run ahead of.

Peak [resident set size](https://en.wikipedia.org/wiki/Resident_set_size) - the RAM a process has
actually touched, at its high-water mark - over 500k times `SELECT a FROM t;` is 127 MiB against
their 285, or 267 bytes per statement to their 598.
That is the number to watch for an embedding, because the AST *is* the output and is held for as long
as the caller needs it.

Three things worth reading off that table.
Throughput does not fall off as the corpus grows, because the per-statement footprint is small enough
that the working set does not grow with it either.
The margin is narrowest in `--each`, where registering each source and hashing its path is a larger
share of the work than parsing - that, rather than anything in the parser, is what the two modes
still differ by.
And `--stress-names` is the worst case a design built on interning can be handed: with no name ever
reused, lexing ties exactly - 162.7 MB/s to 164.0 - and parsing falls from a 2.6x lead to a 1.7x one
over the same corpus with its names reused.

The two do not do quite the same work per byte, in both directions: their scanner recognizes keywords
inside the DFA, where this one interns and looks up every word, but it is also byte-oriented and
never decodes UTF-8, where this one decodes and validates every code point.

### Reproducing It

`test/bench/` times the parser over a corpus of `.sql` files. It is no CTest entry - a benchmark is
not a pass/fail test - so build it on demand:

```sh
cmake --build build --target bench
./build/bin/bench --each test/job/*.sql     # a Driver and a Parser per file
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

The first two modes answer different questions. `--each` is what an embedding that parses one query
at a time pays, setup included; `--once` pays the setup once and leaves parsing throughput. What is
left between them is registering each source and constructing a Parser - both O(1), since the few
hundred reserved and non-reserved words are interned once per Driver rather than once per Parser.

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
  `SELECT limit FROM view` still parses as a query over a table. `VALUE` sits here too, against the
  standard, which reserves it: TPC-H Q11 names a column that, and so `ORDER BY value` has to work.

The upshot is that some things parse that a conforming implementation would reject.
That is intentional: it keeps the grammar small, and a checking pass has the whole AST to work with.

The trailing `;`, on the other hand, is *not* optional - `<direct SQL statement>` ends in one, and
saying so gives a better diagnostic than running off the end of the file.

## 📜 Grammar

Every construct the parser accepts has a production in **[GRAMMAR.md](GRAMMAR.md)**, together with
the precedence table that says how to read the ambiguous ones.
It is the grammar this parser implements, not the standard's, which is both larger and stricter.

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
FE comes along as a submodule; nothing else is required.

To install the library, its headers, and its CMake package:

```sh
cmake --install build --prefix /usr/local
```

## 📦 Using It as a Library

Link `sql::sql` - via `find_package` against an install, or by pulling the repository into your build:

```cmake
find_package(sql 0.1 REQUIRED)             # against `cmake --install`
# or: add_subdirectory(sql)                # as a submodule
# or: FetchContent_MakeAvailable(sql)      # straight from the repository

target_link_libraries(my_app PRIVATE sql::sql)
```

One header, one call:

```cpp
#include <sql/sql.h>

auto res = sql::parse("SELECT a, b FROM t WHERE a > 42;");
if (!res) {
    res.report(std::cerr);       // `<input>:1:10: error: ...`, snippet and all
    return EXIT_FAILURE;
}

for (auto stmt : res.stmts())
    if (auto select = stmt->isa<sql::Select>())
        std::cout << select->elems().size() << " selected, " << select->froms().size() << " table refs\n";

std::cout << res << '\n';       // the AST, streamed back out as SQL
```

`sql::parse_file(path)` and `sql::parse(std::istream&)` read from a file or a stream instead; the
former throws `std::filesystem::filesystem_error` if the file cannot be read.
A complete program is in [example/](example/), and it builds on its own:

```sh
cmake -S example -B build-example -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build-example
```

### Lifetime

Every AST node is allocated in an arena and handed out as an `AST<T>` - a *non-owning* pointer.
That arena belongs to the `sql::Result`, which is why the Result is what you keep: the whole AST
dies with it, so returning an `AST<T>` from a function that let its Result go out of scope dangles.
A Result moves freely, though, so returning *it* is fine.

### Walking the AST

Statements are `Expr`s (see [Design](#-design-parse-loosely-check-later)), and every node is an
`fe::RuntimeCast`, so a dynamic check is `isa` and an assertion is `as`:

```cpp
if (auto select = stmt->isa<sql::Select>()) /* ... */;   // nullptr if it is not a Select
auto& select = *stmt->as<sql::Select>();                 // asserts that it is
```

`stream` is `virtual`, so any node - not just a whole `Prog` - streams itself back out as SQL via
`operator<<` or `std::format`.

### Diagnostics

The parser recovers, so a `Result` with errors still carries an AST, just one with holes in it.
`res.errors()` hands out the `fe::Error::Msg`es themselves - each a `Loc`, a tag, the text, and its
notes - while `res.report(os)` streams them the way the command-line tool does and returns how many
errors there were. Nothing is printed unless you ask for it.

For finer control - a `fe::Diag` of your own, or parsing many inputs into a single arena - drop to
`sql::Driver` and `sql::Parser` directly; `sql::parse` is a thin wrapper over exactly that, and
`res.driver()` hands you the one it made.

## 🔧 Command-Line Tool

```sh
./build/bin/sql -d test/parse/select.sql   # parse and dump the AST back as SQL
./build/bin/sql --help                     # list all options
echo 'SELECT * FROM t;' | ./build/bin/sql -d -
```

Use `-` as the file to read from stdin.
Diagnostics go to stderr and the exit status is non-zero if anything was rejected:

```txt
$ ./build/bin/sql test/error/missing_from.sql
test/error/missing_from.sql:1:10: error: expected 'FROM', got 't' while parsing SELECT expression
1 error(s) encountered
```

## 🛠️ Testing

The test suite is *black box*: every test runs the `sql` binary and inspects only its exit code, its
dump, and its diagnostics.
Nothing links against the parser.
CI runs it on Linux, macOS and Windows, and once more under ASan, LSan and UBSan, so a green build
means leak- and UB-clean as well as passing.

```sh
ctest --test-dir build --output-on-failure
```

There are four kinds of test, one CTest entry per fixture:

| Test | Fixtures | Asserts |
| --- | --- | --- |
| `parse/parse/<name>` | `test/parse/` | Parses cleanly, and the dump matches the neighboring `.out` golden. |
| `error/error/<name>` | `test/error/` | Is rejected, with the diagnostics matching the neighboring `.out` golden. |
| `reject/reject/<name>` | `test/reject/` | Every query in the corpus, one per line, is rejected. |
| `idempotent/...` | `test/parse/`, `test/job/`, `test/tpch/`, `test/hyrise/` | Dumping a dump reproduces it verbatim. |

That last one is the interesting one: it holds the printer and the parser to each other, since
whatever the printer emits, the parser has to read back into the very same AST.
It runs over the curated fixtures and over three real-world corpora that get no goldens of their own:
`test/job/`, the [Join Order Benchmark](https://github.com/gregrahn/join-order-benchmark) with its
113 queries plus their schema; `test/tpch/`, the 22 TPC-H queries; and `test/hyrise/`, the SQL the
[hyrise](https://github.com/hyrise/hyrise) database itself runs - the 366 queries of its
SQLiteTestRunner, the Star Schema Benchmark, and the TPC-H and TPC-DS schemas with their indexes.

The corpora under `test/tpch/`, `test/reject/`, and `test/parse/hyrise.sql` come from
[hyrise/sql-parser](https://github.com/hyrise/sql-parser), the parser hyrise vendors, and the ones
under `test/hyrise/` from [hyrise](https://github.com/hyrise/hyrise) itself; each file says in its
header what was adapted and what was left out.

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

- class/type names in `CamelCase`
- constants as defined in an `enum` or via `static const` in `Camel_Snake_Case`
- macro names in `SNAKE_IN_ALL_CAPS`
- everything else like variables, functions, etc. in `snake_case`
- use a trailing underscore suffix for a `private_or_protected_member_variable_`
- don't do that for a `public_member_variable`
- use `struct` for [plain old data](https://en.cppreference.com/w/cpp/named_req/PODType)
- use `class` for everything else
- visibility groups in this order:
    1. `public`
    2. `protected`
    3. `private`
- prefer `// C++-style comments` over `/* C-style comments */`
- use `/// three slashes for Doxygen` and [group](https://www.doxygen.nl/manual/grouping.html) your methods into logical units if possible
- use [Markdown-style](https://doxygen.nl/manual/markdown.html) Doxygen comments
- methods/functions that return a `bool` should be prefixed with `is_`
- methods/functions that return a `std::optional` or a pointer that may be `nullptr` should be prefixed with `isa_`

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
