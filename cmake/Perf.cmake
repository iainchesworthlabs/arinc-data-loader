# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# Perf.cmake
#
# ADL_BUILD_BENCHMARKS builds the Catch2 micro-benchmarks under libs/<lib>/bench (Catch2 comes from
# vcpkg, as for the tests). adl_add_perf_test(<name> <executable>) registers the CTest test
# perf.<name> (label perf), which runs the executable and writes its results as XML to
# <build dir>/perf/<name>.xml for tools/ci/perf_compare.py (docs/performance.md). The perf label is
# run by the linux-perf preset only; a benchmark is not a test of correctness.
# ---------------------------------------------------------------------------

option(ADL_BUILD_BENCHMARKS "Build the Catch2 micro-benchmarks (libs/*/bench)" OFF)

if(ADL_BUILD_BENCHMARKS)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/perf")
endif()

function(adl_add_perf_test name executable)
    add_test(NAME perf.${name}
        COMMAND ${executable}
            --benchmark-samples 50
            --reporter "XML::out=${CMAKE_BINARY_DIR}/perf/${name}.xml"
            --reporter "console::out=-")
    set_tests_properties(perf.${name} PROPERTIES TIMEOUT 600 LABELS "perf")
endfunction()
