#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail when a page says the tool is certified, qualified, compliant, approved or guaranteed, without an approved exception.

    python3 tools/ci/check_claims.py [--root .] [--strict]

ARINC Data Loader is none of those things as far as the project can show, and a page that says so, or seems to, would be a claim the
project cannot back. This check reads README.md, SECURITY.md, CONTRIBUTING.md, CHANGELOG.md, the changelog fragments and every Markdown
page of docs/ except docs/plan/ (the plans are not published, and they have to discuss qualification), and fails on each use of one of
the words, ignoring case, that is not covered by an entry of tools/docs/claims_exceptions.json.

An exception names one file and the words around the word (spelling and line breaks do not matter) and says why the use is not a claim: a
statement that something is NOT so ("no guaranteed response time"), a warning text that GnuPG prints, or a use that is about something
else. The owner approves each exception by putting a name in its "approved_by". The check passes with exceptions that no one has approved
yet and says how many wait. With --strict they fail it, which is how the owner can hold a release to a fully approved list.

The names of the two standards, DO-178C and DO-330, may appear only where claims_exceptions.json lists a file in "standards_named_in"
(the Assurance page), because the project's pages may name them only to say which standards a user's own programme applies.

tests/CMakeLists.txt runs it as the test docs.claims.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXCEPTIONS = Path("tools/docs/claims_exceptions.json")
WORDS = ("certified", "qualified", "compliant", "approved", "guaranteed")
STANDARDS = ("DO-178C", "DO-330")
WORD = re.compile(r"\b(" + "|".join(WORDS) + r")\b", re.IGNORECASE)
STANDARD = re.compile("|".join(re.escape(s) for s in STANDARDS))


def scanned_files(root: Path) -> list[Path]:
    files = [root / name for name in ("README.md", "SECURITY.md", "CONTRIBUTING.md", "CHANGELOG.md") if (root / name).is_file()]
    files += sorted((root / "changelog.d").glob("*.md"))
    for path in sorted((root / "docs").rglob("*.md")):
        if "plan" in path.relative_to(root / "docs").parts[:1]:
            continue
        files.append(path)
    return files


def load(root: Path) -> dict:
    return json.loads((root / EXCEPTIONS).read_text(encoding="utf-8"))


def blank_out(text: str, phrase: str) -> tuple[str, int]:
    """Replace each use of the phrase with blanks of the same shape, so line numbers stay. Words match across line breaks."""
    pattern = re.compile(r"\s+".join(re.escape(part) for part in phrase.split()), re.IGNORECASE)
    return pattern.subn(lambda m: re.sub(r"\S", " ", m.group(0)), text)


def check(root: Path) -> tuple[list[str], list[str], list[str]]:
    """(problems, exceptions that were used and are not approved, exceptions that matched nothing)"""
    config = load(root)
    exceptions = config["exceptions"]
    standards_ok = set(config.get("standards_named_in", []))
    problems: list[str] = []
    used: set[int] = set()
    for path in scanned_files(root):
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8")
        for index, entry in enumerate(exceptions):
            if entry["file"] == rel:
                text, count = blank_out(text, entry["phrase"])
                if count:
                    used.add(index)
        for number, line in enumerate(text.split("\n"), 1):
            for match in WORD.finditer(line):
                problems.append(f"{rel}:{number}: says {match.group(1)!r}; the project cannot back that claim. Reword it, or add an exception to {EXCEPTIONS.as_posix()}")
            if STANDARD.search(line) and rel not in standards_ok:
                problems.append(f"{rel}:{number}: names {STANDARD.search(line).group(0)}; only the pages in standards_named_in may")
    unapproved = [f"{e['file']}: {e['phrase']!r}" for i, e in enumerate(exceptions) if i in used and not e.get("approved_by", "").strip()]
    unused = [f"{e['file']}: {e['phrase']!r}" for i, e in enumerate(exceptions) if i not in used]
    return problems, unapproved, unused


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--strict", action="store_true", help="fail while an exception in use has no approved_by")
    args = parser.parse_args(argv)
    try:
        problems, unapproved, unused = check(args.root)
    except (OSError, ValueError, KeyError) as error:
        print(f"check_claims: cannot check: {error}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"::error::{problem}")
    for entry in unused:
        print(f"::error::the exception for {entry} matches nothing: remove it, so the list stays what the pages need")
        problems.append(entry)
    if unapproved:
        level = "error" if args.strict else "notice"
        print(f"::{level}::{len(unapproved)} exception(s) wait for the owner's approval (approved_by in {EXCEPTIONS.as_posix()}): " + "; ".join(unapproved))
        if args.strict:
            return 1
    if problems:
        return 1
    print(f"Claims: no page says {', '.join(WORDS)} outside the exception list")
    return 0


if __name__ == "__main__":
    sys.exit(main())
