# SPDX-License-Identifier: GPL-3.0-or-later
# The tests of adl_licence (libs/licence_cli, apps/licence). The commands run in process over licence files held
# in memory, with the published test key of RFC 8032 and a time of the test's own, for every state a licence can
# be in (unit); over this computer's own file system in a scratch folder, with names outside the code page
# (integration); and the built program is started as a process (system). The program that ships trusts no key
# yet, so the process tests of a licence use adl_licence_test_program: the same commands with the test key, a
# clock and a computer name the test sets, and never installed or packed.

add_executable(adl_licence_cli_unit_tests
    cli_test.cpp
    manifest_test.cpp
    text_test.cpp)

target_include_directories(adl_licence_cli_unit_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/licence_cli/src"
    "${PROJECT_SOURCE_DIR}/tests/licence")

# Where the test licences and their manifest are.
target_compile_definitions(adl_licence_cli_unit_tests PRIVATE
    ADL_LICENCE_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/licence/data")

target_link_libraries(adl_licence_cli_unit_tests
    PRIVATE
        adl::licence_cli
        adl::licence_test_keys
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_licence_cli_unit_tests)

catch_discover_tests(adl_licence_cli_unit_tests PROPERTIES LABELS "unit")

add_executable(adl_licence_cli_integration_tests
    real_files_test.cpp)

target_include_directories(adl_licence_cli_integration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/licence"
    "${PROJECT_SOURCE_DIR}/tests/support")

target_link_libraries(adl_licence_cli_integration_tests
    PRIVATE
        adl::licence_cli
        adl::licence_test_keys
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_licence_cli_integration_tests)

catch_discover_tests(adl_licence_cli_integration_tests PROPERTIES LABELS "integration")

# The commands with the test key, for the process tests below. Tests only: it is not installed, and
# tools/release/check_package_contents.py fails a package that holds it.
add_executable(adl_licence_test_program test_program/main.cpp)
target_link_libraries(adl_licence_test_program
    PRIVATE
        adl::licence_cli
        adl::licence_platform
        adl::licence_test_keys
        adl::warnings
        adl::sanitizers
        adl::coverage)
adl_deploy_sanitizer_runtime(adl_licence_test_program)

# As a process: the program that ships (what it says without a key, usage, names outside the code page) and the
# test program (a licence imported, shown, checked and removed in each state).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME licence.cli
        COMMAND "${Python3_EXECUTABLE}" -X utf8 -I "${CMAKE_CURRENT_SOURCE_DIR}/test_adl_licence.py"
            --program "$<TARGET_FILE:adl_licence_program>"
            --test-program "$<TARGET_FILE:adl_licence_test_program>"
            --data "${PROJECT_SOURCE_DIR}/tests/licence/data"
            --scratch "${CMAKE_CURRENT_BINARY_DIR}/scratch")
    set_tests_properties(licence.cli PROPERTIES TIMEOUT 300 LABELS "system")
endif()
