# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's scenarios: a file that says which fault happens when, so that a run with faults can be repeated.
# The reading and writing of the file (over the shared reader of libs/scenario_base) and the player, which turns a
# scenario into the FaultPlan that the engine of libs/mock_target_core asks at each request, status file and time.
# adl_mock and the mock target's window both take a scenario through it.
#
# A desktop library, because the shared reader reads JSON with the licence library, which exists only there. Not
# installed and not part of the adl package.

add_library(adl_mock_scenario STATIC
    include/adl/mock/scenario.hpp
    include/adl/mock/scenario_file.hpp
    include/adl/mock/scenario_player.hpp
    src/scenario_file.cpp
    src/scenario_player.cpp)
add_library(adl::mock_scenario ALIAS adl_mock_scenario)

target_include_directories(adl_mock_scenario PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_mock_scenario PUBLIC cxx_std_23)
target_link_libraries(adl_mock_scenario
    PUBLIC
        adl::mock_target_core
        adl::scenario_base
    PRIVATE
        adl::arinc615a
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The libFuzzer harness for the file, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
