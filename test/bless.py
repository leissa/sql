#!/usr/bin/env python3
"""Regenerate every golden `.out` file under `parse/` and `error/`.

Driven by the `bless` build target: `cmake --build build --target bless`.
Review the resulting diff before committing it - a golden file is only as good as the
output someone actually read.
"""

import argparse
import pathlib
import subprocess
import sys

MODES = {"parse": "parse", "error": "error"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sql", required=True, help="path to the sql binary")
    parser.add_argument("--test-dir", required=True, type=pathlib.Path)
    args = parser.parse_args()

    run_test = args.test_dir / "run_test.py"
    failed = 0
    for directory, mode in MODES.items():
        for fixture in sorted((args.test_dir / directory).glob("*.sql")):
            res = subprocess.run(
                [sys.executable, str(run_test), "--sql", args.sql,
                 "--mode", mode, "--bless", str(fixture)]
            )
            if res.returncode != 0:
                print(f"error: could not bless {fixture}", file=sys.stderr)
                failed = 1
    return failed


if __name__ == "__main__":
    sys.exit(main())
