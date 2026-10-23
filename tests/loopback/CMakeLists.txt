# SPDX-License-Identifier: GPL-3.0-or-later
# The loopback integration test (tests/loopback): the loader's real 615A engine (DataLoader
# driven by Runner over the real platform host) against the mock target's engine (MockEngine
# driven by run() over its RealTargetLink), end to end over real UDP sockets on 127.0.0.1: the
# first run of a whole 615A operation through real sockets in this repository. The engine's own
# tests run the same operations on the fake in-memory platform; the conformance suite checks the
# platform's sockets; this test is where the two meet.
#
# The test's own code uses no Qt (the engine is run on a plain std::thread), but it links
# adl::mock_core, which is only built with the application (ADL_BUILD_APP), and whose Qt6::Core
# dependency it therefore carries transitively. None of the test's code pulls in a Qt symbol,
# so there is nothing Qt to deploy. Wire this subdirectory inside the ADL_BUILD_APP block of
# tests/CMakeLists.txt, next to the mock target's tests.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(adl_loopback_test
    loopback_test.cpp)

target_include_directories(adl_loopback_test PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")

target_link_libraries(adl_loopback_test
    PRIVATE
        adl::mock_core
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
set_tests_properties(loopback.integration PROPERTIES
    TIMEOUT 300
    LABELS "integration;system")
