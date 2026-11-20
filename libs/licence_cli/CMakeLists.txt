# SPDX-License-Identifier: GPL-3.0-or-later
# The commands of adl_licence, the program that shows, imports, removes and checks the licence on this computer
# (docs/user-guide/command-line.md): `run` takes a command line and an Environment (the files, the folders, the
# time, the computer's name and the keys) and writes to two streams, so that every state and every failure is
# tried in a test. SystemEnvironment fills the Environment in from this computer. The program itself is
# apps/licence.
#
# It links adl::licence, which verifies and judges, and not adl::licence_sign or adl::licence_test_keys: no
# signing code and no test key is in a program that ships (tools/release/check_package_contents.py would refuse
# the package). Not part of the installed adl package: nothing outside this repository builds on it.

add_library(adl_licence_cli STATIC
    src/commands.cpp
    src/licence_cli.cpp
    src/messages.cpp
    src/system.cpp
    src/text.cpp)
add_library(adl::licence_cli ALIAS adl_licence_cli)

target_include_directories(adl_licence_cli
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(adl_licence_cli PUBLIC cxx_std_23)
target_link_libraries(adl_licence_cli
    PUBLIC
        adl::licence
        adl::licence_platform
        adl::platform
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

# The libFuzzer harness for the command line and the file it names (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
