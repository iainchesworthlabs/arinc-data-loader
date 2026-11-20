#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-test of tools/ci/check_messages.py (CTest: ci.check_messages)."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_messages  # noqa: E402

CATALOGUE = """<?xml version="1.0" encoding="utf-8"?>
<TS version="2.1" language="de_DE">
<context>
    <name>Worker</name>
    <message>
        <location filename="../core/src/worker.cpp" line="10"/>
        <source>The media set could not be written below %1.</source>
        <translation>x</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>%1 KB</source>
        <translation>x</translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n load(s) were ticked in the list</source>
        <translation>x</translation>
    </message>
</context>
<context>
    <name>Window</name>
    <message>
        <location filename="../qml/Window.qml" line="3"/>
        <source>A button caption that nobody explains</source>
        <translation>x</translation>
    </message>
</context>
</TS>
"""

MAIN = """namespace {
constexpr int exit_ok = 0;
constexpr int exit_failed = 1;
constexpr int exit_usage = 2;
void print_usage(std::ostream& out)
{
    out << "usage: tool [--scenario <name>] <directory>\\n"
           "Writes the samples.\\n";
}
}
int main() { std::cerr << "no scenario called " << name << " (see --list)\\n"; std::cout << count(1, "member", "members") << ": " << "done\\n"; }
"""

GOOD_PAGE = """# Command line
| Code | Meaning |
| --- | --- |
| 0 | ok |
| 1 | failed |
| 2 | usage |

usage: tool [--scenario <name>] <directory>
Writes the samples.

no scenario called <name> (see --list)
done
"""

FINDING_TEXTS = (
    "2.1.1 the part number has an embedded blank. 2.2.2 the file name has a blank or a non-printable character. "
    "3.2.1 the file is more than ... directories deep."
)

GOOD_MESSAGES = (
    "The media set could not be written below `%1`. And %n load(s) were ticked in the list. " + FINDING_TEXTS
)

FINDINGS_SOURCE = """namespace
{
void add(Findings& findings, std::string clause, std::string message)
{
    findings.push_back({std::move(clause), std::move(message)});
}
}
void check()
{
    add(findings, "2.1.1", "the part number has an embedded blank");
    add(findings, "2.2.2", "the file name has a blank or a non-printable character");
    findings.push_back({"3.2.1", "the file is more than " + std::to_string(4) + " directories deep"});
}
"""

MOCK_OPTIONS = """#include "adl/mock/cli/options.hpp"
// a comment with "a quoted text that nobody asks for" in it
constexpr auto limit = 1'000'000; // and a digit separator, which is not a character literal
std::expected<Options, UsageError> parse()
{
    return fail(std::string{name} + " needs a value");
    return fail("--silent needs --seconds, because the operation never ends");
}
"""

MOCK_HEADER = "inline constexpr int exit_ok = 0;   // stopped as asked\ninline constexpr int exit_folder = 3; // a folder\n"

MOCK_ENGINE = """void log() { note(Severity::info, "listening: a log line that adl_mock does not show"); }
bool start() { last_error_ = "the FIND port could not be bound: " + error; return false; }
"""

MOCK_SECTION = """
## `adl_mock`

| Code | Meaning |
| --- | --- |
| 0 | stopped |
| 3 | folder |

`<option> needs a value` and `--silent needs --seconds, because the operation never ends`
and `the FIND port could not be bound: <reason>`.

## The windows programs
"""


class Check(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "apps" / "app" / "translations").mkdir(parents=True)
        (self.root / "apps" / "write_payloads").mkdir(parents=True)
        (self.root / "docs" / "user-guide").mkdir(parents=True)
        (self.root / "apps" / "app" / "translations" / "app_de.ts").write_text(CATALOGUE, encoding="utf-8")
        (self.root / "apps" / "write_payloads" / "main.cpp").write_text(MAIN, encoding="utf-8")
        (self.root / "docs" / "user-guide" / "command-line.md").write_text(GOOD_PAGE, encoding="utf-8")
        (self.root / "docs" / "user-guide" / "messages.md").write_text(GOOD_MESSAGES, encoding="utf-8")
        (self.root / "docs" / "file-reference.md").write_text("", encoding="utf-8")
        (self.root / "libs" / "arinc665" / "src").mkdir(parents=True)
        (self.root / "libs" / "arinc665" / "src" / "naming.cpp").write_text(FINDINGS_SOURCE, encoding="utf-8")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def problems(self) -> str:
        return "\n".join(check_messages.check(self.root))

    def test_a_guide_that_has_everything_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_the_repository_passes(self) -> None:
        self.assertEqual(check_messages.check(check_messages.ROOT), [])

    def test_only_the_classes_of_the_code_are_asked_for(self) -> None:
        found = check_messages.code_messages(self.root)
        self.assertIn("The media set could not be written below %1.", found)
        self.assertNotIn("A button caption that nobody explains", found)

    def test_the_findings_are_read_from_the_calls_and_not_from_the_definition(self) -> None:
        found = check_messages.finding_literals(self.root)
        self.assertEqual([literals for _, _, literals in found][:2], [["2.1.1", "the part number has an embedded blank"], ["2.2.2", "the file name has a blank or a non-printable character"]])
        self.assertEqual(len(found), 3)

    def test_a_finding_that_is_not_in_the_guide_is_found(self) -> None:
        page = self.root / "docs" / "user-guide" / "messages.md"
        page.write_text(GOOD_MESSAGES.replace("3.2.1 the file is more than ... directories deep.", ""), encoding="utf-8")
        text = self.problems()
        self.assertIn("the file is more than", text)
        self.assertNotIn("embedded blank", text)

    def test_a_message_that_is_not_in_the_guide_is_found(self) -> None:
        (self.root / "docs" / "user-guide" / "messages.md").write_text("Nothing here.", encoding="utf-8")
        text = self.problems()
        self.assertIn("could not be written below", text)
        self.assertIn("load(s) were ticked", text)

    def test_a_piece_of_a_format_is_not_asked_for(self) -> None:
        self.assertEqual(check_messages.segments("%1 KB"), [])
        self.assertNotIn("KB", self.problems())

    def test_placeholders_match_anything_and_spacing_and_marks_are_ignored(self) -> None:
        (self.root / "docs" / "user-guide" / "messages.md").write_text(
            "**The media set could not be   written below** C:/out. 3 load were ticked in the list " + FINDING_TEXTS, encoding="utf-8")
        self.assertEqual(self.problems(), "")

    def test_an_exit_code_that_is_not_a_row_is_found(self) -> None:
        page = self.root / "docs" / "user-guide" / "command-line.md"
        page.write_text(GOOD_PAGE.replace("| 2 | usage |\n", ""), encoding="utf-8")
        self.assertIn("the exit code 2 is not a row", self.problems())

    def test_a_new_exit_code_in_the_source_is_found(self) -> None:
        main = self.root / "apps" / "write_payloads" / "main.cpp"
        main.write_text(MAIN.replace("exit_usage = 2;", "exit_usage = 2;\nconstexpr int exit_other = 3;"), encoding="utf-8")
        self.assertIn("the exit code 3", self.problems())

    def test_a_windowed_program_exit_code_is_read(self) -> None:
        (self.root / "apps" / "app" / "main.cpp").write_text("[] { QCoreApplication::exit(7); }", encoding="utf-8")
        self.assertIn("the exit code 7", self.problems())

    def test_a_printed_text_that_is_not_in_the_page_is_found(self) -> None:
        page = self.root / "docs" / "user-guide" / "command-line.md"
        page.write_text(GOOD_PAGE.replace("no scenario called <name> (see --list)", "no scenario"), encoding="utf-8")
        self.assertIn("no scenario called", self.problems())

    def test_the_printed_texts_are_the_literal_operands(self) -> None:
        texts = check_messages.printed_texts(self.root)
        self.assertIn("usage: tool [--scenario <name>] <directory>\nWrites the samples.\n", texts)
        self.assertIn("no scenario called ", texts)
        self.assertNotIn("member", "".join(texts))


    def add_mock(self, page: str = GOOD_PAGE + MOCK_SECTION) -> None:
        (self.root / "apps" / "mock" / "src").mkdir(parents=True)
        (self.root / "apps" / "mock" / "include" / "adl" / "mock" / "cli").mkdir(parents=True)
        (self.root / "libs" / "mock_target_core" / "src").mkdir(parents=True)
        (self.root / "apps" / "mock" / "src" / "options.cpp").write_text(MOCK_OPTIONS, encoding="utf-8")
        (self.root / "apps" / "mock" / "include" / "adl" / "mock" / "cli" / "command.hpp").write_text(MOCK_HEADER, encoding="utf-8")
        (self.root / "libs" / "mock_target_core" / "src" / "engine.cpp").write_text(MOCK_ENGINE, encoding="utf-8")
        (self.root / "docs" / "user-guide" / "command-line.md").write_text(page, encoding="utf-8")

    def test_adl_mock_with_everything_in_its_section_passes(self) -> None:
        self.add_mock()
        self.assertEqual(self.problems(), "")
        self.assertEqual(check_messages.mock_exit_codes(self.root), {0, 3})

    def test_a_text_of_adl_mock_that_is_not_in_the_page_is_found(self) -> None:
        self.add_mock(GOOD_PAGE + MOCK_SECTION.replace("--silent needs --seconds, because the operation never ends", ""))
        self.assertIn("because the operation never ends", self.problems())
        self.assertIn("of adl_mock", self.problems())

    def test_an_exit_code_of_adl_mock_has_to_be_in_its_own_section(self) -> None:
        # 3 is a row of the page, but of the table of another program, which does not count for adl_mock.
        page = GOOD_PAGE.replace("| 2 | usage |", "| 2 | usage |\n| 3 | another |") + MOCK_SECTION.replace("| 3 | folder |\n", "")
        self.add_mock(page)
        self.assertIn("the exit code 3 of adl_mock is not a row of its section", self.problems())

    def test_the_error_texts_of_the_engine_are_asked_for_and_its_log_lines_are_not(self) -> None:
        self.add_mock()
        texts = check_messages.mock_texts(self.root)
        self.assertIn("the FIND port could not be bound: ", texts)
        self.assertNotIn("listening: a log line that adl_mock does not show", texts)

    def test_comments_includes_and_digit_separators_are_not_texts(self) -> None:
        literals = check_messages.literals_of(MOCK_OPTIONS)
        self.assertEqual(literals, [" needs a value", "--silent needs --seconds, because the operation never ends"])

    def test_a_literal_is_unescaped(self) -> None:
        self.assertEqual(
            check_messages.literals_of(r'x = "a \"quoted\" word\n" "and a \\ backslash";'),
            ['a "quoted" word\n', "and a \\ backslash"],
        )


OWNER_MESSAGES = """namespace adl::licence_owner::message {
inline constexpr std::string_view read_failed = "cannot read %1: %2";
inline constexpr std::string_view usage = R"(usage: tool <command>
  verify <file>   judge it
)";
inline constexpr std::string_view joined = "the first half of a sentence, "
    "and the second half of it";
inline constexpr std::string_view reason = "it does not exist";
}
"""
OWNER_EXIT_CODES = """inline constexpr int exit_ok = 0;
inline constexpr int exit_failed = 1;
inline constexpr int exit_usage = 2;
"""
OWNER_PAGE = """# The owner's tool
usage: tool <command>
  verify <file>   judge it

| Code | Meaning |
| --- | --- |
| 0 | done |
| 1 | not done |
| 2 | usage |

`cannot read %1: %2`, `the first half of a sentence, and the second half of it`, `it does not exist`
"""


class OwnerTool(Check):
    """The owner's licence tool is not shipped: its own page is held to its own sentences and exit codes."""

    def setUp(self) -> None:
        super().setUp()
        (self.root / "tools" / "licence" / "src").mkdir(parents=True)
        (self.root / "tools" / "licence" / "src" / "messages.hpp").write_text(OWNER_MESSAGES, encoding="utf-8")
        (self.root / "tools" / "licence" / "src" / "owner.hpp").write_text(OWNER_EXIT_CODES, encoding="utf-8")
        self.page = self.root / "docs" / "licence-owner-tool.md"
        self.page.write_text(OWNER_PAGE, encoding="utf-8")

    def test_a_page_that_has_everything_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_a_raw_string_and_joined_literals_are_one_sentence_each(self) -> None:
        self.assertEqual(
            check_messages.owner_tool_messages(self.root),
            ["cannot read %1: %2", "usage: tool <command>\n  verify <file>   judge it\n",
             "the first half of a sentence, and the second half of it", "it does not exist"])

    def test_a_sentence_that_is_not_on_the_page_is_found(self) -> None:
        self.page.write_text(OWNER_PAGE.replace("`it does not exist`", ""), encoding="utf-8")
        self.assertIn("'it does not exist' of the owner's tool is not on docs/licence-owner-tool.md", self.problems())

    def test_a_joined_sentence_is_asked_for_whole(self) -> None:
        self.page.write_text(OWNER_PAGE.replace("and the second half of it", ""), encoding="utf-8")
        self.assertIn("the first half of a sentence", self.problems())

    def test_the_usage_text_is_asked_for_whole(self) -> None:
        self.page.write_text(OWNER_PAGE.replace("  verify <file>   judge it\n", ""), encoding="utf-8")
        self.assertIn("usage: tool", self.problems())

    def test_an_exit_code_that_is_not_a_row_is_found(self) -> None:
        self.page.write_text(OWNER_PAGE.replace("| 2 | usage |\n", ""), encoding="utf-8")
        self.assertIn("the exit code 2 of the owner's tool is not a row of the table in docs/licence-owner-tool.md", self.problems())

    def test_a_new_exit_code_is_found(self) -> None:
        header = self.root / "tools" / "licence" / "src" / "owner.hpp"
        header.write_text(OWNER_EXIT_CODES + "inline constexpr int exit_other = 3;\n", encoding="utf-8")
        self.assertIn("the exit code 3 of the owner's tool", self.problems())

    def test_a_missing_page_is_found(self) -> None:
        self.page.unlink()
        self.assertIn("docs/licence-owner-tool.md does not exist", self.problems())

    def test_without_the_tool_nothing_is_asked(self) -> None:
        (self.root / "tools" / "licence" / "src" / "messages.hpp").unlink()
        (self.root / "tools" / "licence" / "src" / "owner.hpp").unlink()
        self.page.unlink()
        self.assertEqual(self.problems(), "")


LICENCE_COMMAND_MESSAGES = """namespace adl::licence_cli::message {
inline constexpr std::string_view usage = R"(usage: adl_licence show
       adl_licence import <file>
)";
inline constexpr std::string_view cannot_read = "cannot read %1: %2";
inline constexpr std::string_view label_edition = "Edition:";
}
"""
LICENCE_COMMAND_EXIT_CODES = """inline constexpr int exit_ok = 0;
inline constexpr int exit_failed = 1;
inline constexpr int exit_usage = 2;
"""
LICENCE_COMMAND_SECTION = """
## `adl_licence`

usage: adl_licence show
       adl_licence import <file>

The first line of `show` is `Edition: pro`. `cannot read %1: %2`.

| Code | Meaning |
| --- | --- |
| 0 | done |
| 1 | not done |
| 2 | usage |
"""


class LicenceCommand(Check):
    """adl_licence is a program of the guide, but its sentences and exit codes are held to its own section of the page."""

    def setUp(self) -> None:
        super().setUp()
        (self.root / "libs" / "licence_cli" / "src").mkdir(parents=True)
        (self.root / "libs" / "licence_cli" / "include" / "adl" / "licence_cli").mkdir(parents=True)
        (self.root / "libs" / "licence_cli" / "src" / "messages.hpp").write_text(LICENCE_COMMAND_MESSAGES, encoding="utf-8")
        self.codes = self.root / "libs" / "licence_cli" / "include" / "adl" / "licence_cli" / "licence_cli.hpp"
        self.codes.write_text(LICENCE_COMMAND_EXIT_CODES, encoding="utf-8")
        self.page = self.root / "docs" / "user-guide" / "command-line.md"
        self.page.write_text(GOOD_PAGE + LICENCE_COMMAND_SECTION, encoding="utf-8")

    def test_a_page_that_has_everything_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_a_sentence_that_is_not_on_the_page_is_found(self) -> None:
        self.page.write_text(GOOD_PAGE + LICENCE_COMMAND_SECTION.replace("`cannot read %1: %2`", ""), encoding="utf-8")
        self.assertIn("'cannot read %1: %2' of adl_licence is not on docs/user-guide/command-line.md", self.problems())

    def test_the_usage_text_is_asked_for_whole(self) -> None:
        self.page.write_text(GOOD_PAGE + LICENCE_COMMAND_SECTION.replace("       adl_licence import <file>\n", ""),
                             encoding="utf-8")
        self.assertIn("usage: adl_licence show", self.problems())

    def test_a_new_exit_code_is_found(self) -> None:
        self.codes.write_text(LICENCE_COMMAND_EXIT_CODES + "inline constexpr int exit_other = 3;\n", encoding="utf-8")
        self.assertIn("the exit code 3 of adl_licence is not a row of the table in docs/user-guide/command-line.md",
                      self.problems())

    def test_without_the_program_nothing_is_asked(self) -> None:
        (self.root / "libs" / "licence_cli" / "src" / "messages.hpp").unlink()
        self.codes.unlink()
        self.page.write_text(GOOD_PAGE, encoding="utf-8")
        self.assertEqual(self.problems(), "")


if __name__ == "__main__":
    unittest.main()
