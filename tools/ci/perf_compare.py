#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare the micro-benchmarks' results with the budget.

    python3 tools/ci/perf_compare.py --results build/linux-perf/perf [--budget perf-budget.json]
                                     [--summary FILE] [--environment-file FILE] [--update]

--results is the directory the perf.* CTest tests write to: one Catch2 XML report for each library
(cmake/Perf.cmake). The budget (perf-budget.json) holds, for each benchmark, the mean time in
nanoseconds that was measured on the runner class named in "environment", and the percentage a result
may exceed it by ("threshold_percent") plus a few nanoseconds ("slack_ns", 50 unless set), so that a
benchmark of a few nanoseconds is not failed by clock noise. A result over its limit fails the run, and so does a benchmark
that has no entry or an entry that no benchmark matches, so the file cannot drift from the code. While
"enforce" is false the table is printed and nothing fails: the budget is first filled in from a run on
the CI runner itself, because times measured on another machine say nothing about it.

--update rewrites the means from the results (the thresholds stay). It is how the budget is first
filled in and how it is moved when a change makes something slower on purpose; read the diff of the
file like a change to a test's expected value.

Exit status: 0 within budget (or not enforced), 1 over budget or out of step, 2 a usage or file error.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def read_results(directory: Path) -> dict[str, float]:
    """Benchmark name -> mean time in nanoseconds, from every XML report in the directory."""
    results: dict[str, float] = {}
    reports = sorted(directory.glob("*.xml"))
    if not reports:
        raise SystemExit(f"no benchmark report (*.xml) in {directory}")
    for report in reports:
        for node in ET.parse(report).getroot().iter("BenchmarkResults"):
            name = node.get("name")
            mean = node.find("mean")
            if name is None or mean is None or mean.get("value") is None:
                raise SystemExit(f"{report}: a benchmark has no name or no mean")
            if name in results:
                raise SystemExit(f"{report}: the benchmark '{name}' is defined twice")
            results[name] = float(mean.get("value"))
    return results


def environment() -> dict[str, str]:
    """What the numbers were measured on, so a later reader can tell whether they compare."""
    cpu = ""
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    return {
        "os": platform.platform(),
        "machine": platform.machine(),
        "cpu": cpu or platform.processor(),
        "cpus": str(os.cpu_count() or ""),
        "runner": os.environ.get("ImageOS", "") + " " + os.environ.get("ImageVersion", ""),
        "compiler": os.environ.get("CXX", ""),
    }


def compare(results: dict[str, float], budget: dict) -> tuple[list[list[str]], list[str]]:
    """Rows of the table, and the problems."""
    threshold = float(budget.get("threshold_percent", 50))
    slack = float(budget.get("slack_ns", 50))
    entries = budget.get("benchmarks", {})
    rows: list[list[str]] = []
    problems: list[str] = []
    for name in sorted(results):
        got = results[name]
        entry = entries.get(name)
        if entry is None:
            rows.append([name, f"{got:,.0f}", "-", "-", "no budget"])
            problems.append(f"'{name}' has no entry in the budget (run with --update)")
            continue
        base = float(entry["mean_ns"])
        change = (got / base - 1.0) * 100.0 if base > 0 else 0.0
        over = got > base * (1.0 + threshold / 100.0) + slack
        rows.append([name, f"{got:,.0f}", f"{base:,.0f}", f"{change:+.0f}%", "OVER" if over else "ok"])
        if over:
            problems.append(f"'{name}' took {got:,.0f} ns against {base:,.0f} ns (+{change:.0f}%, the limit is +{threshold:.0f}%)")
    for name in sorted(set(entries) - set(results)):
        rows.append([name, "-", f"{float(entries[name]['mean_ns']):,.0f}", "-", "not run"])
        problems.append(f"the budget names '{name}', which no benchmark has now")
    return rows, problems


def table(rows: list[list[str]]) -> str:
    head = ["benchmark", "mean ns", "budget ns", "change", ""]
    lines = ["| " + " | ".join(head) + " |", "|" + "|".join(["---"] * len(head)) + "|"]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", required=True, type=Path)
    ap.add_argument("--budget", type=Path, default=Path("perf-budget.json"))
    ap.add_argument("--summary", type=Path, help="append the table to this file (the job summary)")
    ap.add_argument("--environment-file", type=Path, help="write what the numbers were measured on")
    ap.add_argument("--update", action="store_true", help="rewrite the means in the budget from the results")
    args = ap.parse_args(argv)

    results = read_results(args.results)
    if args.environment_file:
        args.environment_file.write_text(json.dumps(environment(), indent=2) + "\n", encoding="utf-8")
    try:
        budget = json.loads(args.budget.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"{args.budget}: {error}", file=sys.stderr)
        return 2

    if args.update:
        budget["benchmarks"] = {name: {"mean_ns": round(mean)} for name, mean in sorted(results.items())}
        budget["measured_on"] = environment()
        args.budget.write_text(json.dumps(budget, indent=2) + "\n", encoding="utf-8")
        print(f"{args.budget}: {len(results)} benchmark(s) written")
        return 0

    rows, problems = compare(results, budget)
    text = table(rows)
    print(text)
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as out:
            out.write(f"### Benchmarks against the budget (limit +{budget.get('threshold_percent', 50)}%)\n\n{text}\n")
    if not problems:
        return 0
    enforce = bool(budget.get("enforce", False))
    for problem in problems:
        print(("error: " if enforce else "not enforced: ") + problem, file=sys.stderr)
    return 1 if enforce else 0


if __name__ == "__main__":
    sys.exit(main())
