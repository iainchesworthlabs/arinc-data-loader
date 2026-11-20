# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# adl_mock serves a download from, and stores an upload into, folders whose names no Windows code page can hold.
#
#   cmake -DPROGRAM=<adl_mock> -DCLIENT=<adl_mock_command_test> -DWORK_DIR=<scratch folder> -P mock_unicode.cmake
#
# The names have an accented letter (in the Western code pages), Japanese (in none of them) and a character
# outside the Basic Multilingual Plane (two UTF-16 units), and spaces. A program that reads --holds and --storage
# from main's argv on Windows gets them in the system's code page, where the Japanese is question marks, and
# fails or uses folders with other names.
#
# The program is started as a process, with the names as its arguments: CMake hands execute_process's arguments
# to the system in UTF-16 (Windows) or as they are (elsewhere), so nothing between this script and the program
# has narrowed them. The loader is the next command of the same execute_process: CMake connects the program's
# standard output to its standard input, which is how it knows the target is listening (the "started" event) and
# what the target wrote. CLIENT is the test program of tests/mock_cli/command_test.cpp, asked for by the tag of the
# hidden test case that is its client: it uploads a load, downloads F1.BIN, checks what it got, and echoes the events.
# Then, byte for byte:
#   1. both programs exit with 0, and the folders CMake lists under WORK_DIR are exactly the two with the names
#      that were given (CMake reads names from the system as UTF-16 and gives them back as UTF-8; a mangled name
#      would be a different folder, or no folder, or the program would not have started);
#   2. the loader downloaded F1.BIN from the --holds folder, with the content this script wrote (the client's
#      check, and it is the only way it can have it: the target holds nothing else called that);
#   3. the files of the upload are in the --storage folder, under the load's part number; and
#   4. the program names the storage folder in its upload_received event as UTF-8 text with the same bytes.
# Linux and macOS have no code page to lose the names in, so this is the same check that the bytes came through; on
# Windows it fails with the arguments read from argv.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_variable PROGRAM CLIENT WORK_DIR)
    if(NOT ${_variable})
        message(FATAL_ERROR "Pass -D${_variable}=<value>.")
    endif()
endforeach()

# é, 日本語 and the grinning face (U+1F600), as UTF-8.
set(_holds_leaf "holds-é-日本語-😀 and spaces")
set(_storage_leaf "storage-é-日本語-😀 and spaces")
set(_holds "${WORK_DIR}/${_holds_leaf}")
set(_storage "${WORK_DIR}/${_storage_leaf}")

# What the target holds, and what the client expects to get (the same text is in command_test.cpp). It has no
# line end, since file(WRITE) writes a line feed as a carriage return and a line feed on Windows.
set(_held_text "Held in a folder whose name no code page can hold.")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${_holds}")
file(WRITE "${_holds}/F1.BIN" "${_held_text}")

# The ports and the pace are those of command_test.cpp, whose client this is, and the test has the same resource
# lock, so the two never run at once. The program ends by itself after the two operations; --seconds is for the
# day something fails.
execute_process(
    COMMAND "${PROGRAM}"
        --holds "${_holds}" --storage "${_storage}"
        --operations 2 --seconds 60 --json
        --tftp-port 31959 --find-port 31961 --think-time-ms 200 --status-period 1
    COMMAND "${CLIENT}" "[unicode_client]"
    RESULTS_VARIABLE _results
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    TIMEOUT 150
    ENCODING UTF-8)

# 1. Both programs exited with 0, and the two folders are the only ones under WORK_DIR.
foreach(_result IN LISTS _results)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "The programs exited with '${_results}' (adl_mock first) for the folders '${_holds_leaf}' and "
            "'${_storage_leaf}'.\nStandard output of the loader: ${_output}\nStandard error: ${_error}")
    endif()
endforeach()
file(GLOB _entries LIST_DIRECTORIES true RELATIVE "${WORK_DIR}" "${WORK_DIR}/*")
list(SORT _entries)
set(_expected "${_holds_leaf}" "${_storage_leaf}")
if(NOT _entries STREQUAL _expected)
    message(FATAL_ERROR
        "The folders under WORK_DIR are '${_entries}', not '${_expected}': a name did not reach the program whole.")
endif()

# 2 is the client's own check. 3. The upload is stored in the --storage folder.
file(GLOB_RECURSE _stored "${_storage}/*")
list(LENGTH _stored _count)
if(_count LESS 2)
    message(FATAL_ERROR
        "There are ${_count} files under '${_storage_leaf}', and an upload has at least two (the header file and "
        "a data file).\nStandard output: ${_output}")
endif()

# 4. The program says where it stored the upload, as UTF-8 text with the same bytes (a JSON string: the name
# in it is not escaped, apart from the separators of the path).
string(FIND "${_output}" "${_storage_leaf}" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR "The upload_received event does not name '${_storage_leaf}'.\nStandard output: ${_output}")
endif()

message(STATUS "adl_mock stored ${_count} files into '${_storage_leaf}' and served F1.BIN from '${_holds_leaf}'.")
