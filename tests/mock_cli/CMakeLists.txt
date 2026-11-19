# SPDX-License-Identifier: GPL-3.0-or-later
# The tests of adl_mock (apps/mock), the mock target as a command: its JSON writer, its events and its
# options, without a network; the program's own run function in a thread with the loader's real engine
# run against it over real sockets on 127.0.0.1; and the built program started as a process. No Qt.

find_package(Catch2 3 CONFIG REQUIRED)

add_executable(adl_mock_cli_tests
    events_test.cpp
    json_read.hpp
    json_test.cpp
    options_test.cpp)
target_link_libraries(adl_mock_cli_tests
    PRIVATE
        adl::mock_cli
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_mock_cli_tests)

add_test(NAME mock.cli COMMAND adl_mock_cli_tests)
set_tests_properties(mock.cli PROPERTIES
    TIMEOUT 120
    LABELS "unit")

# The program's run function, with the loader against it. Every port is a high one, apart from those
# of the loopback test (30901 to 30961), so that the two can run at the same time.
add_executable(adl_mock_command_test
    command_test.cpp
    json_read.hpp)
target_include_directories(adl_mock_command_test PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_command_test
    PRIVATE
        adl::mock_cli
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::platform
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_mock_command_test)

add_test(NAME mock.command COMMAND adl_mock_command_test)
set_tests_properties(mock.command PROPERTIES
    TIMEOUT 300
    LABELS "system")

# The built program, started as a process: what it says about itself, that wrong options are refused with
# exit code 2, and that it runs for the time it is told and writes its events.
add_test(NAME tools.adl_mock.version COMMAND adl_mock --version)
set_tests_properties(tools.adl_mock.version PROPERTIES
    PASS_REGULAR_EXPRESSION "^adl_mock [0-9]+[.][0-9]+[.][0-9]+[^ ]* events 1"
    LABELS "system")

add_test(NAME tools.adl_mock.help COMMAND adl_mock --help)
set_tests_properties(tools.adl_mock.help PROPERTIES
    PASS_REGULAR_EXPRESSION "^usage: adl_mock"
    LABELS "system")

add_test(NAME tools.adl_mock.wrong_usage COMMAND adl_mock --no-such-option)
set_tests_properties(tools.adl_mock.wrong_usage PROPERTIES
    WILL_FAIL TRUE
    LABELS "system")

add_test(NAME tools.adl_mock.runs_for_a_second
    COMMAND adl_mock --seconds 1 --json --tftp-port 31975 --find-port 31977)
set_tests_properties(tools.adl_mock.runs_for_a_second PROPERTIES
    PASS_REGULAR_EXPRESSION "\"event\":\"started\".*\"event\":\"stopped\",\"t\":[0-9.]+,\"reason\":\"seconds\""
    TIMEOUT 60
    LABELS "system")
