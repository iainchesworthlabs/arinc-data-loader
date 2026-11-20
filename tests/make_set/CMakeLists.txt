# SPDX-License-Identifier: GPL-3.0-or-later
# adl_make_set (apps/make_set): the description file, the JSON it writes and its command line, the program as a
# function run on temporary folders with the loader's own check to say that what it writes is a media set, and the
# seeds of its fuzz harnesses. No Qt, no network. The logic under it has tests of its own in tests/set_maker.

add_executable(adl_make_set_tests
    command_test.cpp
    description_test.cpp
    docs_test.cpp
    fuzz_replay_test.cpp
    json_options_test.cpp)
target_include_directories(adl_make_set_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/set_maker"
    "${PROJECT_SOURCE_DIR}/libs/set_maker/fuzz"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_compile_definitions(adl_make_set_tests PRIVATE
    ADL_FILE_REFERENCE="${PROJECT_SOURCE_DIR}/docs/file-reference.md"
    ADL_MAKE_SET_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    ADL_SET_MAKER_FUZZ_DIR="${PROJECT_SOURCE_DIR}/libs/set_maker/fuzz")
target_link_libraries(adl_make_set_tests
    PRIVATE
        adl::make_set_cli
        adl::set_maker
        adl::loader_core
        adl::media_tree
        adl::arinc665
        adl::licence
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_make_set_tests)

# The description, the JSON and the command line: plain functions. The test of the limits reads a description of
# 22000 files, which a Debug build with the sanitizers does slowly (std::filesystem::path above all), so they have
# longer than the 120 seconds of most tests.
foreach(tag description cli fuzz docs)
    add_test(NAME make_set.${tag} COMMAND adl_make_set_tests "[${tag}]")
    set_tests_properties(make_set.${tag} PROPERTIES
        TIMEOUT 300
        LABELS "unit")
endforeach()

# The program's own function on temporary folders: a set made, read back by the loader's own check, and the
# exit code and the events of everything that can go wrong.
add_test(NAME make_set.command COMMAND adl_make_set_tests "[command]")
set_tests_properties(make_set.command PROPERTIES
    TIMEOUT 300
    LABELS "system")

# The built program as a process: its version and help, and a set made from a folder named in three languages
# (tests/system/make_set_unicode.cmake says how it is made sure the name reached it whole).
add_test(NAME tools.adl_make_set.version COMMAND adl_make_set --version)
set_tests_properties(tools.adl_make_set.version PROPERTIES
    LABELS "system"
    PASS_REGULAR_EXPRESSION "^adl_make_set [0-9]+\\.[0-9]+\\.[0-9]+ events 1")
add_test(NAME tools.adl_make_set.no_command COMMAND adl_make_set)
set_tests_properties(tools.adl_make_set.no_command PROPERTIES
    LABELS "system"
    WILL_FAIL TRUE)

add_test(NAME tools.adl_make_set.unicode_folders
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:adl_make_set>"
        "-DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/golden"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/unicode"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/make_set_unicode.cmake")
set_tests_properties(tools.adl_make_set.unicode_folders PROPERTIES LABELS "system")
