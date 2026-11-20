# SPDX-License-Identifier: GPL-3.0-or-later
# The tests of the shared part of the scenario files (libs/scenario_base): the random numbers pinned to the
# algorithm's own outputs, the shared reader and every sentence it says, and the seeds of its fuzzer replayed under
# the ordinary compilers. No Qt, no network.

find_package(Catch2 3 CONFIG REQUIRED)

# The harness of the fuzzer (libs/scenario_base/fuzz) is compiled into this program as it is, so that its seeds are
# replayed under every compiler the tests build with (fuzz_replay_test.cpp).
add_executable(adl_scenario_base_tests
    envelope_test.cpp
    fuzz_replay_test.cpp
    random_test.cpp
    "${PROJECT_SOURCE_DIR}/libs/scenario_base/fuzz/envelope.cpp")
target_include_directories(adl_scenario_base_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")
target_compile_definitions(adl_scenario_base_tests PRIVATE
    ADL_SCENARIO_FUZZ_DIR="${PROJECT_SOURCE_DIR}/libs/scenario_base/fuzz")
target_link_libraries(adl_scenario_base_tests
    PRIVATE
        adl::scenario_base
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_scenario_base_tests)

add_test(NAME scenario.base COMMAND adl_scenario_base_tests)
set_tests_properties(scenario.base PROPERTIES
    TIMEOUT 120
    LABELS "unit")
