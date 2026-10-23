# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# Tracy.cmake
#
# Defines an INTERFACE target `adl::tracy` that, when ADL_ENABLE_TRACY is on,
# links Tracy's client library. Off by default, matching Coverage.cmake's
# shape, so a normal build does not need Tracy present.
#
# Tracy is an OPT-IN vcpkg manifest feature ("profiling" in vcpkg.json, not a
# base dependency); the top-level CMakeLists.txt adds it to
# VCPKG_MANIFEST_FEATURES when this option is on, so one switch is the whole
# story:
#
#   cmake --preset windows-msvc-release -DADL_ENABLE_TRACY=ON
#
# The feature also builds Tracy's command-line tools for the HOST (capture and
# csvexport), so a trace can be recorded and dumped headlessly, without the GUI
# profiler.
#
# The instrumentation itself lives in the generated <adl/profiling.hpp> (below)
# and the zones in the libraries and the application; this file is the switch,
# the dependency and the header.
#
# Nothing here is for shipping: a Tracy client opens a TCP listener and answers
# UDP discovery broadcasts, which a released binary - and anything that runs on
# an aircraft network - must not do. Release presets leave this off.
# ---------------------------------------------------------------------------

option(ADL_ENABLE_TRACY "Enable Tracy profiler instrumentation" OFF)

add_library(adl_tracy INTERFACE)
add_library(adl::tracy ALIAS adl_tracy)

# <adl/profiling.hpp> is generated, one flavour for each state of the switch: the adl::profiling
# names call Tracy when profiling is on, and do nothing when it is off. The repository forbids #if
# and #define in its own C++, so the choice is made in a build artifact; every instrumented file
# includes this header and uses only those names.
set(ADL_PROFILING_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/profiling/profiling_noop.hpp.in")
if(ADL_ENABLE_TRACY)
    set(ADL_PROFILING_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/profiling/profiling_tracy.hpp.in")
endif()
set(ADL_PROFILING_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${ADL_PROFILING_INCLUDE_DIR}/adl")
configure_file("${ADL_PROFILING_TEMPLATE}" "${ADL_PROFILING_INCLUDE_DIR}/adl/profiling.hpp"
    @ONLY)
target_include_directories(adl_tracy INTERFACE "${ADL_PROFILING_INCLUDE_DIR}")

if(ADL_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(adl_tracy INTERFACE Tracy::TracyClient)

    # Not an error: someone chasing a bug in the profiling build itself wants
    # exactly this. But a number taken from an AddressSanitizer or coverage
    # build is not a number about the program, and nothing else will say so.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR ADL_ENABLE_COVERAGE)
        message(WARNING
            "ADL_ENABLE_TRACY is on for a Debug or coverage build. "
            "Sanitizer and coverage instrumentation dominate the timings Tracy "
            "reports; use a Release or RelWithDebInfo build for numbers worth "
            "reading.")
    endif()
endif()
