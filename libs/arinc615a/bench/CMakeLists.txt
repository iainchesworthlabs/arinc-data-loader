# SPDX-License-Identifier: GPL-3.0-or-later
# Micro-benchmarks of the TFTP packets and the FIND answer (docs/performance.md). Catch2's BENCHMARK,
# one executable, run by the CTest test perf.arinc615a (label perf), which writes the results as XML
# for tools/ci/perf_compare.py.
find_package(Catch2 3 CONFIG REQUIRED)

add_executable(adl_bench_arinc615a bench_arinc615a.cpp)
target_include_directories(adl_bench_arinc615a PRIVATE "${PROJECT_SOURCE_DIR}/tests/support" "${PROJECT_SOURCE_DIR}/tests/golden")
target_link_libraries(adl_bench_arinc615a
    PRIVATE
        adl::arinc615a
        adl::warnings
        Catch2::Catch2WithMain)

adl_add_perf_test(arinc615a adl_bench_arinc615a)
