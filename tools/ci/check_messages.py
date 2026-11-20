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
4. adl_make_set, which has no catalogue (no Qt). Its exit codes (the exit_* constants of command.hpp) must be rows of the table
   in the page's section "adl_make_set" (the others are matched anywhere in the page, which would let a code pass by the table of
   another program), and every string literal of its sources (the command, the options, the description reader, and the rules of
   libs/set_maker that its sentences come from) must be in that section, with the match the rest of this script has. Comments and
   #include lines are not texts.
4. The owner's licence tool (tools/licence), which is not shipped and so is not a program of the guide. Every sentence in
   tools/licence/src/messages.hpp and every exit_* constant of tools/licence/src/owner.hpp must be on its own page,
   docs/licence-owner-tool.md, by the same rules (a placeholder stands for anything; a row of a table for a code).

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
    return codes


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


MAKE_SET_SOURCES = (
    "apps/make_set/src/command.cpp",
    "apps/make_set/src/options.cpp",
    "apps/make_set/src/description.cpp",
    "libs/set_maker/src/validate.cpp",
)
MAKE_SET_EXIT_CODES = "apps/make_set/include/adl/make_set/command.hpp"
MAKE_SET_HEADING = re.compile(r"^## .*adl_make_set.*$", flags=re.M)
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", flags=re.S)
INCLUDE_LINE = re.compile(r"^\s*#include.*$", flags=re.M)


def source_literals(path: Path) -> list[str]:
    """The string literals of a C++ file, ordinary and raw, without those of its comments and #include lines."""
    source = INCLUDE_LINE.sub("", COMMENT.sub("", path.read_text(encoding="utf-8")))
    # findall gives "" for the form that did not match, so a raw literal is the one whose first group is not empty.
    return [raw or re.sub(r"\\(.)", lambda m: "\n" if m.group(1) == "n" else m.group(1), ordinary)
            for raw, ordinary in STRING_LITERAL.findall(source)]


def make_set_section(page: str) -> str:
    """The section of the command-line page that describes adl_make_set: from its heading to the next of its level."""
    start = MAKE_SET_HEADING.search(page)
    if start is None:
        return ""
    following = re.compile(r"^## ", flags=re.M).search(page, start.end())
    return page[start.start() : following.start() if following else len(page)]


def make_set_problems(root: Path) -> list[str]:
    """What adl_make_set says and returns that the section of its page does not have."""
    sources = [root / name for name in MAKE_SET_SOURCES if (root / name).is_file()]
    codes_path = root / MAKE_SET_EXIT_CODES
    codes = sorted({int(n) for n in re.findall(r"constexpr\s+int\s+exit_\w+\s*=\s*(\d+)\s*;", codes_path.read_text(encoding="utf-8"))}) if codes_path.is_file() else []
    if not sources and not codes:
        return []
    page_path = root / COMMAND_LINE_PAGE
    raw = make_set_section(page_path.read_text(encoding="utf-8")) if page_path.is_file() else ""
    if not raw:
        return [f"adl_make_set has sentences and exit codes and {COMMAND_LINE_PAGE} has no section for it"]
    section = normalise(raw)
    problems = []
    for path in sources:
        for literal in source_literals(path):
            wanted = normalise(literal)
            for part in segments(literal) or ([wanted] if len(wanted.strip(" ,.:;()[]")) >= MIN_SEGMENT else []):
                if part not in section:
                    problems.append(f"the text {literal.strip()!r} of {path.name} is not in the section adl_make_set of {COMMAND_LINE_PAGE}")
                    break
    for code in codes:
        if not re.search(rf"^\|\s*{code}\s*\|", raw, flags=re.M):
            problems.append(f"the exit code {code} of adl_make_set is not a row of the table in the section adl_make_set of {COMMAND_LINE_PAGE}")
    return problems


OWNER_TOOL_MESSAGES = "tools/licence/src/messages.hpp"
OWNER_TOOL_EXIT_CODES = "tools/licence/src/owner.hpp"
OWNER_TOOL_PAGE = "docs/licence-owner-tool.md"
# A string literal of the sentences' header: an ordinary one, or a raw one (the usage text).
STRING_LITERAL = re.compile(r'R"\((.*?)\)"|"((?:[^"\\]|\\.)*)"', flags=re.S)
STRING_VIEW_DECLARATION = re.compile(r"inline constexpr std::string_view \w+\s*=")
WHITESPACE = re.compile(r"\s*")


def owner_tool_messages(root: Path) -> list[str]:
    """The sentences of the owner's tool: each `inline constexpr std::string_view` of its messages.hpp, literals joined."""
    path = root / OWNER_TOOL_MESSAGES
    if not path.is_file():
        return []
    source = path.read_text(encoding="utf-8")
    sentences = []
    # The literals of a declaration are read one after another from where the `=` ends, and not by one pattern
    # that repeats the alternation of the two literal forms (a repeated alternation is the shape CodeQL flags as
    # able to backtrack without end).
    for declaration in STRING_VIEW_DECLARATION.finditer(source):
        pieces = []
        position = declaration.end()
        while True:
            literal = STRING_LITERAL.match(source, WHITESPACE.match(source, position).end())
            if literal is None:
                break
            raw, ordinary = literal.groups()
            pieces.append(raw if raw is not None else re.sub(r"\\(.)", lambda m: "\n" if m.group(1) == "n" else m.group(1), ordinary))
            position = literal.end()
        if source.startswith(";", WHITESPACE.match(source, position).end()):
            sentences.append("".join(pieces))
    return sentences


def owner_tool_problems(root: Path) -> list[str]:
    """What the owner's tool says and returns that its page does not have."""
    problems = []
    sentences = owner_tool_messages(root)
    codes_path = root / OWNER_TOOL_EXIT_CODES
    codes = sorted({int(n) for n in re.findall(r"constexpr\s+int\s+exit_\w+\s*=\s*(\d+)\s*;", codes_path.read_text(encoding="utf-8"))}) if codes_path.is_file() else []
    if not sentences and not codes:
        return problems
    page_path = root / OWNER_TOOL_PAGE
    if not page_path.is_file():
        return [f"the owner's licence tool has sentences and exit codes and {OWNER_TOOL_PAGE} does not exist"]
    raw = page_path.read_text(encoding="utf-8")
    page = normalise(raw)
    for sentence in sentences:
        for part in segments(sentence):
            if part not in page:
                problems.append(f"the sentence {sentence.strip()!r} of the owner's tool is not on {OWNER_TOOL_PAGE}: describe it there")
                break
    for code in codes:
        if not re.search(rf"^\|\s*{code}\s*\|", raw, flags=re.M):
            problems.append(f"the exit code {code} of the owner's tool is not a row of the table in {OWNER_TOOL_PAGE}")
    return problems


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
    problems.extend(owner_tool_problems(root))
    problems.extend(make_set_problems(root))
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
