# ---------------------------------------------------------------------------
# The selector (cmake/Platform.cmake) learns the target the way
# docs/platform-isolation.md says, and refuses a platform that has no code to
# build.
#
#   cmake -DREPO_ROOT=<repository root> -DWORK_DIR=<scratch directory>
#         -DGENERATOR=<generator> -DCOMPILER=<C++ compiler>
#         -P tests/platform/check_selector.cmake
#
# Each case configures the whole project in a directory of its own with
# ADL_TARGET_PLATFORM set, and looks at whether that succeeded and what it said.
# Configuring is the only way to run the selector as a build does: it decides
# from the toolchain's own flags, which a script cannot set up.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)

foreach(_required REPO_ROOT WORK_DIR GENERATOR COMPILER)
    if(NOT ${_required})
        message(FATAL_ERROR "Pass -D${_required}=<value>.")
    endif()
endforeach()

set(_failures "")

# configure_case(<name> <succeeds: ON|OFF> <text the output must contain> [<extra -D arguments>...])
function(configure_case name succeeds text)
    set(_dir "${WORK_DIR}/${name}")
    file(REMOVE_RECURSE "${_dir}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${REPO_ROOT}" -B "${_dir}" -G "${GENERATOR}"
            "-DCMAKE_CXX_COMPILER=${COMPILER}" -DADL_BUILD_TESTS=OFF ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error)
    set(_all "${_output}\n${_error}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _all "${_all}")

    if(succeeds AND NOT _result EQUAL 0)
        list(APPEND _failures "${name}: configuring failed, and should have worked:\n${_all}")
    elseif(NOT succeeds AND _result EQUAL 0)
        list(APPEND _failures "${name}: configuring worked, and should have been refused.")
    elseif(NOT _all MATCHES "${text}")
        list(APPEND _failures "${name}: the output does not say '${text}':\n${_all}")
    else()
        message(STATUS "${name}: ok")
    endif()
    set(_failures "${_failures}" PARENT_SCOPE)
endfunction()

# The platforms with a design and no code are refused, and say why.
configure_case(freertos OFF "FreeRTOS platform is a design only" -DADL_TARGET_PLATFORM=freertos)
configure_case(vxworks OFF "VxWorks platform is a design only" -DADL_TARGET_PLATFORM=vxworks)
configure_case(zephyr OFF "Zephyr platform is a design only" -DADL_TARGET_PLATFORM=zephyr)

# The platform with code nobody has compiled is refused until it is asked for.
configure_case(qnx_refused OFF "ADL_ACCEPT_UNBUILT_PLATFORM" -DADL_TARGET_PLATFORM=qnx)
configure_case(qnx_accepted ON "for QNX Neutrino" -DADL_TARGET_PLATFORM=qnx
    -DADL_ACCEPT_UNBUILT_PLATFORM=ON)

# A name that is not a platform is an error that lists the ones that are.
configure_case(nonsense OFF "which is not a platform" -DADL_TARGET_PLATFORM=nonsense)

if(_failures)
    list(JOIN _failures "\n\n" _report)
    message(FATAL_ERROR "The platform selector misbehaves:\n\n${_report}")
endif()
