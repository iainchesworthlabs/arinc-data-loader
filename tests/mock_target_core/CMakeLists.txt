# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's engine (libs/mock_target_core), headless: whole operations of the loader's
# real engine (DataLoader and Runner over the fake platform, on one fake network) against the
# engine over a fake link, and the injection controls. No Qt, so it builds and runs without the
# applications. The mock target's model and window have tests of their own in
# apps/mock_target/tests, which use the fake link and the loader rig kept here.

add_executable(adl_mock_tests
    engine_test.cpp
    fake_link.hpp
    link_test.cpp
    loader_rig.hpp)
target_include_directories(adl_mock_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_tests
    PRIVATE
        adl::mock_target_core
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_mock_tests)

add_test(NAME mock.engine COMMAND adl_mock_tests)
set_tests_properties(mock.engine PROPERTIES
    TIMEOUT 120
    LABELS "integration")

# The folders the target reads and writes, and what the operator's settings become: plain functions
# over a temporary directory, no network.
add_executable(adl_mock_core_tests
    settings_test.cpp
    storage_test.cpp)
target_include_directories(adl_mock_core_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_core_tests
    PRIVATE
        adl::mock_target_core
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_mock_core_tests)

add_test(NAME mock.core COMMAND adl_mock_core_tests)
set_tests_properties(mock.core PROPERTIES
    TIMEOUT 120
    LABELS "unit")
