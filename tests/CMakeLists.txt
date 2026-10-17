find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(adl_unit_tests
    unit/batch_file_test.cpp
    unit/check_value_type_test.cpp
    unit/compatibility_test.cpp
    unit/crc_test.cpp
    unit/digest_test.cpp
    unit/golden_test.cpp
    unit/header_file_test.cpp
    unit/integrity_checker_test.cpp
    unit/limits_test.cpp
    unit/load_test.cpp
    unit/media_files_test.cpp
    unit/media_label_test.cpp
    unit/media_set_test.cpp
    unit/media_tree_test.cpp
    unit/naming_test.cpp
    unit/part_number_test.cpp
    unit/paths_test.cpp
    unit/robustness_test.cpp)

target_include_directories(adl_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/golden")

target_link_libraries(adl_unit_tests
    PRIVATE
        adl::arinc665
        adl::media_tree
        adl::platform
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_unit_tests)

catch_discover_tests(adl_unit_tests)

# Every platform fragment satisfies the platform contract, on whichever host
# this runs (a build only includes the one fragment for its own platform).
add_test(NAME platform.fragments
    COMMAND "${CMAKE_COMMAND}"
        "-DREPO_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/platform/check_fragments.cmake")

# No preprocessor conditionals or defines, no OS headers outside the platform
# section, no OS tests in CMake outside the selector. CI runs it as its own job
# too; this is the same check, so a developer sees it fail locally.
find_program(ADL_PWSH NAMES pwsh)
if(ADL_PWSH)
    add_test(NAME platform.isolation
        COMMAND "${ADL_PWSH}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/checks/check_platform_isolation.ps1"
            -Root "${PROJECT_SOURCE_DIR}")
endif()

# The program that writes the sample media sets: it runs on a directory of its
# own (emptied first, since it never overwrites), and refuses what it cannot use.
add_test(NAME tools.write_payloads.clean
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads.clean PROPERTIES FIXTURES_SETUP write_payloads_output)

add_test(NAME tools.write_payloads
    COMMAND adl_write_payloads "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads PROPERTIES FIXTURES_REQUIRED write_payloads_output)

add_test(NAME tools.write_payloads.unknown_scenario
    COMMAND adl_write_payloads --scenario no-such-scenario "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads.unknown_scenario PROPERTIES WILL_FAIL TRUE)

add_test(NAME tools.write_payloads.no_output_directory COMMAND adl_write_payloads)
set_tests_properties(tools.write_payloads.no_output_directory PROPERTIES WILL_FAIL TRUE)

add_test(NAME tools.write_payloads.list COMMAND adl_write_payloads --list)
