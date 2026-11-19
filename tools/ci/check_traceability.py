#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the links from the standards' coverage tables to the tests, and report the gaps.

    python3 tools/ci/check_traceability.py [--root .] [--report]

docs/arinc665-coverage.md and docs/arinc615a-coverage.md each have a table, "Section by section", with one row for each section of the standard:
what it asks for, where the code does it, and the tests that hold it down. There are no requirement identifiers yet (docs/plan/traceability.md
proposes them), so the section number is the identifier and the Tests column is the link. This check holds that link to the tests:

- each test named in the column (`header_file_test`) is a test source file of the repository;
- each test case named in brackets after it ("fixed layout") is the name of a test case in that file;
- each row has a test, or says why not: `n/a`, `by construction`, or `as <section>` (the same tests as another row).

It prints the gaps (the rows that say n/a or by construction) and with --report prints each. A gap does not fail the check: it is a fact to know.
tests/CMakeLists.txt runs it as the test docs.traceability.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLES = ("docs/arinc665-coverage.md", "docs/arinc615a-coverage.md")
SKIP = {".git", "out", "build", "deps", ".cache", ".vs", "vcpkg_installed"}
TOKEN = re.compile(r"`(\w+_test)`(?:\s*\(([^)]*)\))?")


def test_files(root: Path) -> dict[str, Path]:
    """The test source files by name: header_file_test -> .../header_file_test.cpp"""
    found: dict[str, Path] = {}
    for path in sorted(root.rglob("*_test.cpp")):
        if SKIP.intersection(path.relative_to(root).parts):
            continue
        found.setdefault(path.stem, path)
    return found


def rows(root: Path, doc: str) -> list[tuple[str, str]]:
    """(the first cell, the Tests cell) of each row of the 'Section by section' table."""
    text = (root / doc).read_text(encoding="utf-8")
    start = text.index("## Section by section")
    end = text.find("\n## ", start + 5)
    table = text[start : end if end != -1 else len(text)]
    out = []
    for line in table.splitlines():
        if not line.startswith("| ") or line.startswith("| ---"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        out.append((cells[0], cells[-1]))
    return out[1:]  # the header


def analyse(root: Path) -> tuple[list[str], dict[str, list[str]], dict[str, int]]:
    """(problems, gaps by kind, counts)"""
    files = test_files(root)
    sources: dict[str, str] = {}
    problems: list[str] = []
    gaps: dict[str, list[str]] = {"n/a": [], "by construction": []}
    counts = {"rows": 0, "linked": 0, "same as another row": 0, "test cases checked": 0}
    for doc in TABLES:
        for section, cell in rows(root, doc):
            counts["rows"] += 1
            where = f"{doc} {section}"
            lowered = cell.lower()
            tokens = TOKEN.findall(cell)
            if tokens:
                counts["linked"] += 1
            elif lowered.startswith("n/a"):
                gaps["n/a"].append(where)
                continue
            elif lowered.startswith("by construction"):
                gaps["by construction"].append(where)
                continue
            elif lowered.startswith("as "):
                counts["same as another row"] += 1
                continue
            else:
                problems.append(f"{where}: the Tests cell {cell!r} names no test, and is not n/a, by construction or 'as <section>'")
                continue
            for name, detail in tokens:
                path = files.get(name)
                if path is None:
                    problems.append(f"{where}: {name} is not a test source file of the repository")
                    continue
                source = sources.setdefault(name, path.read_text(encoding="utf-8", errors="replace"))
                for case in re.findall(r'"([^"]+)"', detail or ""):
                    counts["test cases checked"] += 1
                    # A page may cut a long test name short with "...": the start must be there.
                    wanted = case[:-3].rstrip() if case.endswith("...") else case
                    if wanted not in source:
                        problems.append(f"{where}: {name} has no test case or section called {case!r}")
    return problems, gaps, counts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", action="store_true", help="list each gap")
    args = parser.parse_args(argv)
    try:
        problems, gaps, counts = analyse(args.root)
    except (OSError, ValueError) as error:
        print(f"check_traceability: cannot check: {error}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"::error::{problem}")
    print(f"Traceability: {counts['rows']} sections, {counts['linked']} linked to a test file, {counts['same as another row']} as another row, "
          f"{counts['test cases checked']} named test cases found; gaps: {len(gaps['n/a'])} n/a, {len(gaps['by construction'])} by construction")
    if args.report:
        for kind, items in gaps.items():
            for item in items:
                print(f"  gap ({kind}): {item}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
