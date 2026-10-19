# ---------------------------------------------------------------------------
# CoverageReport.cmake
#
# Runs gcovr over the coverage build and enforces the project's minimum coverage:
# at least 90% of lines and 80% of branches of the portable code in src/. It is the
# one place those figures and the gcovr switches live; the `coverage` target and
# the coverage CI job both call it.
#
#   cmake -DREPO_ROOT=<source root> -DBUILD_DIR=<coverage build dir>
#         [-DLINE_THRESHOLD=90] [-DBRANCH_THRESHOLD=80]
#         [-DGCOVR=<gcovr>] [-DGCOV=<gcov that matches the compiler>]
#         -P cmake/CoverageReport.cmake
#
# The report is written to <build dir>/coverage: index.html with one page per
# source file, summary.json and cobertura.xml. --exclude-throw-branches and
# --exclude-unreachable-branches are the standard's exclusions (a throwing
# branch and a line the compiler proves unreachable are not testable), and the
# gcov parse-error switches tolerate a negative counter (gcc bug 68080) while
# naming it once instead of aborting the report.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_required REPO_ROOT BUILD_DIR)
    if(NOT ${_required})
        message(FATAL_ERROR "Pass -D${_required}=<value>.")
    endif()
endforeach()

if(NOT DEFINED LINE_THRESHOLD)
    set(LINE_THRESHOLD 90)
endif()
if(NOT DEFINED BRANCH_THRESHOLD)
    set(BRANCH_THRESHOLD 80)
endif()

if(NOT GCOVR)
    find_program(GCOVR NAMES gcovr)
endif()
if(NOT GCOVR)
    message(FATAL_ERROR
        "gcovr was not found. Install the pinned tool:\n"
        "  pip install -r .github/requirements/coverage.txt")
endif()

set(_output "${BUILD_DIR}/coverage")
file(MAKE_DIRECTORY "${_output}")

# The portable code only: the two format libraries, the engine and its runner, the media tree, the
# samples, the payload writer, and the platform interfaces. The OS fragments under src/platforms
# are not measured (each host builds a different set, and the conformance suite is what holds them
# to the contract), and neither are the Qt applications, which have their own tests and UI suite.
set(_arguments
    --root "${REPO_ROOT}"
    --filter "${REPO_ROOT}/src/(arinc665|arinc615a|arinc615a_mini|arinc615a_runner|media_tree|samples|write_payloads|platforms/(interfaces|detail))/.*"
    --object-directory "${BUILD_DIR}"
    --exclude-throw-branches
    --exclude-unreachable-branches
    --gcov-ignore-errors=no_working_dir_found
    --gcov-ignore-parse-errors
    negative_hits.warn_once_per_file
    --fail-under-line "${LINE_THRESHOLD}"
    --fail-under-branch "${BRANCH_THRESHOLD}"
    --print-summary
    --html-details "${_output}/index.html"
    --json-summary "${_output}/summary.json"
    --json-summary-pretty
    --cobertura "${_output}/cobertura.xml"
    --cobertura-pretty)

if(GCOV)
    list(INSERT _arguments 6 --gcov-executable "${GCOV}")
endif()

execute_process(
    COMMAND "${GCOVR}" ${_arguments}
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE _result)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "Coverage is below the minimum (${LINE_THRESHOLD}% lines, "
        "${BRANCH_THRESHOLD}% branches), or gcovr failed (${_result}). "
        "The report is in ${_output}.")
endif()

# gcovr reports 0 of 0 lines as 100%, so a filter that matches nothing (a renamed directory) or a
# gcov that cannot read the data passes both thresholds. The totals have to be there too.
file(READ "${_output}/summary.json" _summary)
string(JSON _line_total GET "${_summary}" line_total)
string(JSON _branch_total GET "${_summary}" branch_total)
if(_line_total LESS 1000 OR _branch_total LESS 100)
    message(FATAL_ERROR
        "Coverage measured only ${_line_total} lines and ${_branch_total} branches: the filter "
        "matched too little or gcov could not read the data, so the percentages mean nothing. "
        "The report is in ${_output}.")
endif()

message(STATUS "Coverage meets the minimum: ${LINE_THRESHOLD}% lines and "
               "${BRANCH_THRESHOLD}% branches. Report: ${_output}/index.html")
