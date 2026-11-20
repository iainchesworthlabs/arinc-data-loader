# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
# The owner's licence tool: makes a signing key, issues a licence file, checks one and writes a revocation list
# (docs/licence-owner-tool.md). It is a program for the owner's own computer and for the tests, so it is
# built with the other tools and is never installed, packed or exported: no install rule names it, and
# tools/release/check_package_contents.py fails a package that holds it, a signing library or a key file.
#
#   adl_licence_owner_core  the commands, over adl::licence and the standard library (tests link it too)
#   adl_licence_owner       the program: the core and the entry point of the platform
#
# It includes nothing of this repository but adl::licence, so it keeps the licence library's terms
# (GPL-3.0-or-later OR BSD-2-Clause). Its arguments and file names are UTF-8: the entry point is the only part
# that differs between systems (see os/), and CMake picks it by the platform's name, as it picks libs/platform.

add_library(adl_licence_owner_core STATIC
    src/commands.cpp
    src/files.cpp
    src/messages.cpp
    src/options.cpp
    src/owner.cpp
    src/public_key.cpp
    src/report.cpp
    src/signing.cpp
    src/utf8.cpp)
target_include_directories(adl_licence_owner_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(adl_licence_owner_core PUBLIC cxx_std_23)
target_link_libraries(adl_licence_owner_core
    PUBLIC
        adl::licence
        adl::licence_sign
        adl::licence_test_keys
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The entry point: os/<platform>.cmake names it. A platform with no file there gets the one that reads argv.
set(ADL_LICENCE_OWNER_ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/os/posix/entry.cpp")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/os/${ADL_PLATFORM}.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/os/${ADL_PLATFORM}.cmake")
endif()

add_executable(adl_licence_owner "${ADL_LICENCE_OWNER_ENTRY}")
target_link_libraries(adl_licence_owner
    PRIVATE
        adl_licence_owner_core
        adl::warnings
        adl::sanitizers
        adl::coverage)
adl_deploy_sanitizer_runtime(adl_licence_owner)

# The libFuzzer harness for what the tool reads that is not a licence file (a public key file, a command line).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
