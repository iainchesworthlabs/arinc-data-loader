# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# CoverageReport.cmake
#
# Runs gcovr over the coverage build and enforces the project's minimum coverage in two passes.
# It is the one place those figures and the gcovr switches live; the `coverage` target and the
# coverage CI job both call it.
#
#   1. The portable code in libs/ and apps/write_payloads: at least 90% of lines and 80% of branches.
#   2. The applications' window-free logic (apps/app/core, apps/mock_target/core and
#      apps/media_gen/core: the models, the engine's worker, the settings, the generator's and the
#      mock target's cores), at a lower floor set a little under what the suite measures. It needs
#      the Qt kit, so the coverage build has to be configured with the applications on; a build
#      without them measures too little, and the pass fails instead of passing on nothing.
#
#   cmake -DREPO_ROOT=<source root> -DBUILD_DIR=<coverage build dir>
#         [-DLINE_THRESHOLD=90] [-DBRANCH_THRESHOLD=80]
#         [-DAPP_LINE_THRESHOLD=..] [-DAPP_BRANCH_THRESHOLD=..]
#         [-DAPP_MIN_LINES=..] [-DAPP_MIN_BRANCHES=..]
#         [-DGCOVR=<gcovr>] [-DGCOV=<gcov that matches the compiler>]
#         -P cmake/CoverageReport.cmake
#
# The first report is written to <build dir>/coverage: index.html with one page per source file,
# summary.json and cobertura.xml; the second to <build dir>/coverage/apps in the same form.
# --exclude-throw-branches and --exclude-unreachable-branches are the standard's exclusions (a
# throwing branch and a line the compiler proves unreachable are not testable), and the gcov
# parse-error switches tolerate a negative counter (gcc bug 68080) while naming it once instead of
# aborting the report.
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
# The applications' logic: a floor a little under what the suite measures (see pass 2).
if(NOT DEFINED APP_LINE_THRESHOLD)
    set(APP_LINE_THRESHOLD 80)
endif()
if(NOT DEFINED APP_BRANCH_THRESHOLD)
    set(APP_BRANCH_THRESHOLD 66)
endif()
# Less than this in pass 2 means the pass looked at (almost) nothing.
if(NOT DEFINED APP_MIN_LINES)
    set(APP_MIN_LINES 1000)
endif()
if(NOT DEFINED APP_MIN_BRANCHES)
    set(APP_MIN_BRANCHES 300)
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

# The switches both passes share. The gcov that matches the compiler is passed when there is one.
set(_common
    --root "${REPO_ROOT}"
    --object-directory "${BUILD_DIR}"
    --exclude-throw-branches
    --exclude-unreachable-branches
    --gcov-ignore-errors=no_working_dir_found
    --gcov-ignore-parse-errors
    negative_hits.warn_once_per_file
    --print-summary
    --json-summary-pretty
    --cobertura-pretty)
if(GCOV)
    list(APPEND _common --gcov-executable "${GCOV}")
endif()

# ---- Pass 1: the portable code ------------------------------------------------------------------
# The two format libraries, the engine and its runner, the media tree, the samples, the payload
# writer, and the platform interfaces. The OS fragments under libs/platform are not measured (each
# host builds a different set, and the conformance suite is what holds them to the contract), and
# neither are the Qt windows: QML cannot be measured, which is why this gate is the libraries' and
# not a blended figure.
execute_process(
    COMMAND "${GCOVR}" ${_common}
        --filter "${REPO_ROOT}/(libs/(arinc665|arinc615a|arinc615a_mini|arinc615a_runner|media_tree|samples|platform/(interfaces|detail))|apps/write_payloads)/.*"
        --fail-under-line "${LINE_THRESHOLD}"
        --fail-under-branch "${BRANCH_THRESHOLD}"
        --html-details "${_output}/index.html"
        --json-summary "${_output}/summary.json"
        --cobertura "${_output}/cobertura.xml"
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

# ---- Pass 2: the applications' window-free logic -----------------------------------------------
# The cores of the three applications hold the logic the windows are thin layers over. These
# directories are plain C++ (with Qt types), and the application tests, the UI suite and the
# generator and mock target tests drive them. Qt's generated files (moc_*.cpp, *_autogen) are not
# source and are left out. The floor is a little under what the suite measures today; raise it when
# the measure rises.
set(_app_output "${_output}/apps")
file(MAKE_DIRECTORY "${_app_output}")
execute_process(
    COMMAND "${GCOVR}" ${_common}
        --filter "${REPO_ROOT}/apps/(app|mock_target|media_gen)/core/.*"
        --exclude ".*/moc_[^/]*"
        --exclude ".*/mocs_compilation[^/]*"
        --exclude ".*_autogen/.*"
        --fail-under-line "${APP_LINE_THRESHOLD}"
        --fail-under-branch "${APP_BRANCH_THRESHOLD}"
        --html-details "${_app_output}/index.html"
        --json-summary "${_app_output}/summary.json"
        --cobertura "${_app_output}/cobertura.xml"
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE _app_result)

if(NOT _app_result EQUAL 0)
    message(FATAL_ERROR
        "The applications' logic is below its minimum (${APP_LINE_THRESHOLD}% lines, "
        "${APP_BRANCH_THRESHOLD}% branches), or gcovr failed (${_app_result}). Is the coverage "
        "build configured with the applications (Qt found, ADL_BUILD_APP on)? "
        "The report is in ${_app_output}.")
endif()

# As above: a pass that measured nothing passes any threshold.
file(READ "${_app_output}/summary.json" _app_summary)
string(JSON _app_line_total GET "${_app_summary}" line_total)
string(JSON _app_branch_total GET "${_app_summary}" branch_total)
if(_app_line_total LESS ${APP_MIN_LINES} OR _app_branch_total LESS ${APP_MIN_BRANCHES})
    message(FATAL_ERROR
        "The applications' pass measured only ${_app_line_total} lines and ${_app_branch_total} "
        "branches (at least ${APP_MIN_LINES} and ${APP_MIN_BRANCHES} are expected): the coverage "
        "build was configured without the applications, the tests did not run, or the filter "
        "matched too little. The report is in ${_app_output}.")
endif()

message(STATUS "The applications' logic meets its minimum: ${APP_LINE_THRESHOLD}% lines and "
               "${APP_BRANCH_THRESHOLD}% branches. Report: ${_app_output}/index.html")
