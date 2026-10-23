#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-test of tools/ci/perf_compare.py (CTest: ci.perf_compare)."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import perf_compare  # noqa: E402

XML = """<?xml version="1.0"?>
<Catch2TestRun name="t">
  <TestCase name="c">
    <BenchmarkResults name="{name}" samples="50" resamples="1" iterations="1" clockResolution="1" estimatedDuration="1">
      <mean value="{mean}" lowerBound="1" upperBound="2" ci="0.95"/>
    </BenchmarkResults>
  </TestCase>
</Catch2TestRun>
"""


def write(directory: Path, name: str, mean: float, file: str = "a.xml") -> None:
    (directory / file).write_text(XML.format(name=name, mean=mean), encoding="utf-8")


class Compare(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.budget = self.dir / "budget.json"

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def run_with(self, budget: dict) -> int:
        self.budget.write_text(json.dumps(budget), encoding="utf-8")
        return perf_compare.main(["--results", str(self.dir), "--budget", str(self.budget)])

    def test_within_the_limit_passes(self):
        write(self.dir, "x", 140)
        self.assertEqual(self.run_with({"threshold_percent": 50, "enforce": True, "benchmarks": {"x": {"mean_ns": 100}}}), 0)

    def test_over_the_limit_fails_when_enforced(self):
        write(self.dir, "x", 160)
        self.assertEqual(self.run_with({"threshold_percent": 50, "slack_ns": 0, "enforce": True, "benchmarks": {"x": {"mean_ns": 100}}}), 1)

    def test_over_the_limit_is_reported_but_passes_when_not_enforced(self):
        write(self.dir, "x", 160)
        self.assertEqual(self.run_with({"threshold_percent": 50, "enforce": False, "benchmarks": {"x": {"mean_ns": 100}}}), 0)

    def test_a_few_nanoseconds_of_noise_on_a_tiny_benchmark_pass(self):
        write(self.dir, "x", 40)
        self.assertEqual(self.run_with({"threshold_percent": 10, "slack_ns": 50, "enforce": True, "benchmarks": {"x": {"mean_ns": 11}}}), 0)

    def test_faster_always_passes(self):
        write(self.dir, "x", 10)
        self.assertEqual(self.run_with({"threshold_percent": 5, "enforce": True, "benchmarks": {"x": {"mean_ns": 100}}}), 0)

    def test_a_benchmark_without_an_entry_fails(self):
        write(self.dir, "new", 1)
        self.assertEqual(self.run_with({"threshold_percent": 50, "enforce": True, "benchmarks": {}}), 1)

    def test_an_entry_without_a_benchmark_fails(self):
        write(self.dir, "x", 100)
        budget = {"threshold_percent": 50, "enforce": True, "benchmarks": {"x": {"mean_ns": 100}, "gone": {"mean_ns": 5}}}
        self.assertEqual(self.run_with(budget), 1)

    def test_no_report_is_an_error(self):
        self.budget.write_text("{}", encoding="utf-8")
        with self.assertRaises(SystemExit):
            perf_compare.main(["--results", str(self.dir), "--budget", str(self.budget)])

    def test_update_writes_the_means_and_keeps_the_threshold(self):
        write(self.dir, "x", 123.4)
        self.budget.write_text(json.dumps({"threshold_percent": 25, "enforce": True, "benchmarks": {}}), encoding="utf-8")
        self.assertEqual(perf_compare.main(["--results", str(self.dir), "--budget", str(self.budget), "--update"]), 0)
        written = json.loads(self.budget.read_text(encoding="utf-8"))
        self.assertEqual(written["benchmarks"], {"x": {"mean_ns": 123}})
        self.assertEqual(written["threshold_percent"], 25)
        self.assertTrue(written["enforce"])


if __name__ == "__main__":
    unittest.main()
