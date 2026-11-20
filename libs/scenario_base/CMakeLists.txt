# SPDX-License-Identifier: GPL-3.0-or-later
# What the scenario files have in common: the random numbers (SplitMix64, the
# same on every system, so that a seed repeats a run with faults) and the reading of the top level of a file and of
# the shape of its steps, with the licence library's strict JSON reader. The mock target's scenario (libs/mock_scenario)
# and the loader's are built on it and add their own actions.
#
# A desktop library, like the licence library it reads JSON with (a target build has neither). Not installed and not
# part of the adl package.

add_library(adl_scenario_base STATIC
    include/adl/scenario/envelope.hpp
    include/adl/scenario/random.hpp
    src/envelope.cpp)
add_library(adl::scenario_base ALIAS adl_scenario_base)

target_include_directories(adl_scenario_base PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_scenario_base PUBLIC cxx_std_23)
target_link_libraries(adl_scenario_base
    PUBLIC
        adl::licence
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The libFuzzer harness for the shared reader, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
