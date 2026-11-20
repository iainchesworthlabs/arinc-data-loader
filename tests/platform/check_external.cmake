# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# A platform kept outside the repository (ADL_PLATFORM_DIR) is built from where it is, and the
# mistakes that can be made with it are refused by name.
#
#   cmake -DREPO_ROOT=<repository root> -DWORK_DIR=<scratch directory>
#         -DGENERATOR=<generator> -DCOMPILER=<C++ compiler>
#         -DHOST_PLATFORM=<the platform this build targets> -DHOST_DIR=<its directory>
#         -P tests/platform/check_external.cmake
#
# It takes the platform this build targets, copies its fragment and its own sources to a
# directory under WORK_DIR (outside the source tree), points the fragment's source list at the
# copies, and configures and builds the whole project for a platform of another name from that
# directory. The shared sources the fragment names relative to libs/platform (posix/, detail/)
# are not copied: they are the ones the outside platform is meant to reuse. Then it looks at the
# compile commands to see that every source of the copy was compiled from the copy and none from
# the repository's directory of the platform.
#
# The refusals are checked on the selector alone (cmake/Platform.cmake included by a script),
# which is what decides them, and which does not need a compiler.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

# ---- child mode: run the selector alone, in this (fresh) process -----------------
if(SELECT_ONLY)
    set(PROJECT_SOURCE_DIR "${REPO_ROOT}")
    list(APPEND CMAKE_MODULE_PATH "${REPO_ROOT}/cmake")
    include(Platform)
    message(STATUS "selected ${ADL_PLATFORM} in ${ADL_PLATFORM_DIR} (outside: ${ADL_PLATFORM_EXTERNAL})")
    return()
endif()

foreach(_required REPO_ROOT WORK_DIR GENERATOR COMPILER HOST_PLATFORM HOST_DIR)
    if(NOT ${_required})
        message(FATAL_ERROR "Pass -D${_required}=<value>.")
    endif()
endforeach()

set(_failures "")
set(_port_name "outside_${HOST_PLATFORM}")

# run(<result> <output> <command and arguments...>): a process; its output and error text as one
# line.
function(run result output)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error)
    string(REGEX REPLACE "[ \t\r\n]+" " " _all "${_output}\n${_error}")
    set(${result} ${_result} PARENT_SCOPE)
    set(${output} "${_all}" PARENT_SCOPE)
endfunction()

# select_case(<name> <succeeds: ON|OFF> <text the output must contain> <-D arguments>...)
function(select_case name succeeds text)
    run(_result _all "${CMAKE_COMMAND}" "-DREPO_ROOT=${REPO_ROOT}" -DSELECT_ONLY=ON ${ARGN}
        -P "${CMAKE_CURRENT_LIST_FILE}")
    if(succeeds AND NOT _result EQUAL 0)
        list(APPEND _failures "${name}: the selector refused it, and should have accepted it:\n${_all}")
    elseif(NOT succeeds AND _result EQUAL 0)
        list(APPEND _failures "${name}: the selector accepted it, and should have refused it.")
    elseif(NOT _all MATCHES "${text}")
        list(APPEND _failures "${name}: the output does not say '${text}':\n${_all}")
    else()
        message(STATUS "${name}: ok")
    endif()
    set(_failures "${_failures}" PARENT_SCOPE)
endfunction()

# ---- the copy ------------------------------------------------------------------------------------

set(_outside "${WORK_DIR}/outside")
set(_port "${_outside}/port")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${_outside}")
file(COPY "${HOST_DIR}/" DESTINATION "${_port}")

# The fragment names its own files as <name>/<file>; in the copy they are beside the fragment. The
# names of the shared ones (posix/..., detail/...) stay as they are.
get_filename_component(_host_name "${HOST_DIR}" NAME)
file(READ "${_port}/platform.cmake" _fragment)
string(REGEX MATCHALL "\n    ${_host_name}/[^\n]+" _own_entries "${_fragment}")
if(NOT _own_entries)
    message(FATAL_ERROR
        "${HOST_DIR}/platform.cmake lists no source as ${_host_name}/<file>, so there is nothing to copy.")
endif()
string(REPLACE "\n    ${_host_name}/" "\n    \${ADL_PLATFORM_DIR}/" _fragment "${_fragment}")
file(WRITE "${_port}/platform.cmake" "${_fragment}")

# ---- what the selector accepts and refuses -------------------------------------------------------

select_case(accepted ON "selected ${_port_name} in ${_port} [(]outside: ON[)]"
    -DADL_TARGET_PLATFORM=${_port_name} -DADL_PLATFORM_DIR=${_port})

# The directory is the platform, not the name: a name the repository has is read from there too.
select_case(known_name ON "selected vxworks in ${_port} [(]outside: ON[)]"
    -DADL_TARGET_PLATFORM=vxworks -DADL_PLATFORM_DIR=${_port})

select_case(no_name OFF "ADL_PLATFORM_DIR is '${_port}', but ADL_TARGET_PLATFORM is not set"
    -DADL_PLATFORM_DIR=${_port})

select_case(bad_name OFF "ADL_TARGET_PLATFORM is 'My-Port', which cannot name a platform"
    -DADL_TARGET_PLATFORM=My-Port -DADL_PLATFORM_DIR=${_port})

select_case(no_directory OFF "ADL_PLATFORM_DIR is '${_outside}/missing', which is not a directory"
    -DADL_TARGET_PLATFORM=${_port_name} -DADL_PLATFORM_DIR=${_outside}/missing)

file(MAKE_DIRECTORY "${_outside}/empty")
select_case(no_fragment OFF "ADL_PLATFORM_DIR is '${_outside}/empty', which has no platform.cmake"
    -DADL_TARGET_PLATFORM=${_port_name} -DADL_PLATFORM_DIR=${_outside}/empty)

# The contract holds for a platform outside the repository as for one inside it: a file the
# fragment lists must be there.
file(COPY "${_port}/" DESTINATION "${_outside}/lists_a_missing_file")
file(APPEND "${_outside}/lists_a_missing_file/platform.cmake"
    "\nlist(APPEND ADL_PLATFORM_SOURCES \"\${ADL_PLATFORM_DIR}/not_there.cpp\")\n")
select_case(missing_source OFF "not_there.cpp, which does not exist"
    -DADL_TARGET_PLATFORM=${_port_name} -DADL_PLATFORM_DIR=${_outside}/lists_a_missing_file)

# ---- a build from the copy -------------------------------------------------------------------

# The whole project is configured for the platform of the copy, with its library built. The
# platform tests of the copy are run by building the project with the same two options (a test
# program needs the test framework, which is a long way to fetch in a test).
set(_build "${WORK_DIR}/build")
run(_result _all "${CMAKE_COMMAND}" -S "${REPO_ROOT}" -B "${_build}" -G "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${COMPILER}" -DADL_BUILD_TESTS=OFF -DADL_BUILD_APP=OFF
    -DADL_TARGET_PLATFORM=${_port_name} "-DADL_PLATFORM_DIR=${_port}")
if(NOT _result EQUAL 0)
    list(APPEND _failures "configure: configuring from the copy failed:\n${_all}")
else()
    run(_result _all "${CMAKE_COMMAND}" --build "${_build}" --target adl_platform)
    if(NOT _result EQUAL 0)
        list(APPEND _failures "build: building adl_platform from the copy failed:\n${_all}")
    else()
        message(STATUS "build: adl_platform built from ${_port}")

        # Every source of the copy was compiled from the copy, and the repository's own copy of
        # it was not compiled at all.
        file(READ "${_build}/compile_commands.json" _commands)
        file(GLOB _copied RELATIVE "${_port}" "${_port}/*.cpp")
        foreach(_source IN LISTS _copied)
            string(FIND "${_commands}" "\"file\": \"${_port}/${_source}\"" _from_copy)
            string(FIND "${_commands}" "${HOST_DIR}/${_source}" _from_repository)
            if(_from_copy EQUAL -1)
                list(APPEND _failures "provenance: ${_source} was not compiled from ${_port}.")
            elseif(NOT _from_repository EQUAL -1)
                list(APPEND _failures "provenance: ${_source} was compiled from ${HOST_DIR} too.")
            else()
                message(STATUS "provenance: ${_source} compiled from the copy")
            endif()
        endforeach()
        list(LENGTH _copied _copied_count)
        if(_copied_count EQUAL 0)
            list(APPEND _failures "provenance: there is no source in ${_port}.")
        endif()

        # A shared source was compiled from the repository, not copied.
        string(FIND "${_commands}" "${REPO_ROOT}/libs/platform/detail/ring_queue.cpp" _shared)
        if(_shared EQUAL -1)
            list(APPEND _failures "provenance: detail/ring_queue.cpp was not compiled from the repository.")
        endif()
    endif()
endif()

if(_failures)
    list(JOIN _failures "\n\n" _report)
    message(FATAL_ERROR "A platform outside the repository is not handled as it should be:\n\n${_report}")
endif()
