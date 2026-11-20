# SPDX-License-Identifier: GPL-3.0-or-later
# adl_make_set: the media set generator as a command, with no window and no Qt, for a build that makes media
# sets from a description file, and for a program that needs to know what a media set holds (docs/user-guide/
# command-line.md). It is libs/set_maker with a description file, a command line and one line of output for
# each event. The program is a function in a library of its own (adl_make_set_cli, not installed) so that the
# tests can run it on temporary folders.
#
# Desktop only: the description is read with the licence library's strict JSON reader, which only a desktop
# build has (the root CMakeLists adds this directory next to libs/licence).

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/adl/make_set/version.hpp" @ONLY)

add_library(adl_make_set_cli STATIC
    include/adl/make_set/command.hpp
    include/adl/make_set/description.hpp
    include/adl/make_set/inspection.hpp
    include/adl/make_set/json.hpp
    include/adl/make_set/options.hpp
    src/command.cpp
    src/description.cpp
    src/inspection.cpp
    src/json.cpp
    src/options.cpp)
add_library(adl::make_set_cli ALIAS adl_make_set_cli)

target_include_directories(adl_make_set_cli PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(adl_make_set_cli PUBLIC cxx_std_23)
target_link_libraries(adl_make_set_cli
    PUBLIC
        adl::set_maker
        adl::licence
        adl::loader_core
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

add_executable(adl_make_set main.cpp)
target_link_libraries(adl_make_set
    PRIVATE
        adl_make_set_cli
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_make_set)

# The libFuzzer harness for the description file, when fuzzing is on (cmake/Fuzzing.cmake). It is kept in
# libs/set_maker/fuzz, since the fuzzing workflows look for harnesses and seeds in libs/*/fuzz, and added
# from here, where the reader it fuzzes is.
if(ADL_ENABLE_FUZZING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/libs/set_maker/fuzz" fuzz)
endif()

# A command-line program for builds and rigs: it ships with the programs, as adl_write_payloads does.
if(ADL_PLATFORM_DESKTOP)
    install(TARGETS adl_make_set
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
endif()
