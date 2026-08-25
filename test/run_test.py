#!/usr/bin/env python3
"""Black-box test driver for the `sql` command-line utility.

Every test runs the real binary on a `.sql` fixture and inspects only what a user would see:
its exit code, its stdout, and its diagnostics. Nothing links against the parser.

Modes
  parse       `sql -d <file>` must succeed; its dump must match the `.out` golden.
  error       `sql <file>` must fail; its diagnostics must match the `.out` golden.
  idempotent  Dumping a dump must reproduce it verbatim - the printer has to emit
              syntax the parser reads back into the very same AST.

`--bless` rewrites the golden instead of comparing against it.
"""

import argparse
import difflib
import pathlib
import subprocess
import sys


def run(sql, args, cwd, stdin=None):
    return subprocess.run(
        [sql, *args], input=stdin, capture_output=True, text=True, cwd=cwd
    )


def report(kind, expected, actual):
    """Print a unified diff and return the failing exit code."""
    print(f"error: {kind} does not match the golden file", file=sys.stderr)
    diff = difflib.unified_diff(
        expected.splitlines(keepends=True),
        actual.splitlines(keepends=True),
        fromfile="expected",
        tofile="actual",
    )
    sys.stderr.writelines(diff)
    return 1


def check_golden(golden, actual, kind, bless):
    if bless:
        golden.write_text(actual)
        print(f"blessed {golden}")
        return 0
    if not golden.exists():
        print(f"error: missing golden file {golden}; re-run with --bless", file=sys.stderr)
        return 1
    expected = golden.read_text()
    return 0 if expected == actual else report(kind, expected, actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sql", required=True, help="path to the sql binary")
    parser.add_argument("--mode", required=True, choices=("parse", "error", "idempotent"))
    parser.add_argument("--bless", action="store_true", help="rewrite the golden file")
    parser.add_argument("file", type=pathlib.Path, help="the .sql fixture")
    args = parser.parse_args()

    sql = str(pathlib.Path(args.sql).resolve())
    fixture = args.file.resolve()
    # Run from the fixture's directory and pass a bare name, so the paths in the
    # diagnostics stay independent of where the checkout lives.
    cwd, name = fixture.parent, fixture.name
    golden = fixture.with_suffix(".out")

    if args.mode == "parse":
        res = run(sql, ["-d", name], cwd)
        if res.returncode != 0:
            print(f"error: expected a clean parse, got exit {res.returncode}", file=sys.stderr)
            sys.stderr.write(res.stderr)
            return 1
        if res.stderr:
            print("error: expected no diagnostics", file=sys.stderr)
            sys.stderr.write(res.stderr)
            return 1
        return check_golden(golden, res.stdout, "dump", args.bless)

    if args.mode == "error":
        res = run(sql, [name], cwd)
        if res.returncode == 0:
            print("error: expected the parse to fail, but it succeeded", file=sys.stderr)
            return 1
        return check_golden(golden, res.stderr, "diagnostics", args.bless)

    # idempotent
    first = run(sql, ["-d", name], cwd)
    if first.returncode != 0:
        print(f"error: expected a clean parse, got exit {first.returncode}", file=sys.stderr)
        sys.stderr.write(first.stderr)
        return 1

    second = run(sql, ["-d", "-"], cwd, stdin=first.stdout)
    if second.returncode != 0:
        print("error: the dump does not parse back", file=sys.stderr)
        sys.stderr.write(second.stderr)
        return 1
    if first.stdout != second.stdout:
        return report("re-dump", first.stdout, second.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
