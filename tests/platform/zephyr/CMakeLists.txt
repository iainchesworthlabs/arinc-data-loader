# The Zephyr test program: the platform layer's conformance suite and the 615A engine tests,
# run by Catch2 on Zephyr. Built with west or cmake as any Zephyr application is, for native_sim
# in CI (docs/building.md, docs/testing.md); this is the Zephyr counterpart of the
# adl_platform_tests and adl_engine_tests programs of tests/CMakeLists.txt, and runs the same
# source files:
#
#   west build -b native_sim/native/64 tests/platform/zephyr -- -DZEPHYR_EXTRA_MODULES=$PWD
#
# The repository is a Zephyr module (zephyr/module.yml) that builds the platform layer and the
# libraries; this application supplies Catch2, a main, and the board's file system.
cmake_minimum_required(VERSION 3.20.0)

get_filename_component(ADL_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES "${ADL_REPO_ROOT}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(adl_zephyr_tests LANGUAGES C CXX)

# Catch2, the version the vcpkg baseline of the other platforms resolves to. A source tree can
# be named with -DFETCHCONTENT_SOURCE_DIR_CATCH2=<dir> for a build without network access.
include(FetchContent)
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.3.tar.gz
    URL_HASH SHA256=b0299ae552918220a7a6e21e7de5b714777f4e8c883fb70c4bb23fe01df8c6e3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
set(CATCH_DEVELOPMENT_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Catch2)

# What the engine tests need beyond the module's libraries: the sample loads and the second
# TFTP implementation the simulated target is written with.
list(PREPEND CMAKE_MODULE_PATH "${ADL_REPO_ROOT}/cmake")
add_subdirectory("${ADL_REPO_ROOT}/src/samples" adl_samples)
add_subdirectory("${ADL_REPO_ROOT}/src/arinc615a_mini" adl_arinc615a_mini)

# Zephyr's flags for them, as the module gives its libraries (-std=c++23 in particular).
foreach(_lib IN ITEMS adl_samples adl_arinc615a_mini)
    target_link_libraries(${_lib} PRIVATE zephyr_interface)
    if(NOT CONFIG_ADL_WARNINGS_AS_ERRORS)
        target_link_libraries(${_lib} PRIVATE adl_zephyr_tolerant)
    endif()
endforeach()

# The sources and include directories the RTOS test programs share.
include("${ADL_REPO_ROOT}/tests/platform/rtos_support/test_sources.cmake")

target_sources(app PRIVATE
    src/main.cpp
    src/stack_storage_test.cpp
    ${ADL_RTOS_PLATFORM_TEST_SOURCES}
    ${ADL_RTOS_ENGINE_TEST_SOURCES})
target_include_directories(app PRIVATE ${ADL_RTOS_TEST_INCLUDES})
target_compile_definitions(app PRIVATE "ADL_TEST_SCRATCH_ROOT=\"/lfs\"")

target_link_libraries(app PRIVATE
    Catch2::Catch2
    adl::platform
    adl::arinc615a_runner
    adl::arinc615a
    adl::arinc665
    adl::arinc615a_mini
    adl::samples)
