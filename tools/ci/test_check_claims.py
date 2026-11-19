#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-test of tools/ci/check_claims.py (CTest: ci.check_claims)."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_claims  # noqa: E402


class Claims(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "docs" / "plan").mkdir(parents=True)
        (self.root / "tools" / "docs").mkdir(parents=True)
        self.config = {"standards_named_in": ["docs/assurance.md"], "exceptions": []}
        self.write_config()
        self.page("docs/index.md", "A plain page about a data loader.\n")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def write_config(self) -> None:
        (self.root / check_claims.EXCEPTIONS).write_text(json.dumps(self.config), encoding="utf-8")

    def page(self, name: str, text: str) -> None:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def problems(self) -> str:
        return "\n".join(check_claims.check(self.root)[0])

    def test_a_plain_tree_passes(self) -> None:
        self.assertEqual(check_claims.check(self.root), ([], [], []))

    def test_the_repository_passes(self) -> None:
        problems, _, unused = check_claims.check(check_claims.ROOT)
        self.assertEqual(problems, [])
        self.assertEqual(unused, [])

    def test_each_word_is_found_in_any_case_and_with_its_line(self) -> None:
        for word in ("certified", "Qualified", "COMPLIANT", "approved", "guaranteed"):
            self.page("docs/index.md", f"One line.\nThe tool is {word}.\n")
            self.assertIn(f"docs/index.md:2: says '{word}'", self.problems())

    def test_a_longer_word_is_not_a_claim(self) -> None:
        self.page("docs/index.md", "A certification liaison, a qualification plan, an unapproved draft and a guarantee.\n")
        self.assertEqual(self.problems(), "")

    def test_the_plans_are_not_scanned_and_the_root_files_are(self) -> None:
        self.page("docs/plan/x.md", "The tool is not qualified.\n")
        self.assertEqual(self.problems(), "")
        self.page("README.md", "It is approved.\n")
        self.assertIn("README.md:1", self.problems())
        self.page("changelog.d/x.added.md", "Guaranteed.\n")
        self.assertIn("changelog.d/x.added.md:1", self.problems())

    def test_an_exception_covers_its_phrase_in_its_file_across_a_line_break(self) -> None:
        self.page("docs/support.md", "There is no guaranteed\nresponse time.\n")
        self.assertIn("docs/support.md:1", self.problems())
        self.config["exceptions"] = [{"file": "docs/support.md", "phrase": "no guaranteed response time", "reason": "x", "approved_by": ""}]
        self.write_config()
        self.assertEqual(self.problems(), "")

    def test_an_exception_does_not_cover_another_file_or_another_use(self) -> None:
        self.config["exceptions"] = [{"file": "docs/support.md", "phrase": "no guaranteed response time", "reason": "x", "approved_by": ""}]
        self.write_config()
        self.page("docs/support.md", "No guaranteed response time. The tool is guaranteed.\n")
        self.page("docs/faq.md", "No guaranteed response time.\n")
        text = self.problems()
        self.assertIn("docs/support.md:1: says 'guaranteed'", text)
        self.assertIn("docs/faq.md:1", text)

    def test_an_exception_that_matches_nothing_is_reported(self) -> None:
        self.config["exceptions"] = [{"file": "docs/gone.md", "phrase": "no guaranteed response time", "reason": "x", "approved_by": ""}]
        self.write_config()
        self.assertEqual(len(check_claims.check(self.root)[2]), 1)
        self.assertEqual(check_claims.main(["--root", str(self.root)]), 1)

    def test_an_exception_waits_for_approval_and_strict_fails_until_it_has_it(self) -> None:
        self.page("docs/support.md", "No guaranteed response time.\n")
        self.config["exceptions"] = [{"file": "docs/support.md", "phrase": "no guaranteed response time", "reason": "x", "approved_by": ""}]
        self.write_config()
        self.assertEqual(len(check_claims.check(self.root)[1]), 1)
        self.assertEqual(check_claims.main(["--root", str(self.root)]), 0)
        self.assertEqual(check_claims.main(["--root", str(self.root), "--strict"]), 1)
        self.config["exceptions"][0]["approved_by"] = "the owner"
        self.write_config()
        self.assertEqual(check_claims.check(self.root)[1], [])
        self.assertEqual(check_claims.main(["--root", str(self.root), "--strict"]), 0)

    def test_the_standards_may_be_named_only_in_the_listed_pages(self) -> None:
        self.page("docs/assurance.md", "Your programme may apply DO-178C and DO-330.\n")
        self.assertEqual(self.problems(), "")
        self.page("docs/features.md", "It follows DO-178C.\n")
        self.assertIn("docs/features.md:1: names DO-178C", self.problems())


if __name__ == "__main__":
    unittest.main()
