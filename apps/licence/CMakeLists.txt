# SPDX-License-Identifier: GPL-3.0-or-later
# adl_licence: shows, imports, removes and checks the licence on this computer, with no window and no Qt
# (docs/user-guide/command-line.md). The commands are libs/licence_cli; this is the program around them: its
# arguments, the one environment variable it reads (adl::licence_platform::environment_variable), and the time
# and the version of its build.
#
#   adl_licence_program      the program, installed as adl_licence (the target of the licence library is
#                            adl_licence, so the program has another name for CMake and not for the person)
#
# It has no signing code and no test key: it links adl::licence_cli, which links adl::licence and nothing of
# adl::licence_sign or adl::licence_test_keys (tools/release/check_package_contents.py refuses a package that
# holds either).

add_executable(adl_licence_program main.cpp)
set_target_properties(adl_licence_program PROPERTIES OUTPUT_NAME adl_licence)
# The version and the time of the commit the program was built from are the ones cmake/AdlUi.cmake reads from
# the repository for the windows programs (a header with constants only; it needs no Qt).
target_include_directories(adl_licence_program PRIVATE "${ADL_UI_GENERATED_INCLUDE_DIR}")
target_link_libraries(adl_licence_program
    PRIVATE
        adl::licence_cli
        adl::licence_platform
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_licence_program)

# A command-line program for the person who has a licence file: it ships with the programs.
install(TARGETS adl_licence_program
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
