# SPDX-License-Identifier: GPL-3.0-or-later
# INTEGRITY-178B, the certified Green Hills INTEGRITY. Included by
# cmake/Platform.cmake; the contract it fills in is described at the top of
# that file, and docs/platform-ghs.md is the review that shaped this.
#
# STATUS: UNBUILT. The sources exist and have never been compiled against the
# INTEGRITY-178B headers or run on a board: no GHS toolchain exists on this
# project's machines, and the supplied tree's application API is a subset that
# the port maps as best the headers and the vendor's own ARINC 615A module
# show. Configure with -DADL_ACCEPT_UNBUILT_PLATFORM=ON to build.
#
# The port is native INTEGRITY (no POSIX, no pthread): semaphores and the
# virtual-clock + WaitForActivity pattern for time-outs, ipflite for UDP, and a
# static memory provider. Two interfaces are unsupported on purpose, and say
# so: Threading::spawn (application tasks are configured into the kernel
# image, not created by code) and the whole FileSystem (the tree declares no
# application file API; a product supplies a storage adapter). See
# docs/platform-ghs.md for the audit behind those choices.

set(ADL_PLATFORM_DISPLAY_NAME "INTEGRITY-178B (GHS)")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS unbuilt)

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES
    ghs178b/paths_ghs178b.cpp
    ghs178b/memory_ghs178b.cpp
    ghs178b/clock_ghs178b.cpp
    ghs178b/threads_ghs178b.cpp
    ghs178b/files_ghs178b.cpp
    ghs178b/network_ghs178b.cpp
    ghs178b/host_ghs178b.cpp
    ghs/ghs_native.cpp
    detail/ring_queue.cpp)

# The kernel/BSP tree and the GHS toolchain; see the ghs508 fragment for why
# QUIET, and cmake/Platform.cmake for where a real build's error is raised.
find_package(Integrity178 QUIET)
set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")
if(Integrity178_FOUND)
    set(ADL_PLATFORM_LINK_LIBRARIES Integrity178::Runtime)
endif()
set(ADL_PLATFORM_PACKAGE_FOUND "${Integrity178_FOUND}")
set(ADL_PLATFORM_PACKAGE_NAME "the INTEGRITY-178B tree and the GHS toolchain")
set(ADL_PLATFORM_PACKAGE_HINT
    "Set INTEGRITY178_ROOT to the tree (the directory holding INTEGRITY-include/INTEGRITY.h, "
    "or its rtos- directory) and INTEGRITY_TOOLCHAIN_ROOT to MULTI, or put ccppc/cxppc on "
    "PATH (docs/platform-ghs.md).")

# No Qt application on INTEGRITY in this project.
function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
