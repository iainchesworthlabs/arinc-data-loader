# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# StaticAnalysis.cmake
#
# The two build modes that run an analyzer over first-party code as part of the
# build, so a finding is a failed build rather than a report nobody reads:
#
#   ADL_ENABLE_CLANG_TIDY     runs clang-tidy for every C++ translation unit
#                             under libs/ and apps/ (the tests clear the launcher), with
#                             .clang-tidy and warnings as errors. Used by the
#                             linux-gcc-tidy preset and the nightly
#                             static-analysis workflow.
#
#   ADL_ENABLE_MSVC_ANALYSIS  adds MSVC Code Analysis (/analyze) to the warning
#                             interface, so every first-party target is
#                             analyzed. Used by the windows-msvc-analyze preset.
#
# Both are off by default: the ordinary build is the compiler's warnings only,
# and the analyzers are slow enough to belong to a nightly run, not to every
# commit (docs/static-analysis.md).
# ---------------------------------------------------------------------------

option(ADL_ENABLE_CLANG_TIDY
    "Run clang-tidy over first-party targets during the build, warnings as errors" OFF)
option(ADL_ENABLE_MSVC_ANALYSIS
    "Run MSVC Code Analysis (/analyze) over first-party targets" OFF)

if(ADL_ENABLE_CLANG_TIDY)
    find_program(ADL_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    # CMake runs this as a compiler launcher. --warnings-as-errors=* is what
    # makes the build red: .clang-tidy itself keeps WarningsAsErrors empty so an
    # editor does not turn every hint into a failed save.
    set(CMAKE_CXX_CLANG_TIDY "${ADL_CLANG_TIDY_EXECUTABLE};--warnings-as-errors=*")
    message(STATUS "clang-tidy: ${ADL_CLANG_TIDY_EXECUTABLE} (warnings are errors)")
endif()

if(ADL_ENABLE_MSVC_ANALYSIS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "ADL_ENABLE_MSVC_ANALYSIS needs MSVC's cl.exe; "
            "${CMAKE_CXX_COMPILER_ID} was selected")
    endif()
    # /analyze:external- keeps the analyzer out of the SDK and Qt headers. It
    # needs those headers passed as /external:I, which the imported targets do;
    # C6101 from the SDK's inline WSAGetIPUserMtu is the one that still leaks
    # through, and libs/platform/windows/platform.cmake disables it for the one
    # file that includes that header.
    target_compile_options(adl_warnings INTERFACE /analyze /analyze:external-)
    message(STATUS "MSVC Code Analysis: /analyze (warnings are errors)")
endif()
