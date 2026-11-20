#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-test of tools/ci/check_traceability.py (CTest: ci.check_traceability)."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_traceability  # noqa: E402

DOC = """# Coverage

## Section by section

| Section | What | Where | Tests |
| --- | --- | --- | --- |
{rows}

## Not yet verified
"""

TEST_CPP = """TEST_CASE("fixed layout of the header", "[x]") {}
TEST_CASE("a long name that the page cuts short", "[x]") {}
"""


class Traceability(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "docs").mkdir()
        (self.root / "tests").mkdir()
        (self.root / "tests" / "header_file_test.cpp").write_text(TEST_CPP, encoding="utf-8")
        self.rows('| 1.1 | a | b | `header_file_test` ("fixed layout") |')

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def rows(self, text: str) -> None:
        (self.root / "docs" / "arinc665-coverage.md").write_text(DOC.format(rows=text), encoding="utf-8")
        (self.root / "docs" / "arinc615a-coverage.md").write_text(DOC.format(rows="| 5.1 | a | b | n/a |"), encoding="utf-8")

    def run_check(self):  # noqa: ANN202
        return check_traceability.analyse(self.root)

    def test_a_linked_row_passes(self) -> None:
        problems, gaps, counts = self.run_check()
        self.assertEqual(problems, [])
        self.assertEqual(counts["rows"], 2)
        self.assertEqual(counts["linked"], 1)
        self.assertEqual(counts["test cases checked"], 1)
        self.assertEqual(gaps["n/a"], ["docs/arinc615a-coverage.md 5.1"])

    def test_the_repository_passes(self) -> None:
        problems, _, counts = check_traceability.analyse(check_traceability.ROOT)
        self.assertEqual(problems, [])
        self.assertGreater(counts["linked"], 90)

    def test_a_test_file_that_does_not_exist_is_found(self) -> None:
        self.rows("| 1.1 | a | b | `gone_test` |")
        self.assertIn("gone_test is not a test source file", "\n".join(self.run_check()[0]))

    def test_a_test_case_that_does_not_exist_is_found(self) -> None:
        self.rows('| 1.1 | a | b | `header_file_test` ("no such case") |')
        self.assertIn("no test case or section called 'no such case'", "\n".join(self.run_check()[0]))

    def test_a_name_cut_short_with_dots_matches_its_start(self) -> None:
        self.rows('| 1.1 | a | b | `header_file_test` ("a long name ...") |')
        self.assertEqual(self.run_check()[0], [])
        self.rows('| 1.1 | a | b | `header_file_test` ("a different name ...") |')
        self.assertNotEqual(self.run_check()[0], [])

    def test_the_accepted_ways_to_have_no_test(self) -> None:
        self.rows("| 1.1 | a | b | n/a (see below) |\n| 1.2 | a | b | by construction |\n| 1.3 | a | b | as 1.1 |")
        problems, gaps, counts = self.run_check()
        self.assertEqual(problems, [])
        self.assertEqual(len(gaps["n/a"]), 2)
        self.assertEqual(gaps["by construction"], ["docs/arinc665-coverage.md 1.2"])
        self.assertEqual(counts["same as another row"], 1)

    def test_a_row_with_prose_and_no_test_is_found(self) -> None:
        self.rows("| 1.1 | a | b | every test there is |")
        self.assertIn("names no test", "\n".join(self.run_check()[0]))


if __name__ == "__main__":
    unittest.main()
