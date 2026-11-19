#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that every message and exit code the code can produce is in the user's guide.

    python3 tools/ci/check_messages.py [--root .]

What the programs say to the person using them has two sources, and this check holds the guide to both.

1. The messages of the C++ code. Each program has translation catalogues (apps/*/translations, libs/ui/translations), and the
   sources in them are the English texts. A context whose texts are written in a .cpp or .hpp file belongs to a class of the code
   (the loader's engine worker, the generator's worker, the wording of a status and so on), as against a context of a window,
   whose texts are in a .qml file and are mostly labels. Every text of the first kind that has wording in it must appear in the
   guide (docs/user-guide and docs/file-reference.md). The match ignores case, emphasis marks and spacing, and treats a
   placeholder (%1, %n) as anything: it asks for each stretch of 6 or more characters between placeholders. A text with no such
   stretch ("%1 KB", "None") is a piece of a format and is not asked for.
2. The findings of the ARINC 665 checks (the departures from the standard's recommendations). They are made in libs/arinc665/src by
   add(findings, clause, message) and are not in a catalogue. Every string literal of such a call must appear in the guide
   (docs/user-guide/findings.md lists them).
3. Exit codes and the command line. Every exit code that the programs' code sets (the exit_* constants of
   apps/write_payloads/main.cpp, and the exit(N) calls of the three windowed programs and of the smoke test) must be a row of
   the table in docs/user-guide/command-line.md, and every text that adl_write_payloads prints must be in that page.
4. adl_mock, which has no Qt and so no catalogue. Its exit codes (the exit_* constants of its command.hpp) must be rows of the table
   in the page's section of adl_mock, and every text in the literals of the sources that make what it says (MOCK_SOURCES: the
   usage errors, the folder and port errors, the usage text) must be in the page, with the same match as the rest.

The check does not decide that the description is good, only that it is there. A new message or exit code fails it until the guide says
something about it. Run it after a change to a message. tests/CMakeLists.txt runs it as the test docs.messages.
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOGUES = ("apps/*/translations/*.ts", "libs/ui/translations/*.ts")
GUIDE = ("docs/user-guide/*.md", "docs/file-reference.md")
COMMAND_LINE_PAGE = "docs/user-guide/command-line.md"
MIN_SEGMENT = 6

# adl_mock (apps/mock) has no catalogue: what it says is in the literals of these sources. A file with no pattern is read
# whole; with one, only the statements that begin with it are (the assignments of an error text, in the engine and the link,
# which also have log lines that adl_mock does not show).
MOCK_HEADING = "adl_mock"
MOCK_EXIT_CODES = "apps/mock/include/adl/mock/cli/command.hpp"
MOCK_SOURCES = (
    ("apps/mock/src/options.cpp", None),
    ("apps/mock/src/command.cpp", None),
    ("libs/mock_target_core/src/storage.cpp", None),
    ("libs/mock_target_core/src/engine.cpp", r"last_error_\s*="),
    ("libs/mock_target_core/src/link.cpp", r"\berror\s*\+?="),
)


def normalise(text: str) -> str:
    text = html.unescape(text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"[`*_\"'‘’“”]", "", text)
    return re.sub(r"\s+", " ", text).strip().lower()


def segments(text: str) -> list[str]:
    """The stretches of wording of a message: what is left between the placeholders."""
    parts = re.split(r"%l?\d|%n|\(s\)", normalise(text))
    return [p.strip(" ,.:;()[]") for p in parts if len(p.strip(" ,.:;()[]")) >= MIN_SEGMENT]


def code_messages(root: Path) -> dict[str, str]:
    """English text -> the context it is in, for the contexts that are classes of the code."""
    found: dict[str, str] = {}
    for pattern in CATALOGUES:
        for path in sorted(root.glob(pattern)):
            text = path.read_text(encoding="utf-8")
            for name, body in re.findall(r"<context>\s*<name>(.*?)</name>(.*?)</context>", text, flags=re.S):
                files = re.findall(r'filename="([^"]+)"', body)
                if not any(f.endswith((".cpp", ".hpp")) for f in files):
                    continue
                for source in re.findall(r"<source>(.*?)</source>", body, flags=re.S):
                    found.setdefault(html.unescape(source), name)
    return found


def guide_text(root: Path) -> str:
    chunks = []
    for pattern in GUIDE:
        for path in sorted(root.glob(pattern)):
            chunks.append(path.read_text(encoding="utf-8"))
    return normalise(" ".join(chunks))


def exit_codes(root: Path) -> set[int]:
    codes: set[int] = set()
    main = root / "apps" / "write_payloads" / "main.cpp"
    if main.is_file():
        codes.update(int(n) for n in re.findall(r"constexpr\s+int\s+exit_\w+\s*=\s*(\d+)\s*;", main.read_text(encoding="utf-8")))
    for path in [*sorted((root / "apps").glob("*/main.cpp")), root / "libs" / "ui" / "smoke_test.cpp"]:
        if path.is_file():
            codes.update(int(n) for n in re.findall(r"\bexit\(\s*(\d+)\s*\)", path.read_text(encoding="utf-8")))
    return codes | mock_exit_codes(root)


def printed_texts(root: Path) -> list[str]:
    """The string literals that adl_write_payloads writes to the user, joined as the compiler joins adjacent ones.

    An operand of `<<` that is only literals counts; an operand that calls a function or names a variable does not.
    """
    main = root / "apps" / "write_payloads" / "main.cpp"
    if not main.is_file():
        return []
    source = main.read_text(encoding="utf-8")
    literal = re.compile(r'"((?:[^"\\]|\\.)*)"')
    texts = []
    for statement in re.findall(r"(?:std::cerr|std::cout|out)\s*<<(.*?);", source, flags=re.S):
        for operand in statement.split("<<"):
            if literal.sub("", operand).strip():
                continue
            text = "".join(literal.findall(operand)).replace("\\n", "\n").replace('\\"', '"')
            if text.strip():
                texts.append(text)
    return texts


STRING_LITERAL = r'"(?:[^"\\\n]|\\.)*"'
SOURCE_TOKEN = re.compile(STRING_LITERAL + r"|'(?:[^'\\\n]|\\.)'|//[^\n]*|/\*.*?\*/", flags=re.S)


def literals_of(source: str, statement: str | None = None) -> list[str]:
    """The string literals of C++ source, unescaped, outside its comments and its #include lines.

    With `statement`, only those of the statements that begin with that pattern (up to the semicolon that ends them).
    """
    source = SOURCE_TOKEN.sub(lambda m: m.group(0) if m.group(0)[0] in "\"'" else " ", source)
    source = "\n".join(line for line in source.split("\n") if not line.lstrip().startswith("#"))
    if statement is not None:
        chunks = [m.group(0) for m in re.finditer(rf"(?:{statement})(?:{STRING_LITERAL}|[^;\"])*;", source)]
    else:
        chunks = [source]
    unescape = {"n": "\n", "t": "\t"}
    return [
        re.sub(r"\\(.)", lambda m: unescape.get(m.group(1), m.group(1)), literal[1:-1])
        for chunk in chunks
        for literal in re.findall(STRING_LITERAL, chunk)
    ]


def mock_texts(root: Path) -> list[str]:
    """The lines of the texts adl_mock says, from the literals of its sources."""
    lines: list[str] = []
    for name, statement in MOCK_SOURCES:
        path = root / name
        if path.is_file():
            for literal in literals_of(path.read_text(encoding="utf-8"), statement):
                lines.extend(line for line in literal.split("\n") if line.strip())
    return lines


def section_of(page: str, heading: str) -> str:
    """The text of the section of `page` whose second-level heading is `heading` (backticks around it are allowed)."""
    match = re.search(rf"^## `?{re.escape(heading)}`?[ \t]*$(.*?)(?=^## |\Z)", page, flags=re.S | re.M)
    return match.group(1) if match else ""


def mock_exit_codes(root: Path) -> set[int]:
    path = root / MOCK_EXIT_CODES
    if not path.is_file():
        return set()
    return {int(n) for n in re.findall(r"constexpr\s+int\s+exit_\w+\s*=\s*(\d+)\s*;", path.read_text(encoding="utf-8"))}


FINDING_CALL = re.compile(r"(?<![\w:.])(?:add|findings\.push_back)\(")


def finding_literals(root: Path) -> list[tuple[str, int, list[str]]]:
    """The wording of the findings of adl::arinc665: (file, line, the string literals of each call that makes one).

    A finding is made by add(findings, clause, message) or by a push_back of a clause and a message in libs/arinc665/src. A message
    may be several literals joined with values, so each literal is returned and each is asked for. The two places that define the
    helper (they take a variable and no literal) are skipped.
    """
    found = []
    for path in sorted((root / "libs" / "arinc665" / "src").glob("*.cpp")):
        source = path.read_text(encoding="utf-8")
        for match in FINDING_CALL.finditer(source):
            start = match.start()
            position = source.index("(", start)
            depth = 0
            while True:
                char = source[position]
                if char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
                    if depth == 0:
                        break
                elif char == '"':
                    position += 1
                    while source[position] != '"':
                        if source[position] == "\\":
                            position += 1
                        position += 1
                position += 1
            call = source[start : position + 1]
            literals = re.findall(r'"((?:[^"\\]|\\.)*)"', call)
            if literals:
                found.append((path.name, source.count("\n", 0, start) + 1, literals))
    return found


def check(root: Path) -> list[str]:
    problems: list[str] = []
    guide = guide_text(root)
    for name, line, literals in finding_literals(root):
        for literal in literals:
            wanted = normalise(re.sub(r"\\(.)", r"\1", literal))
            if len(wanted.strip(" ,.:;()[]")) >= MIN_SEGMENT and wanted.strip(" ,.:;()[]") not in guide:
                problems.append(f"the finding text {literal!r} ({name}:{line}) is not in the guide: add it to docs/user-guide/findings.md")
    for source, context in sorted(code_messages(root).items()):
        needed = segments(source)
        if needed and not all(part in guide for part in needed):
            problems.append(f"the message {source!r} (context {context}) is not in the guide: describe it in docs/user-guide/messages.md")
    page_path = root / COMMAND_LINE_PAGE
    page = normalise(page_path.read_text(encoding="utf-8")) if page_path.is_file() else ""
    rows = page_path.read_text(encoding="utf-8") if page_path.is_file() else ""
    for code in sorted(exit_codes(root)):
        if not re.search(rf"^\|\s*{code}\s*\|", rows, flags=re.M):
            problems.append(f"the exit code {code} is not a row of the tables in {COMMAND_LINE_PAGE}")
    for text in printed_texts(root):
        for line in (ln for ln in text.split("\n") if ln.strip()):
            for part in segments(line) or ([normalise(line)] if len(normalise(line)) >= MIN_SEGMENT else []):
                if part not in page:
                    problems.append(f"the text {line.strip()!r} of adl_write_payloads is not in {COMMAND_LINE_PAGE}")
                    break
    section = section_of(rows, MOCK_HEADING)
    for code in sorted(mock_exit_codes(root)):
        if not re.search(rf"^\|\s*{code}\s*\|", section, flags=re.M):
            problems.append(f"the exit code {code} of adl_mock is not a row of its section of {COMMAND_LINE_PAGE}")
    for line in mock_texts(root):
        for part in segments(line) or ([normalise(line)] if len(normalise(line)) >= MIN_SEGMENT else []):
            if part not in page:
                problems.append(f"the text {line.strip()!r} of adl_mock is not in {COMMAND_LINE_PAGE}")
                break
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    try:
        problems = check(args.root)
    except (OSError, ValueError) as error:
        print(f"check_messages: cannot check: {error}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    messages = code_messages(args.root)
    print(f"Messages: {len(messages)} texts of the code, {len(finding_literals(args.root))} findings and "
          f"{len(exit_codes(args.root))} exit codes are in the guide")
    return 0


if __name__ == "__main__":
    sys.exit(main())
