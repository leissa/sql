# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

A handwritten recursive-descent SQL parser built on [FE](https://github.com/leissa/fe), which is
vendored at `submodules/fe`. `README.md` documents what the parser accepts, the EBNF grammar it
actually implements, and the coding style; read it before changing the grammar or the printer.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # Debug is the default if unset
cmake --build build -j $(nproc)
ctest --test-dir build --output-on-failure
```

Abseil and FE come along as submodules (`git clone --recurse-submodules`); nothing else is needed.
Requires a C++23 compiler.

```sh
ctest --test-dir build -R '^parse/parse/expr$' --output-on-failure   # one test
ctest --test-dir build -R '^idempotent/job/' --output-on-failure     # one group
cmake --build build --target bless                                   # regenerate all goldens
cmake --build build --target bench                                   # build the benchmark
```

CI additionally runs an ASan+LSan+UBSan Debug build (`.github/workflows/linux.yml`). A change is done
when it is leak- and UB-clean, not merely when `ctest` passes.

### Formatting

`clang-format` is enforced by CI over this repository's own sources only:

```sh
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
git config --local core.hooksPath .githooks/   # format staged files automatically
```

## Testing model

The suite is **black box**: `test/run_test.py` runs the `sql` binary and inspects only its exit code,
stdout, and diagnostics. Nothing links against the parser. `test/CMakeLists.txt` registers one CTest
entry per `.sql` fixture, named `<mode>/<dir>/<name>`:

| Mode | Fixtures | Asserts |
| --- | --- | --- |
| `parse` | `test/parse/` | Parses cleanly; dump matches the neighboring `.out` golden. |
| `error` | `test/error/` | Is rejected; diagnostics match the `.out` golden. |
| `reject` | `test/reject/` | Every query in the corpus, one per line, is rejected. No golden. |
| `idempotent` | `test/parse/`, `test/job/`, `test/tpch/` | Dumping a dump reproduces it verbatim. |

`idempotent` is the load-bearing one: it holds printer and parser to each other, since whatever the
printer emits the parser must read back into the same AST. **A change to `stream.cpp` almost always
needs a `bless` and a careful review of the golden diff** — and a printer that loses information
shows up as an `idempotent` failure over `test/job/` (113 real queries) or `test/tpch/`.

## Architecture

Four layers, each a thin specialization of an FE CRTP base:

- **`Driver`** (`driver.h`, `driver.cpp`) — owns the `SymPool`, the AST `Arena`, and everything
  interned once and for all: `keys()` (reserved words → `Tok::Tag`), `non_keys()` (non-reserved
  words as `Sym`s, indexed by `NonKey`), and `sym_error()`. There are a few hundred of these, so
  they live here rather than per-Lexer/Parser — constructing a `Parser` is O(1) and that is
  deliberate. Do not move interning back into the Lexer or Parser.
- **`Lexer`** (`lexer.h`, `lexer.cpp`) — `fe::Lexer<1, Lexer>`, one character of lookahead, UTF-8
  aware. `Lexer::lex()` is the single hot loop and carries `[[gnu::flatten]]` for that reason.
  The token's text is **not** accumulated in a buffer: the whole source sits in `buf_`, so `loc_`
  *is* the token and `view()` returns the slice for free. `lower()`/`upper()` return a folded copy,
  and only the paths that genuinely diverge from the source (unescaping a string literal) build a
  local `std::string`.
- **`Parser`** (`parser.h`, `parser.cpp`) — `fe::Parser<Tok, Tok::Tag, 2, Parser>`. Two tokens of
  lookahead, because `name(` vs. a plain `Id`, `(SELECT` vs. a parenthesized list, and `NOT LIKE`
  vs. unary `NOT` each need to peek one past the current token. Expressions use precedence climbing
  over `Tok::Prec`; everything else is straight recursive descent.
- **AST + printer** (`ast.h`, `stream.cpp`) — ~64 node classes, all arena-allocated via
  `Driver::ast<T>()` and held as `AST<T>` = `fe::Arena::Ptr<const T>`. Node lists are
  `ASTs<T>`/`Syms`, i.e. `fe::Vector` (small-buffer): qualified names have one to three parts and
  most other lists are just as short, so they must not go to the heap.

### Conventions that are easy to get wrong

- **X-macro token tables.** `tok.h` defines `SQL_TOK` (punctuation and value tokens), `SQL_KEY`
  (reserved words, which get a `Tok::Tag`) and `SQL_NON_KEY` (non-reserved words, which do **not** —
  they lex as `V_id` and are recognized by `Sym` comparison through `NonKey`/`isa_non_key`). Adding
  a word to the wrong list changes what the parser accepts everywhere. Reserved words are interned
  lower-cased via `to_lower`.
- **Token families.** `parser.cpp` defines `C_*` macros (`C_QUERY`, `C_TABLE_CONSTRAINT`, …) that
  expand to a run of `case` labels, plus `ISA(tag, family)` to use one as a predicate outside a
  `switch`. Extend the family, not the individual call sites.
- **Parse loosely, check later** (see README's Design section). Reserved words are accepted as
  identifiers via `parse_sym`, *except* in the three positions where a name stands entirely on its
  own — a lone reference, a type name, and the window a window spec refines. `stream.cpp` has a
  separate `Ref` streamer that quotes exactly there, and `Ident` everywhere else. Changing `Ident`
  to quote more broadly churns every golden file.
- **Statements are `Expr`s** (so subqueries need no second hierarchy) but the *grammar* keeps them
  apart; parentheses around a scalar expression are dropped, around a query they are kept.
- Naming: `is_` prefix for `bool`, `isa_` for `std::optional`/nullable pointer, `Camel_Snake_Case`
  for constants, trailing `_` on private members. Full list in README's Coding Style section.

## Benchmarking

`test/bench/` is not a CTest entry. The curated fixtures are a few hundred bytes each, so a run over
them measures per-statement overhead more than throughput; generate a realistic corpus instead:

```sh
test/bench/gen.py --mb 64 -o /tmp/big.sql       # JOB/TPC-H vocabulary, names reused
test/bench/gen.py --mb 64 --stress-names -o /tmp/big.sql   # every identifier distinct
./build/bin/bench --each /tmp/big.sql   # a Driver + Parser per file: what an embedding pays
./build/bin/bench --once /tmp/big.sql   # one Parser for everything: parsing throughput
./build/bin/bench --lex  /tmp/big.sql   # lexing alone
```

Wall-clock on this workload swings by tens of percent under load. Use `perf stat -e instructions`
as the primary signal — it is deterministic run to run — and treat MB/s as secondary.

## Working with the FE submodule

`submodules/fe` is a separate git repository with its own `CLAUDE.md`, carrying the same comment
policy as below plus a carve-out for Doxygen comments on the public API in `include/fe/`, which are
documentation and expected. Changes there are frequently part of the same task — the Lexer, Parser, `Sym`, and `Loc` all live in FE — but they are committed separately,
and `submodules/fe` shows up as a modified entry in this repository's `git status` until the pointer
is updated. Build and test FE on its own before assuming a change is good:

```sh
cmake -S submodules/fe -B build-fe -DBUILD_TESTING=ON && cmake --build build-fe
ctest --test-dir build-fe --output-on-failure
```

## Comments

Comments are scarce. The default is **no comment**.

Comment only when the code itself cannot reasonably express the information.

- Comment **why**, not what the code does.
- Prefer a better name, structure, or API over a comment.
- Keep comments to **one short sentence**, normally one line.
- When a comment spans multiple lines, use **one complete sentence per line**. Do not wrap a single sentence across multiple lines merely to fit a line-length limit.
- A comment should convey one fact only: an invariant, non-obvious constraint, algorithmic reason, or important external reference.
- Do not explain the implementation, summarize a function, or provide a narrative of its control flow.
- Match the comment density and brevity of the surrounding code. **Never increase comment density.**
- Do not add documentation-style prose, introductions, conclusions, or motivational/explanatory language.
- Do not use rhetorical contrasts such as `"X" -> "Y"`, `"instead of X"`, or `"from X to Y"` to explain an optimization.
- Do not add comments describing the change itself ("now handles X", "renamed from Y"); that belongs in the commit message.
- Do not add banner or section-header comments.
- Do not add a comment if deleting it would leave the code equally correct and understandable.
- A comment that restates the code is worse than no comment:
  ```cpp
  vec.push_back(x); // BAD: "put x into the vector"
  ```

**Hard limit:** Do not write multi-line comments unless the user explicitly asks for documentation or the comment is required to document a non-obvious invariant that cannot be stated briefly.

Before adding a comment, ask:
1. Is this information necessary?
2. Is it already apparent from the code or names?
3. Can it be expressed in one short sentence?
If the answer to 1 or 3 is no, do not add the comment.
