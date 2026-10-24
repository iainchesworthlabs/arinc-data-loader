# SPDX-License-Identifier: GPL-3.0-or-later
# Micro-benchmarks of the check values, the header file and the media set (docs/performance.md).
# Catch2's BENCHMARK, one executable, run by the CTest test perf.arinc665 (label perf), which writes
# the results as XML for tools/ci/perf_compare.py.
find_package(Catch2 3 CONFIG REQUIRED)

add_executable(adl_bench_arinc665 bench_arinc665.cpp)
target_link_libraries(adl_bench_arinc665
    PRIVATE
        adl::arinc665
        adl::samples
        adl::warnings
        Catch2::Catch2WithMain)

adl_add_perf_test(arinc665 adl_bench_arinc665)
