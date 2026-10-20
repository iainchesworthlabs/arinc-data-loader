# ---------------------------------------------------------------------------
# Coverage.cmake
#
# Defines an INTERFACE target `adl::coverage` that, when ADL_ENABLE_COVERAGE is
# on, turns on gcov-style source-based coverage instrumentation (GCC/Clang)
# via --coverage. Off by default: only the dedicated linux-gcc-coverage preset
# turns it on, so normal dev/CI builds pay no instrumentation cost.
#
# Link it PRIVATE into every first-party target whose coverage should be
# measured (the library and the test executables).
#
# With the instrumentation on, the `coverage` target runs the test suite and
# then enforces the project minimum: 90% of lines and 80% of branches of the
# portable code in src/. The figures and the gcovr invocation live in
# cmake/CoverageReport.cmake; the coverage CI job calls the same script.
# ---------------------------------------------------------------------------

option(ADL_ENABLE_COVERAGE "Enable gcov/llvm-cov source coverage instrumentation" OFF)
set(ADL_COVERAGE_LINE_THRESHOLD 90 CACHE STRING
    "Minimum line coverage (percent) the coverage target enforces")
set(ADL_COVERAGE_BRANCH_THRESHOLD 80 CACHE STRING
    "Minimum branch coverage (percent) the coverage target enforces")

add_library(adl_coverage INTERFACE)
add_library(adl::coverage ALIAS adl_coverage)

if(ADL_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # -fprofile-update=atomic: the code under test may be
        # exercised from several threads in the tests, and gcov's default
        # non-atomic counter updates then race. gcov derives the counts of arcs
        # it does not instrument by subtraction, so a lost update shows up as a
        # negative count and gcovr aborts the report (gcc bug 68080).
        target_compile_options(adl_coverage INTERFACE --coverage -fprofile-update=atomic -fno-inline)
        target_link_options(adl_coverage INTERFACE --coverage)

        find_program(ADL_GCOVR_EXECUTABLE NAMES gcovr)
        # The gcov that matches the compiler: gcovr's default `gcov` is the system's, and a GCC
        # newer than it writes data files the system gcov cannot read (an empty report). Clang
        # coverage reads its data through gcov-compatible llvm-cov, which gcovr finds itself.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            find_program(ADL_GCOV_EXECUTABLE NAMES gcov-${CMAKE_CXX_COMPILER_VERSION_MAJOR} gcov)
        endif()
        if(ADL_GCOVR_EXECUTABLE)
            add_custom_target(coverage
                COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
                COMMAND "${CMAKE_COMMAND}"
                    "-DREPO_ROOT=${PROJECT_SOURCE_DIR}"
                    "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
                    "-DLINE_THRESHOLD=${ADL_COVERAGE_LINE_THRESHOLD}"
                    "-DBRANCH_THRESHOLD=${ADL_COVERAGE_BRANCH_THRESHOLD}"
                    "-DGCOVR=${ADL_GCOVR_EXECUTABLE}"
                    "-DGCOV=${ADL_GCOV_EXECUTABLE}"
                    -P "${PROJECT_SOURCE_DIR}/cmake/CoverageReport.cmake"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT
                    "Running the tests and gating coverage at ${ADL_COVERAGE_LINE_THRESHOLD}% lines / ${ADL_COVERAGE_BRANCH_THRESHOLD}% branches"
                USES_TERMINAL)
        else()
            message(WARNING
                "ADL_ENABLE_COVERAGE is on but gcovr was not found; the "
                "coverage target is not available. Install "
                ".github/requirements/coverage.txt.")
        endif()
    else()
        message(WARNING
            "ADL_ENABLE_COVERAGE is on but ${CMAKE_CXX_COMPILER_ID} is not "
            "GCC/Clang; coverage instrumentation is not supported on this "
            "compiler and will be skipped.")
    endif()
endif()
