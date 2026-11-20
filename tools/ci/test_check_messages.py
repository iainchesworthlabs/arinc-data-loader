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


SCENARIO_READER = """std::unexpected<ReadError> fail() { return ReadError{"step " + number + " has the key " + key + ", which this version does not know"}; }
"""
SCENARIO_PAGE = "# Scenarios\n`step <n> has the key <key>, which this version does not know`\n"


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

    def add_scenario_reader(self, page: str = SCENARIO_PAGE) -> None:
        (self.root / "libs" / "scenario_base" / "src").mkdir(parents=True, exist_ok=True)
        (self.root / "libs" / "scenario_base" / "src" / "envelope.cpp").write_text(SCENARIO_READER, encoding="utf-8")
        (self.root / "docs" / "user-guide" / "mock-scenarios.md").write_text(page, encoding="utf-8")

    def test_a_sentence_of_the_scenario_readers_has_to_be_on_the_scenario_page(self) -> None:
        self.add_scenario_reader()
        self.assertEqual(self.problems(), "")
        self.add_scenario_reader(SCENARIO_PAGE.replace("which this version does not know", ""))
        self.assertIn("of the scenario file readers is not in docs/user-guide/mock-scenarios.md", self.problems())

    def test_a_missing_scenario_page_is_found_when_the_readers_are_there(self) -> None:
        self.add_scenario_reader()
        (self.root / "docs" / "user-guide" / "mock-scenarios.md").unlink()
        self.assertIn("of the scenario file readers is not in docs/user-guide/mock-scenarios.md", self.problems())

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


if __name__ == "__main__":
    unittest.main()


MAKE_SET_COMMAND = """#include "adl/make_set/something that is not a text.hpp"
namespace adl::make_set {
// A comment that says "words the guide need not have" is not a text.
inline constexpr int exit_ok = 0;
inline constexpr int exit_input = 3;
}
"""
MAKE_SET_SOURCE = """#include <string>
/* "a block comment with a literal in it" */
std::string say(const std::string& path)
{
    return "Cannot read " + path + " at all.";   // "and a trailing comment"
}
const char* usage = R"(usage: tool make <description>
  --output <folder>)";
"""
MAKE_SET_PAGE = """# Command line
| Code | Meaning |
| --- | --- |
| 0 | ok |
| 1 | failed |
| 2 | usage |
| 3 | another program's three |

## `adl_make_set`

usage: tool make <description>
  --output <folder>

| Code | Meaning |
| --- | --- |
| 0 | done |
| 3 | an input cannot be used |

`Cannot read <path> at all.`

## Other programs
"""


class MakeSet(Check):
    """adl_make_set has no catalogue: its sentences and exit codes are held to the section of its page."""

    def setUp(self) -> None:
        super().setUp()
        (self.root / "apps" / "make_set" / "src").mkdir(parents=True)
        (self.root / "apps" / "make_set" / "include" / "adl" / "make_set").mkdir(parents=True)
        (self.root / "apps" / "make_set" / "src" / "command.cpp").write_text(MAKE_SET_SOURCE, encoding="utf-8")
        (self.root / "apps" / "make_set" / "include" / "adl" / "make_set" / "command.hpp").write_text(
            MAKE_SET_COMMAND, encoding="utf-8")
        self.page = self.root / "docs" / "user-guide" / "command-line.md"
        self.page.write_text(GOOD_PAGE + "\n" + MAKE_SET_PAGE, encoding="utf-8")

    def test_a_page_that_has_everything_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_the_literals_are_read_without_the_comments_and_includes(self) -> None:
        literals = check_messages.source_literals(self.root / "apps" / "make_set" / "src" / "command.cpp")
        self.assertEqual(literals, ["Cannot read ", " at all.", "usage: tool make <description>\n  --output <folder>"])

    def test_a_sentence_that_is_not_in_the_section_is_found(self) -> None:
        self.page.write_text((GOOD_PAGE + "\n" + MAKE_SET_PAGE).replace("`Cannot read <path> at all.`", ""), encoding="utf-8")
        self.assertIn("the text 'Cannot read' of command.cpp is not in the section adl_make_set", self.problems())

    def test_a_sentence_on_another_part_of_the_page_does_not_count(self) -> None:
        self.page.write_text(
            (GOOD_PAGE + "\n" + MAKE_SET_PAGE).replace("`Cannot read <path> at all.`", "").replace(
                "## Other programs", "## Other programs\n\nCannot read <path> at all."), encoding="utf-8")
        self.assertIn("the text 'Cannot read'", self.problems())

    def test_an_exit_code_that_is_a_row_of_another_table_only_is_found(self) -> None:
        self.page.write_text((GOOD_PAGE + "\n" + MAKE_SET_PAGE).replace("| 3 | an input cannot be used |\n", ""), encoding="utf-8")
        self.assertIn("the exit code 3 of adl_make_set is not a row of the table in the section adl_make_set", self.problems())

    def test_a_page_with_no_section_is_found(self) -> None:
        self.page.write_text(GOOD_PAGE, encoding="utf-8")
        self.assertIn("command-line.md has no section for it", self.problems())

    def test_the_usage_text_is_asked_for_whole(self) -> None:
        self.page.write_text((GOOD_PAGE + "\n" + MAKE_SET_PAGE).replace("  --output <folder>\n\n| Code", "\n\n| Code"), encoding="utf-8")
        self.assertIn("usage: tool make", self.problems())

    def test_without_the_program_nothing_is_asked(self) -> None:
        (self.root / "apps" / "make_set" / "src" / "command.cpp").unlink()
        (self.root / "apps" / "make_set" / "include" / "adl" / "make_set" / "command.hpp").unlink()
        self.page.write_text(GOOD_PAGE, encoding="utf-8")
        self.assertEqual(self.problems(), "")
