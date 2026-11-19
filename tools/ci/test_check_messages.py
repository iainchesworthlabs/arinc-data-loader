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


if __name__ == "__main__":
    unittest.main()
