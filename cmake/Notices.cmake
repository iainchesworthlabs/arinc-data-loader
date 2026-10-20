# ---------------------------------------------------------------------------
# Notices.cmake
#
# adl_generate_notices(<out-file>
#     FRAGMENT_DIR <dir>       the directory holding <fragment>.txt files
#     FRAGMENTS <name>...      which fragments, in the order they appear
#     TOKENS <KEY=value>...    every {{KEY}} in the fragments becomes value
#     FILES <NAME=path>...     every {{FILE:NAME}} becomes that file's text)
#
# Assembles the third-party notices file the applications show under About >
# Licences and carry beside the executables, at configure time, from plain-text
# fragments (notices/fragments/) and the licence texts they quote
# (notices/licences/, src/ui/fonts/OFL.txt). One file, written once, from the
# versions CMake already knows, so the dialog and the file on disk cannot say
# different things.
#
# Substitution is string(REPLACE) on {{TOKEN}} markers, deliberately not
# configure_file(@ONLY): licence texts must stay byte-exact, and copyright lines
# carry '@' in e-mail addresses.
#
# Failure is loud, at configure: a fragment that does not exist, a {{FILE:NAME}}
# whose file is missing, and any {{TOKEN}} still in the output each stop the
# configure and name the culprit. Line endings are normalised to LF whatever the
# checkout's autocrlf did, every fragment and licence read is a configure
# dependency, and the output is rewritten only when its content changes.
# ---------------------------------------------------------------------------

function(adl_generate_notices out)
    cmake_parse_arguments(PARSE_ARGV 1 N "" "FRAGMENT_DIR" "FRAGMENTS;TOKENS;FILES")
    if(N_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "adl_generate_notices: unexpected arguments: ${N_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT N_FRAGMENT_DIR OR NOT N_FRAGMENTS)
        message(FATAL_ERROR "adl_generate_notices: FRAGMENT_DIR and FRAGMENTS are required")
    endif()

    # 1. The fragments, in order, one blank line between them.
    set(text "")
    foreach(fragment IN LISTS N_FRAGMENTS)
        set(path "${N_FRAGMENT_DIR}/${fragment}.txt")
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR
                "notices: the fragment list names '${fragment}' but ${path} does not exist")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        file(READ "${path}" chunk)
        string(REPLACE "\r\n" "\n" chunk "${chunk}")
        string(REGEX REPLACE "\n+$" "" chunk "${chunk}")
        string(APPEND text "${chunk}\n\n")
    endforeach()

    # 2. {{FILE:NAME}} -> the named file's text. Read only when a fragment asks for it, so a file
    #    that only an option's fragment uses is not required by every build.
    foreach(pair IN LISTS N_FILES)
        string(FIND "${pair}" "=" eq)
        if(eq EQUAL -1)
            message(FATAL_ERROR "adl_generate_notices: FILES entry '${pair}' is not NAME=path")
        endif()
        string(SUBSTRING "${pair}" 0 ${eq} name)
        math(EXPR after "${eq} + 1")
        string(SUBSTRING "${pair}" ${after} -1 path)
        string(CONCAT marker "{{FILE:" "${name}" "}}")
        string(FIND "${text}" "${marker}" at)
        if(at EQUAL -1)
            continue()
        endif()
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR
                "notices: a fragment uses ${marker} but its file ${path} does not exist")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        file(READ "${path}" content)
        string(REPLACE "\r\n" "\n" content "${content}")
        string(REGEX REPLACE "\n+$" "" content "${content}")
        string(REPLACE "${marker}" "${content}" text "${text}")
    endforeach()

    # 3. {{KEY}} -> value.
    foreach(pair IN LISTS N_TOKENS)
        string(FIND "${pair}" "=" eq)
        if(eq EQUAL -1)
            message(FATAL_ERROR "adl_generate_notices: TOKENS entry '${pair}' is not KEY=value")
        endif()
        string(SUBSTRING "${pair}" 0 ${eq} key)
        math(EXPR after "${eq} + 1")
        string(SUBSTRING "${pair}" ${after} -1 value)
        string(CONCAT marker "{{" "${key}" "}}")
        string(REPLACE "${marker}" "${value}" text "${text}")
    endforeach()

    # 4. Nothing may be left unfilled.
    string(REGEX MATCH "{{[A-Za-z0-9_:.-]+}}" leftover "${text}")
    if(leftover)
        message(FATAL_ERROR
            "notices: ${out} would still contain the token ${leftover}: no TOKENS or FILES "
            "value was supplied for it")
    endif()
    string(REGEX REPLACE "\n+$" "\n" text "${text}")

    # 5. Write, only on change.
    get_filename_component(dir "${out}" DIRECTORY)
    file(MAKE_DIRECTORY "${dir}")
    set(previous "")
    if(EXISTS "${out}")
        file(READ "${out}" previous)
    endif()
    if(NOT previous STREQUAL text)
        file(WRITE "${out}" "${text}")
    endif()
endfunction()
