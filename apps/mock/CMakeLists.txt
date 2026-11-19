# SPDX-License-Identifier: GPL-3.0-or-later
# adl_mock: the mock ARINC 615A target as a command, with no window and no Qt, for test rigs and CI
# jobs (docs/mock-target.md, "The command"). It is the engine of libs/mock_target_core with a command
# line and one line of output for each event. The program is a function in a library of its own
# (adl_mock_cli, not installed) so that the tests can run it in a thread and put the loader against it.

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/adl/mock/cli/version.hpp" @ONLY)

add_library(adl_mock_cli STATIC
    include/adl/mock/cli/command.hpp
    include/adl/mock/cli/events.hpp
    include/adl/mock/cli/json.hpp
    include/adl/mock/cli/options.hpp
    src/command.cpp
    src/events.cpp
    src/json.cpp
    src/options.cpp)
add_library(adl::mock_cli ALIAS adl_mock_cli)

target_include_directories(adl_mock_cli PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(adl_mock_cli PUBLIC cxx_std_23)
target_link_libraries(adl_mock_cli
    PUBLIC
        adl::mock_target_core
        adl::arinc615a
        adl::arinc665
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

add_executable(adl_mock main.cpp)
target_link_libraries(adl_mock
    PRIVATE
        adl_mock_cli
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_mock)

# A command-line program for rigs and CI jobs: it ships with the programs, as adl_write_payloads does.
if(ADL_PLATFORM_DESKTOP)
    install(TARGETS adl_mock
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
endif()
