# FreeRTOS. Included by cmake/Platform.cmake and by the FreeRTOS test project's own
# CMakeLists.txt; the contract it fills in is described at the top of cmake/Platform.cmake.
#
# STATUS: experimental. The code is built and the platform conformance suite and the 615A
# engine tests are run on the FreeRTOS kernel's POSIX simulator port (a Linux process with the
# real kernel, lwIP's UDP sockets over its loopback interface and littlefs on a RAM block
# device: tests/platform/freertos, CI job freertos-posix-sim). No target hardware has run it,
# and the product's FreeRTOSConfig.h, task plan, network interface and flash decide what the
# simulator cannot show. docs/platform-abstraction.md says what is and is not verified.
#
# FreeRTOS is a kernel, not an operating system: the network stack and the file system are
# separate components that a product chooses, and this port is written against one choice,
# lwIP's sockets API for UDP and littlefs for files (the design in docs/platform-abstraction.md
# lists FreeRTOS+TCP and FAT as the alternatives; either would be another port of network_ and
# files_freertos.cpp, not a change to the rest). The product builds the kernel, lwIP and
# littlefs itself, with its own configuration, and names them in ADL_PLATFORM_LINK_LIBRARIES'
# place: the project that builds this platform (tests/platform/freertos is the simulator's)
# adds them to adl_platform with target_link_libraries. The project's top-level CMake refuses
# -DADL_TARGET_PLATFORM=freertos and says where a FreeRTOS build starts (ADL_PLATFORM_PROJECT).

set(ADL_PLATFORM_DISPLAY_NAME "FreeRTOS")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS experimental)
set(ADL_PLATFORM_PROJECT "tests/platform/freertos (and docs/building.md for a product)")

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES
    freertos/paths_freertos.cpp
    freertos/freertos_common.cpp
    freertos/clock_freertos.cpp
    freertos/memory_freertos.cpp
    freertos/files_freertos.cpp
    freertos/threads_freertos.cpp
    freertos/network_freertos.cpp
    freertos/ipc_freertos.cpp
    freertos/host_freertos.cpp
    detail/ring_queue.cpp)

# The kernel, lwIP and littlefs are the product's targets, linked by the project that builds
# the platform; the fragment names none.
set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# No Qt application on FreeRTOS.
function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
