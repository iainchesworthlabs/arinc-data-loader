# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's scenarios (libs/mock_scenario): the file and its sentences, the player, the seeds of the file's
# fuzzer replayed under the ordinary compilers, and three scenarios run against the loader's real engine on the fake
# network, whose events are the same every time and on every system. No Qt.

find_package(Catch2 3 CONFIG REQUIRED)

# The harness of the fuzzer (libs/mock_scenario/fuzz) is compiled into this program as it is, so that its seeds are
# replayed under every compiler the tests build with (fuzz_replay_test.cpp).
add_executable(adl_mock_scenario_tests
    fuzz_replay_test.cpp
    scenario_file_test.cpp
    scenario_player_test.cpp
    "${PROJECT_SOURCE_DIR}/libs/mock_scenario/fuzz/scenario.cpp")
target_include_directories(adl_mock_scenario_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")
target_compile_definitions(adl_mock_scenario_tests PRIVATE
    ADL_MOCK_SCENARIO_FUZZ_DIR="${PROJECT_SOURCE_DIR}/libs/mock_scenario/fuzz")
target_link_libraries(adl_mock_scenario_tests
    PRIVATE
        adl::mock_scenario
        adl::platform
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_mock_scenario_tests)

add_test(NAME mock.scenario COMMAND adl_mock_scenario_tests)
set_tests_properties(mock.scenario PROPERTIES
    TIMEOUT 120
    LABELS "unit")

# Three scenarios against the loader's real engine (DataLoader and Runner over a fake host) on the fake network: the
# clock is the fake world's, so the events come with their times and are the same on every run and every system.
add_executable(adl_mock_scenario_engine_tests
    engine_test.cpp)
target_include_directories(adl_mock_scenario_engine_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/mock_target_core"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_scenario_engine_tests
    PRIVATE
        adl::mock_scenario
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_mock_scenario_engine_tests)

add_test(NAME mock.scenario.engine COMMAND adl_mock_scenario_engine_tests)
set_tests_properties(mock.scenario.engine PROPERTIES
    TIMEOUT 120
    LABELS "integration")
