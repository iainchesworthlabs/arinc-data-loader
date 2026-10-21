# Runs an application's lupdate project over a copy of its Qt Linguist catalogues and fails if the
# result differs from the committed files in anything but the <location> lines.
#
# That catches what a hand edit or a forgotten `update_translations` leaves behind: a string the
# sources gained that no catalogue has (it would be drawn in English), a string the sources lost
# that a catalogue still carries (-no-obsolete would drop it), or one whose English was reworded
# (the old entry is dropped and the new one arrives unfinished). The catalogues are never written
# by this script.
#
# The project is the one the `<name>_lupdate` target runs - the same sources, headers and include
# paths, which is what decides a message's context - with the catalogues swapped for copies. Its
# description is written at generate time (PROJECT_CMAKE) and turned into the file lupdate reads
# by Qt's own script (PROJECT_GENERATOR), exactly as the target does.
#
#   cmake -DLUPDATE=<lupdate> -DLCONVERT=<lconvert> -DPROJECT_CMAKE=<...project.cmake>
#         -DPROJECT_GENERATOR=<GenerateLUpdateProject.cmake> -DTS_FILES=<a.ts|b.ts|...>
#         -DPLURALS_TS_FILE=<en.ts> -DWORK_DIR=<scratch> -P CheckTranslations.cmake
#
# The list is separated by "|" because a ";" does not survive being a test argument.

foreach(required IN ITEMS LUPDATE LCONVERT PROJECT_CMAKE PROJECT_GENERATOR TS_FILES
                          PLURALS_TS_FILE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "CheckTranslations.cmake: ${required} is not set")
    endif()
endforeach()
if(NOT EXISTS "${PROJECT_CMAKE}")
    message(FATAL_ERROR "CheckTranslations.cmake: ${PROJECT_CMAKE} does not exist; "
                        "configure the project first")
endif()

string(REPLACE "|" ";" TS_FILES "${TS_FILES}")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(copies "")
foreach(ts_file IN LISTS TS_FILES PLURALS_TS_FILE)
    get_filename_component(name "${ts_file}" NAME)
    file(COPY_FILE "${ts_file}" "${WORK_DIR}/${name}")
    list(APPEND copies "${WORK_DIR}/${name}")
endforeach()

# The project description with the catalogues' line pointing at the copies.
file(READ "${PROJECT_CMAKE}" project)
list(JOIN copies ";" copies_joined)
string(REGEX REPLACE "set\\(lupdate_translations \"[^\"]*\"\\)"
       "set(lupdate_translations \"${copies_joined}\")" project "${project}")
file(WRITE "${WORK_DIR}/project.cmake" "${project}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DIN_FILE=${WORK_DIR}/project.cmake"
            "-DOUT_FILE=${WORK_DIR}/project.json" -P "${PROJECT_GENERATOR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "the lupdate project could not be written (${result}):\n${output}")
endif()

execute_process(
    COMMAND "${LUPDATE}" -project "${WORK_DIR}/project.json" -no-obsolete
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "lupdate failed (${result}):\n${output}")
endif()

# The plurals-only catalogue is what lconvert -pluralonly leaves of lupdate's English one.
get_filename_component(plurals_name "${PLURALS_TS_FILE}" NAME)
execute_process(
    COMMAND "${LCONVERT}" -pluralonly -i "${WORK_DIR}/${plurals_name}"
            -o "${WORK_DIR}/${plurals_name}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "lconvert failed (${result}):\n${output}")
endif()

# What a catalogue says apart from where in the sources each message was found.
function(adl_catalogue_text out path)
    file(READ "${path}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REGEX REPLACE "[ \t]*<location[^\n]*\n" "" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(drifted "")
foreach(ts_file IN LISTS TS_FILES PLURALS_TS_FILE)
    get_filename_component(name "${ts_file}" NAME)
    adl_catalogue_text(committed "${ts_file}")
    adl_catalogue_text(regenerated "${WORK_DIR}/${name}")
    if(NOT committed STREQUAL regenerated)
        file(WRITE "${WORK_DIR}/${name}.committed" "${committed}")
        file(WRITE "${WORK_DIR}/${name}.regenerated" "${regenerated}")
        list(APPEND drifted "${name}")
    endif()
endforeach()

if(drifted)
    list(JOIN drifted "\n  " listing)
    message(FATAL_ERROR
        "These catalogues no longer match the sources:\n  ${listing}\n"
        "Run `cmake --build <dir> --target update_translations`, give every new entry its "
        "translation, and commit the result. Compare ${WORK_DIR}/<name>.committed with "
        "${WORK_DIR}/<name>.regenerated to see what differs.")
endif()

message(STATUS "The catalogues match the sources")
