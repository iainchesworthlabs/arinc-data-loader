# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# adl_load takes a media set, a download folder and a record file whose folder is named with characters no
# Windows code page holds all of.
#
#   cmake -DPROGRAM=<adl_load> -DPAYLOADS=<adl_write_payloads> -DWORK_DIR=<scratch folder>
#         -P unicode_folder.cmake
#
# The folder's name has an accented letter (in the Western pages), Japanese (in none of them), a character
# outside the Basic Multilingual Plane (two UTF-16 units) and spaces, as tests/system/write_payloads_unicode.cmake
# has it. A program that reads its arguments from main's argv on Windows gets the name in the system's code
# page, where the Japanese is question marks, so the media set is not found, the download folder is made with
# another name and the record is written to another place. CMake reads this file as UTF-8 and hands
# execute_process's arguments to the system in UTF-16 (Windows) or as they are (elsewhere), so the process is
# started with the name as it is and nothing in between has narrowed it.
#
# What is held, for the three paths (--media-set, --into and --record) of two commands:
#   1. An upload reads the media set that adl_write_payloads wrote into that folder: the `media_set_checked`
#      event has its path, in UTF-8, as it was given. (The program is told to use an address that is not this
#      computer's, so that it ends at once with exit code 5 when it opens its port, and nothing is sent: what
#      is tried is the paths, and no target is needed.)
#   2. A download makes its folder inside a folder of that name, and tells where in UTF-8: the line of the log
#      that says the empty folder was removed has the name.
#   3. Each writes its record into that folder, and the file is found by the name, and says the name.
# The media set's own files have ASCII names: libs/media_tree still throws on Windows for a file name inside a
# set that is outside the code page.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_variable PROGRAM PAYLOADS WORK_DIR)
    if(NOT ${_variable})
        message(FATAL_ERROR "Pass -D${_variable}=<value>.")
    endif()
endforeach()

# é, 日本語 and the grinning face (U+1F600), as UTF-8.
set(_leaf "adl-load-é-日本語-😀 and spaces")
set(_folder "${WORK_DIR}/${_leaf}")
# An address that is not any computer's (RFC 5737, documentation): binding to it fails at once.
set(_not_here 192.0.2.1)

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# The media set, written by the program that has to do the same with its own argument.
execute_process(
    COMMAND "${PAYLOADS}" --scenario two-member "${_folder}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error ENCODING UTF-8)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "adl_write_payloads exited with '${_result}' for '${_leaf}'.\nStandard output: ${_output}\nStandard error: ${_error}")
endif()
set(_set "${_folder}/two-member")
file(GLOB_RECURSE _set_files "${_set}/member_*/*")
list(LENGTH _set_files _set_file_count)
if(_set_file_count EQUAL 0)
    message(FATAL_ERROR "There are no files under '${_set}'.")
endif()

# 1 and 3. An upload of the media set, with the record in the folder.
set(_upload_record "${_folder}/upload record.json")
execute_process(
    COMMAND "${PROGRAM}" upload --target 10.0.0.2 --position HNPFMS_L --media-set "${_set}" --all-loads
        --local-address ${_not_here} --record "${_upload_record}" --json
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error ENCODING UTF-8)
if(NOT _result EQUAL 5)
    message(FATAL_ERROR "The upload exited with '${_result}', not 5 (a port that could not be opened).\nStandard output: ${_output}\nStandard error: ${_error}")
endif()
# The set was found: there is a `media_set_checked` event, and it holds the path as it was given.
string(FIND "${_output}" "\"event\":\"media_set_checked\"" _checked_at)
if(_checked_at EQUAL -1)
    message(FATAL_ERROR "The media set in '${_leaf}' was not found: there is no media_set_checked event.\nStandard output: ${_output}\nStandard error: ${_error}")
endif()
string(FIND "${_output}" "${_leaf}" _leaf_at)
if(_leaf_at EQUAL -1)
    message(FATAL_ERROR "The events do not name '${_leaf}'.\nStandard output: ${_output}")
endif()
if(NOT EXISTS "${_upload_record}")
    message(FATAL_ERROR "The record was not written to '${_upload_record}'.\nStandard output: ${_output}")
endif()
file(READ "${_upload_record}" _record)
string(JSON _record_set_path GET "${_record}" media_set path)
if(NOT _record_set_path STREQUAL _set)
    message(FATAL_ERROR "The record says the media set is '${_record_set_path}', not '${_set}'.")
endif()
string(JSON _record_reason GET "${_record}" reason)
if(NOT _record_reason STREQUAL "local_error")
    message(FATAL_ERROR "The record's reason is '${_record_reason}', not local_error.")
endif()

# 2 and 3. A download into a folder inside it, with the record beside the other.
set(_into "${_folder}/downloads")
set(_download_record "${_folder}/download record.json")
execute_process(
    COMMAND "${PROGRAM}" download --target 10.0.0.2 --position HNPFMS_L --into "${_into}" --file A.BIN
        --local-address ${_not_here} --record "${_download_record}" --json
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error ENCODING UTF-8)
if(NOT _result EQUAL 5)
    message(FATAL_ERROR "The download exited with '${_result}', not 5.\nStandard output: ${_output}\nStandard error: ${_error}")
endif()
if(NOT IS_DIRECTORY "${_into}")
    message(FATAL_ERROR "The download folder '${_into}' was not made.\nStandard output: ${_output}")
endif()
# Nothing was received, so the folder made inside it was removed, and says so with its name in UTF-8.
file(GLOB _made "${_into}/DNLD_DATA_*")
if(_made)
    message(FATAL_ERROR "A download folder is left in '${_into}': ${_made}")
endif()
string(FIND "${_output}" "Nothing was received, so the empty download folder " _removed_at)
string(FIND "${_output}" "${_leaf}" _leaf_at)
if(_removed_at EQUAL -1 OR _leaf_at EQUAL -1)
    message(FATAL_ERROR "The events do not say that the folder in '${_leaf}' was removed.\nStandard output: ${_output}")
endif()
if(NOT EXISTS "${_download_record}")
    message(FATAL_ERROR "The record was not written to '${_download_record}'.\nStandard output: ${_output}")
endif()
file(READ "${_download_record}" _record)
string(JSON _record_reason GET "${_record}" reason)
if(NOT _record_reason STREQUAL "local_error")
    message(FATAL_ERROR "The download record's reason is '${_record_reason}', not local_error.")
endif()

message(STATUS "adl_load used the media set, the download folder and the records in '${_leaf}'.")
