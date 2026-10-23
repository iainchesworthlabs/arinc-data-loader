# The FreeRTOS test program: the platform layer's conformance suite and the 615A engine tests, run
# by Catch2 in a FreeRTOS task on the kernel's POSIX simulator port, over lwIP's UDP sockets (its
# loopback interface) and littlefs (a RAM block device). The FreeRTOS counterpart of the Zephyr
# program, with the same test sources (tests/platform/rtos_support/test_sources.cmake).
#
#   cmake -S tests/platform/freertos -B build/freertos -G Ninja
#   cmake --build build/freertos
#   ctest --test-dir build/freertos --output-on-failure
#
# This is a project of its own, not a configuration of the top-level one, because a FreeRTOS
# image is built around the product's kernel configuration, port and main. It is what a product's
# CMake does, on a host: it builds the kernel, lwIP and littlefs at the versions below with a
# configuration (config.cmake), builds the platform layer from the same fragment the other
# platforms use, and links them with a main.
#
# FreeRTOS, lwIP and littlefs are C. The loader's own source is C++ only; these are third-party
# sources, fetched at the versions and hashes below and built as they come.
cmake_minimum_required(VERSION 3.28)

get_filename_component(ADL_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
project(AdlFreeRtosTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# The simulator port multiplexes tasks over host threads with signals, which the address
# sanitizer does not follow; this program is built without it.
set(ADL_ENABLE_SANITIZERS OFF CACHE BOOL "" FORCE)
list(APPEND CMAKE_MODULE_PATH "${ADL_REPO_ROOT}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(Coverage)
include(Tracy)

# ---- the third-party components ------------------------------------------------------------
# A source tree can be named instead of fetched, for a build without network access:
# -DFETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL=<dir> (and _LWIP, _LITTLEFS, _CATCH2).
include(FetchContent)
FetchContent_Declare(freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.2.0.tar.gz
    URL_HASH SHA256=e148d3a442ac7652c998e0a932c4cc2a3c6abff66a3a0a96a7678c0cd092d41c
    SOURCE_SUBDIR no-cmake-here
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(lwip
    URL https://github.com/lwip-tcpip/lwip/archive/refs/tags/STABLE-2_2_1_RELEASE.tar.gz
    URL_HASH SHA256=ce0b7461c0ad9602c376f0bf07c5eb7253b48c7bf66f011c6bf3e2a96731c539
    SOURCE_SUBDIR no-cmake-here
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(littlefs
    URL https://github.com/littlefs-project/littlefs/archive/refs/tags/v2.11.3.tar.gz
    URL_HASH SHA256=15521be8aaed133c453e7337714e029a9f62480ab41f59c4ba5d18297a076207
    SOURCE_SUBDIR no-cmake-here
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# Catch2, the version the vcpkg baseline of the desktop platforms resolves to.
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.3.tar.gz
    URL_HASH SHA256=b0299ae552918220a7a6e21e7de5b714777f4e8c883fb70c4bb23fe01df8c6e3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
set(CATCH_DEVELOPMENT_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freertos_kernel lwip littlefs Catch2)

# ---- the configuration -----------------------------------------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/config.cmake")
set(_generated "${CMAKE_BINARY_DIR}/generated")
adl_generate_header("${_generated}/FreeRTOSConfig.h" "${ADL_FREERTOS_CONFIG_TEXT}")
adl_generate_header("${_generated}/lwipopts.h" "${ADL_LWIP_OPTIONS_TEXT}")
adl_generate_header("${_generated}/arch/cc.h" "${ADL_LWIP_CC_TEXT}")

# ---- FreeRTOS, with its POSIX simulator port and the malloc-backed heap ----------------------
set(_posix "${freertos_kernel_SOURCE_DIR}/portable/ThirdParty/GCC/Posix")
add_library(adl_freertos_kernel STATIC
    "${freertos_kernel_SOURCE_DIR}/tasks.c"
    "${freertos_kernel_SOURCE_DIR}/list.c"
    "${freertos_kernel_SOURCE_DIR}/queue.c"
    "${freertos_kernel_SOURCE_DIR}/event_groups.c"
    "${freertos_kernel_SOURCE_DIR}/stream_buffer.c"
    "${freertos_kernel_SOURCE_DIR}/timers.c"
    "${freertos_kernel_SOURCE_DIR}/portable/MemMang/heap_3.c"
    "${_posix}/port.c"
    "${_posix}/utils/wait_for_event.c")
target_include_directories(adl_freertos_kernel SYSTEM PUBLIC
    "${freertos_kernel_SOURCE_DIR}/include"
    "${_posix}"
    "${_posix}/utils"
    "${_generated}")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(adl_freertos_kernel PUBLIC Threads::Threads)
target_compile_options(adl_freertos_kernel PRIVATE -w)

# ---- littlefs, thread-safe through the configuration's lock and unlock -----------------------
add_library(adl_littlefs STATIC
    "${littlefs_SOURCE_DIR}/lfs.c"
    "${littlefs_SOURCE_DIR}/lfs_util.c")
target_include_directories(adl_littlefs SYSTEM PUBLIC "${littlefs_SOURCE_DIR}")
target_compile_definitions(adl_littlefs PUBLIC LFS_THREADSAFE LFS_NO_DEBUG LFS_NO_WARN)
target_compile_options(adl_littlefs PRIVATE -w)

# ---- lwIP: the core, IPv4 and the sockets API; its system layer is sim/sys_arch_freertos.cpp, in the program ----
set(LWIP_DIR "${lwip_SOURCE_DIR}")
include("${lwip_SOURCE_DIR}/src/Filelists.cmake")
add_library(adl_lwip STATIC
    ${lwipcore_SRCS}
    ${lwipcore4_SRCS}
    ${lwipapi_SRCS})
target_include_directories(adl_lwip SYSTEM PUBLIC
    "${lwip_SOURCE_DIR}/src/include"
    "${CMAKE_CURRENT_LIST_DIR}/sim"
    "${_generated}")
target_link_libraries(adl_lwip PUBLIC adl_freertos_kernel)
target_compile_options(adl_lwip PRIVATE -w)

# ---- the platform layer, from the fragment the other platforms use ----------------------------
set(ADL_PLATFORM freertos)
include("${ADL_REPO_ROOT}/libs/platform/freertos/platform.cmake")
include(PlatformContract)
adl_platform_check_contract("${ADL_REPO_ROOT}/libs/platform/freertos/platform.cmake"
    "${ADL_REPO_ROOT}/libs/platform")
add_subdirectory("${ADL_REPO_ROOT}/libs/platform" adl_platforms)
target_link_libraries(adl_platform PRIVATE adl_freertos_kernel adl_lwip adl_littlefs)

# ---- the loader's platform-free libraries -------------------------------------------------------
add_subdirectory("${ADL_REPO_ROOT}/libs/arinc665" adl_arinc665)
add_subdirectory("${ADL_REPO_ROOT}/libs/arinc615a" adl_arinc615a)
add_subdirectory("${ADL_REPO_ROOT}/libs/arinc615a_runner" adl_arinc615a_runner)
add_subdirectory("${ADL_REPO_ROOT}/libs/arinc615a_mini" adl_arinc615a_mini)
add_subdirectory("${ADL_REPO_ROOT}/libs/samples" adl_samples)

# ---- the program -------------------------------------------------------------------------------
include("${ADL_REPO_ROOT}/tests/platform/rtos_support/test_sources.cmake")

add_executable(adl_freertos_tests
    sim/main.cpp
    sim/ram_file_system.cpp
    sim/stack_storage_test.cpp
    sim/sys_arch_freertos.cpp
    ${ADL_RTOS_PLATFORM_TEST_SOURCES}
    ${ADL_RTOS_ENGINE_TEST_SOURCES})
target_include_directories(adl_freertos_tests PRIVATE
    ${ADL_RTOS_TEST_INCLUDES}
    "${ADL_REPO_ROOT}/libs/platform"
    "${CMAKE_CURRENT_LIST_DIR}/sim")
# littlefs's root: the scratch directories are made there.
target_compile_definitions(adl_freertos_tests PRIVATE "ADL_TEST_SCRATCH_ROOT=\"\"")
target_link_libraries(adl_freertos_tests PRIVATE
    Catch2::Catch2
    adl::platform
    adl::arinc615a_runner
    adl::arinc615a
    adl::arinc665
    adl::arinc615a_mini
    adl::samples
    adl_lwip
    adl_littlefs
    adl_freertos_kernel
    adl::warnings)

enable_testing()
# One CTest test per Catch2 tag group would start a process, a scheduler and a stack each; the
# program is one test with a time limit, and its own output names the failing case.
add_test(NAME freertos.platform_and_engine COMMAND adl_freertos_tests)
set_tests_properties(freertos.platform_and_engine PROPERTIES TIMEOUT 600)
