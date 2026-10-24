# Performance

*For developers. What CI runs is [CI](ci.md); the tests of correctness are in [Testing](testing.md).*

The speed of the parts that run on every byte of a load or every packet of a transfer is measured with
Catch2's `BENCHMARK`, so that a change that makes one of them slower is seen when it is made:

| Library | Benchmarks |
| --- | --- |
| `libs/arinc665/bench` | The CRC-16, CRC-32, MD5 and SHA-1 of 1 MiB; decoding a load's header file; building and reading back the sample media set |
| `libs/arinc615a/bench` | Encoding and decoding TFTP data packets of 512 and 8192 bytes; decoding a read request with the checksum option and a FIND answer |

The benchmarks are not part of the ordinary build or test run. The `linux-perf` preset builds the
libraries and the benchmarks only, in Release:

```sh
cmake --preset linux-perf
cmake --build --preset linux-perf
ctest --preset linux-perf        # runs them, about ten seconds; reports go to build/linux-perf/perf/*.xml
python3 tools/ci/perf_compare.py --results build/linux-perf/perf
```

## The budget

`perf-budget.json` holds, for each benchmark, the mean time in nanoseconds measured on one runner class,
the percentage a result may exceed it by (`threshold_percent`) and a few nanoseconds of slack
(`slack_ns`) for a benchmark too short for the clock to resolve. `tools/ci/perf_compare.py` prints
the table, adds it to the job summary, and fails when a result is over its limit, a benchmark has no entry
or an entry has no benchmark. While `enforce` is `false`, which is how the file starts, it reports and does
not fail.

Times are only comparable on the same kind of machine, so the budget is filled in from a run on the CI
runner (the job's `benchmark-results` artifact holds the reports and a note of what it ran on) and not from
a developer's computer:

1. Run the `Benchmarks (Linux, GCC)` job a few times on `develop` (`gh workflow run ci.yml --ref develop -f legs=perf`).
2. Take the reports from the artifact into a directory and run
   `python3 tools/ci/perf_compare.py --results <directory> --update`.
3. Read the diff of `perf-budget.json`, set `enforce` to `true`, and commit it.

A change that makes something slower on purpose runs `--update` the same way, in the same pull request,
and the diff of the budget is what a reviewer reads.

## In CI

The `Benchmarks (Linux, GCC)` job of `ci.yml` runs in the merge queue and on a push to `develop`, not on a
pull request: it needs a runner that is not busy with other jobs of the same pull request, and a
regression it finds is attributed to the merge that could have caused it. It is part of `CI Status`.
