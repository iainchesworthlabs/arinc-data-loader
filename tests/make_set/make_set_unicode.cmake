# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# adl_make_set reads a description, reads its sources and writes a media set in folders whose names no Windows
# code page can hold.
#
#   cmake -DPROGRAM=<adl_make_set> -DDATA_DIR=<tests/make_set/data/golden> -DWORK_DIR=<scratch folder>
#         -P make_set_unicode.cmake
#
# The names have an accented letter (in the Western pages), Japanese (in none of them) and a character outside
# the Basic Multilingual Plane (two UTF-16 units), and spaces: the folder that holds the description and its
# sources, the description itself, and the folder the set is made in. A program that reads its arguments from
# main's argv on Windows gets them in the system's code page, where the Japanese is question marks, and fails or
# makes the set in a folder with another name.
#
# How this makes sure the names reached the program whole. CMake reads this file as UTF-8 and hands
# execute_process's arguments to the system in UTF-16 (Windows) or as they are (elsewhere), so the process really
# is started with those names as arguments and nothing in between has narrowed them. Then these must agree:
#   1. `make` exits 0 and the folder CMake lists under WORK_DIR for the output has exactly the name given (CMake
#      reads names from the system as UTF-16 and gives them back as UTF-8; a mangled name would be another
#      folder, or none);
#   2. the program says in its events where it made the set, in UTF-8, with the name;
#   3. `verify` and `inspect` find the set by the same name, and exit 0.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_variable PROGRAM DATA_DIR WORK_DIR)
    if(NOT ${_variable})
        message(FATAL_ERROR "Pass -D${_variable}=<value>.")
    endif()
endforeach()

# é, 日本語 and the grinning face (U+1F600), as UTF-8.
set(_rig "rig-é-日本語-😀 and spaces")
set(_out "out-é-日本語-😀 and spaces")
set(_description "set-é-日本語.json")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/${_rig}")
file(COPY "${DATA_DIR}/payloads" DESTINATION "${WORK_DIR}/${_rig}")
file(COPY_FILE "${DATA_DIR}/description.json" "${WORK_DIR}/${_rig}/${_description}")

execute_process(
    COMMAND "${PROGRAM}" make "${WORK_DIR}/${_rig}/${_description}" --output "${WORK_DIR}/${_out}/set" --json
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    ENCODING UTF-8)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "adl_make_set make exited with '${_result}'.\nStandard output: ${_output}\nStandard error: ${_error}")
endif()

# 1. The one folder under WORK_DIR beside the rig has the name that was given, and holds the set.
file(GLOB _entries LIST_DIRECTORIES true RELATIVE "${WORK_DIR}" "${WORK_DIR}/out-*")
if(NOT _entries STREQUAL _out)
    message(FATAL_ERROR "The folder made is '${_entries}', not '${_out}': the name did not reach the program whole.")
endif()
file(GLOB_RECURSE _files "${WORK_DIR}/${_out}/set/member_*/*")
list(LENGTH _files _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "There are no files under '${_out}/set'.\nStandard output: ${_output}")
endif()

# 2. The program names the folder as UTF-8 text, with the same bytes.
string(FIND "${_output}" "${_out}" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR "The program does not say it made the set in '${_out}'.\nStandard output: ${_output}")
endif()
string(FIND "${_output}" "\"reason\":\"completed\"" _completed)
if(_completed EQUAL -1)
    message(FATAL_ERROR "The program does not say it completed.\nStandard output: ${_output}")
endif()

# 3. The same set is found, checked and read by the name.
foreach(_command verify inspect)
    execute_process(
        COMMAND "${PROGRAM}" ${_command} "${WORK_DIR}/${_out}/set" --json
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error
        ENCODING UTF-8)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "adl_make_set ${_command} exited with '${_result}'.\nStandard output: ${_output}\nStandard error: ${_error}")
    endif()
    string(FIND "${_output}" "${_out}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "adl_make_set ${_command} does not name '${_out}'.\nStandard output: ${_output}")
    endif()
endforeach()

message(STATUS "adl_make_set made, verified and inspected a set of ${_count} files in '${_out}'.")
