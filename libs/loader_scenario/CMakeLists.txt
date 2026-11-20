# SPDX-License-Identifier: GPL-3.0-or-later
# Faults on purpose in the data loader: a scenario (an ordered list of steps that say when the loader is to do
# something wrong and what), the reader of the file that holds one, and the Player that carries it out through
# the 615A engine's FaultHook. No Qt, no platform code: the player is driven by the engine's own calls, and the
# reader takes text. The strict JSON reader is the licence library's (adl/licence/json.hpp).
#
# Not part of the installed adl package: a program that wants to run a scenario is in this repository (adl_load).

add_library(adl_loader_scenario STATIC
    src/player.cpp
    src/scenario.cpp)
add_library(adl::loader_scenario ALIAS adl_loader_scenario)

target_include_directories(adl_loader_scenario PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(adl_loader_scenario PUBLIC cxx_std_23)
target_link_libraries(adl_loader_scenario
    PUBLIC
        adl::arinc615a
    PRIVATE
        adl::licence
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)
