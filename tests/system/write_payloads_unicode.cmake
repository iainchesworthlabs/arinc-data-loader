# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# adl_write_payloads writes into a folder whose name no Windows code page can hold.
#
#   cmake -DPROGRAM=<adl_write_payloads> -DWORK_DIR=<scratch folder> -P write_payloads_unicode.cmake
#
# The name has an accented letter (in the Western pages), Japanese (in none of them) and a character
# outside the Basic Multilingual Plane (two UTF-16 units), and spaces. A program that reads its argument
# from main's argv on Windows gets the name in the system's code page, where the Japanese is question
# marks, and fails or writes into a folder with another name.
#
# How this makes sure the name reached the program whole. CMake itself reads this file as UTF-8 and hands
# execute_process's arguments to the system in UTF-16 (Windows) or as they are (elsewhere), so the process
# really is started with that name as an argument and nothing in between has narrowed it. Then three
# things must agree, byte for byte:
#   1. the program exits 0, and the folder CMake lists under WORK_DIR has exactly this name (CMake
#      reads names from the system as UTF-16 and gives them back as UTF-8; a mangled name would be a
#      different folder, or no folder);
#   2. a media set is in it, found by the name;
#   3. the program says where it wrote, and that text, which it makes from the path it holds, has the
#      name in UTF-8.
# Linux and macOS have no code page to lose the name in, so this is the same check that the bytes
# came through; on Windows it fails with the argument read from argv, which is how this test was
# shown to test something.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_variable PROGRAM WORK_DIR)
    if(NOT ${_variable})
        message(FATAL_ERROR "Pass -D${_variable}=<value>.")
    endif()
endforeach()

# é, 日本語 and the grinning face (U+1F600), as UTF-8.
set(_leaf "payloads-é-日本語-😀 and spaces")
set(_folder "${WORK_DIR}/${_leaf}")
set(_scenario arinc615)

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${PROGRAM}" --scenario ${_scenario} "${_folder}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    ENCODING UTF-8)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "adl_write_payloads exited with '${_result}' for the folder '${_leaf}'.\n"
        "Standard output: ${_output}\nStandard error: ${_error}")
endif()

# 1. The one folder under WORK_DIR has the name that was given.
file(GLOB _entries LIST_DIRECTORIES true RELATIVE "${WORK_DIR}" "${WORK_DIR}/*")
if(NOT _entries STREQUAL _leaf)
    message(FATAL_ERROR
        "The folder written is '${_entries}', not '${_leaf}': the name did not reach the program whole.")
endif()

# 2. A media set is in it.
file(GLOB_RECURSE _files "${_folder}/${_scenario}/member_*/*")
list(LENGTH _files _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "There are no files under '${_folder}/${_scenario}'.\nStandard output: ${_output}")
endif()

# 3. The program names the folder as UTF-8 text, with the same bytes.
string(FIND "${_output}" "${_leaf}" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR "The program does not say it wrote to '${_leaf}'.\nStandard output: ${_output}")
endif()

message(STATUS "adl_write_payloads wrote ${_count} files into '${_leaf}'.")
