# SPDX-License-Identifier: GPL-3.0-or-later
# Installs the development component of a finished build into a scratch prefix, then builds a small
# program against it with find_package(adl) and runs it: the proof that what the development package
# holds is enough, and that nothing in it points back into the source or build tree.
#
#   cmake -DBUILD_DIR=<build tree> -DSOURCE_DIR=<repository root> -DWORK_DIR=<scratch>
#         -DGENERATOR=<generator> -DCXX_COMPILER=<compiler> -DBUILD_TYPE=<config> -DDOC_DIR=<where the
#         licence files are installed>
#         -P tests/packaging/check_install.cmake
cmake_minimum_required(VERSION 3.28)

foreach(required BUILD_DIR SOURCE_DIR WORK_DIR GENERATOR CXX_COMPILER BUILD_TYPE DOC_DIR)
    if(NOT ${required})
        message(FATAL_ERROR "Pass -D${required}=...")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --component development --prefix "${WORK_DIR}/prefix"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "installing the development component failed (${result}):\n${output}\n${error}")
endif()

foreach(expected include/adl/arinc665/crc.hpp include/adl/platform/host.hpp lib/cmake/adl/adlConfig.cmake
        lib/cmake/adl/adlTargets.cmake "${DOC_DIR}/LICENSE.txt")
    if(NOT EXISTS "${WORK_DIR}/prefix/${expected}")
        message(FATAL_ERROR "the development package lacks ${expected}")
    endif()
endforeach()

# The exported targets must not name the source or build tree.
file(GLOB targets_files "${WORK_DIR}/prefix/lib/cmake/adl/adlTargets*.cmake")
foreach(file IN LISTS targets_files)
    file(READ "${file}" text)
    foreach(forbidden "${SOURCE_DIR}" "${BUILD_DIR}")
        string(FIND "${text}" "${forbidden}" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${file} names ${forbidden}, which a consumer does not have")
        endif()
    endforeach()
    foreach(forbidden adl_warnings adl_sanitizers adl_coverage adl_tracy)
        string(FIND "${text}" "${forbidden}" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${file} names ${forbidden}, which is a build-only target")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/packaging/consumer" -B "${WORK_DIR}/consumer"
        -G "${GENERATOR}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}" "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "configuring the consumer failed (${result}):\n${output}\n${error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${WORK_DIR}/consumer"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "building the consumer failed (${result}):\n${output}\n${error}")
endif()

file(GLOB_RECURSE consumer_exe "${WORK_DIR}/consumer/consumer*")
list(FILTER consumer_exe EXCLUDE REGEX "\.(obj|o|pdb|ilk|d|json|cmake|txt|ninja|log)$|CMakeFiles")
list(GET consumer_exe 0 consumer)
execute_process(COMMAND "${consumer}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "the consumer failed (${result}):\n${output}\n${error}")
endif()
message(STATUS "development package consumer: ${output}")
