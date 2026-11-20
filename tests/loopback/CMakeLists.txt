# SPDX-License-Identifier: GPL-3.0-or-later
# The loopback integration test (tests/loopback): the loader's real 615A engine (DataLoader
# driven by Runner over the real platform host) against the mock target's engine (MockEngine
# driven by run() over its RealTargetLink), end to end over real UDP sockets on 127.0.0.1: the
# first run of a whole 615A operation through real sockets in this repository. The engine's own
# tests run the same operations on the fake in-memory platform; the conformance suite checks the
# platform's sockets; this test is where the two meet.
#
# The test uses no Qt: the engine is run on a plain std::thread, and the engine and its link are
# the Qt-free library libs/mock_target_core, so this builds without the applications.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(adl_loopback_test
    loopback_test.cpp)

target_include_directories(adl_loopback_test PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")

target_link_libraries(adl_loopback_test
    PRIVATE
        adl::mock_target_core
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::platform
        adl::samples
        Catch2::Catch2WithMain
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_loopback_test)

# One test that runs every scenario; a hang fails after five minutes instead of holding the
# run, and the label lets a person (or CI) pick the real-socket test out of the suite.
add_test(NAME loopback.integration COMMAND adl_loopback_test)
# The engine of the target writes from ports of its own, from 40000 up, on the address it listens on; two
# targets on one address at once would take them from each other, so the tests that run a real target do
# not overlap (ctest -j). tests/mock_cli has the others.
set_tests_properties(loopback.integration PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK mock_target_transfer_ports
    LABELS "integration;system")

# adl_load's whole commands over the same sockets (tests/loopback/load_command_test.cpp): the program's run
# function on the real host against the mock target's engine, which listens on port 59 of the loopback
# address because that is where the loader sends. An account that may not bind port 59 skips them (exit code
# 4). Only one test holds the sockets at a time: the loopback test has the same lock.
add_executable(adl_load_system_test
    load_command_test.cpp)

target_include_directories(adl_load_system_test PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")

target_link_libraries(adl_load_system_test
    PRIVATE
        adl_load_cli
        adl::mock_core
        adl::arinc615a
        adl::arinc665
        adl::media_tree
        adl::platform
        adl::samples
        Catch2::Catch2WithMain
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_load_system_test)

add_test(NAME load.system COMMAND adl_load_system_test)
set_tests_properties(load.system PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK mock_target_transfer_ports
    SKIP_RETURN_CODE 4
    LABELS "system")
