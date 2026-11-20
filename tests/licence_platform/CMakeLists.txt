# SPDX-License-Identifier: GPL-3.0-or-later
# The tests of libs/licence_platform: the store's file interface over the platform's FileSystem and the folders
# the product keeps its licences in. The pure part (the folders) is a unit test. The rest runs the store over
# the fake file system, and over the real one in a scratch folder, so it is an integration test. They sign with
# the published test key of RFC 8032 (tests/licence/support.hpp) and nothing else.

add_executable(adl_licence_platform_unit_tests
    locations_test.cpp)

target_link_libraries(adl_licence_platform_unit_tests
    PRIVATE
        adl::licence_platform
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_licence_platform_unit_tests)

catch_discover_tests(adl_licence_platform_unit_tests PROPERTIES LABELS "unit")

add_executable(adl_licence_platform_integration_tests
    files_test.cpp
    real_files_test.cpp
    store_over_platform_test.cpp)

target_include_directories(adl_licence_platform_integration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/licence"
    "${PROJECT_SOURCE_DIR}/tests/support")

target_link_libraries(adl_licence_platform_integration_tests
    PRIVATE
        adl_fake_platform
        adl::licence_platform
        adl::licence_test_keys
        adl::platform
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_licence_platform_integration_tests)

catch_discover_tests(adl_licence_platform_integration_tests PROPERTIES LABELS "integration")
