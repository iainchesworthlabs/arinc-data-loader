# SPDX-License-Identifier: GPL-3.0-or-later
# adl_load: one ARINC 615A operation on one target, with no window and no Qt, for programs and
# scripts that load, read or download equipment (docs/load-command.md). The program is a thin main
# over adl_load_cli, a static library of its own that is neither exported nor installed: the options,
# the events and the run function live there so that the tests can run a whole command on the fake
# platform.

configure_file(program.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/adl/load/program.hpp" @ONLY)

add_library(adl_load_cli STATIC
    src/events.cpp
    src/job.cpp
    src/json_line.cpp
    src/operation.cpp
    src/options.cpp
    src/record_file.cpp
    src/run.cpp)

target_include_directories(adl_load_cli
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(adl_load_cli PUBLIC cxx_std_23)
target_link_libraries(adl_load_cli
    PUBLIC
        adl::arinc615a
        adl::arinc615a_runner
        adl::loader_core
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

add_executable(adl_load main.cpp)
target_link_libraries(adl_load
    PRIVATE
        adl_load_cli
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_load)

# The libFuzzer harness for the options, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
