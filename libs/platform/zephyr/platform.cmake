# Zephyr. Included by cmake/Platform.cmake and by this directory's own CMakeLists.txt (the
# Zephyr module); the contract it fills in is described at the top of cmake/Platform.cmake.
#
# STATUS: experimental. The code is built and the platform conformance suite and the 615A
# engine tests are run on Zephyr's native_sim board (a host process with Zephyr's own kernel,
# network stack and file system: tests/platform/zephyr, CI job zephyr-native-sim). No target
# hardware has run it, and the boards a product uses decide the things native_sim cannot show
# (priorities against that board's network threads, a real file system on real flash, a link
# layer that can broadcast). docs/platform-abstraction.md says what is and is not verified.
#
# Zephyr builds a product as one image with its own CMake package (find_package(Zephyr)), Kconfig
# and devicetree, so this platform is a Zephyr module, not a library this project's CMake
# links: zephyr/module.yml at the repository root names libs/platform/zephyr as the module's
# CMake directory, and CMakeLists.txt there builds the sources below with Zephyr's flags when
# CONFIG_ADL_PLATFORM is on. The project's top-level CMake refuses -DADL_TARGET_PLATFORM=zephyr
# and says where a Zephyr build starts (ADL_PLATFORM_PROJECT).

set(ADL_PLATFORM_DISPLAY_NAME "Zephyr")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS experimental)
set(ADL_PLATFORM_PROJECT "tests/platform/zephyr (and docs/building.md for a product)")

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES
    zephyr/paths_zephyr.cpp
    zephyr/zephyr_common.cpp
    zephyr/clock_zephyr.cpp
    zephyr/memory_zephyr.cpp
    zephyr/files_zephyr.cpp
    zephyr/threads_zephyr.cpp
    zephyr/network_zephyr.cpp
    zephyr/ipc_zephyr.cpp
    zephyr/host_zephyr.cpp
    detail/ring_queue.cpp)

# The kernel, the network stack and the file system come from Zephyr's own build, through
# the module's Kconfig; nothing is linked by name.
set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# No Qt application on Zephyr.
function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
