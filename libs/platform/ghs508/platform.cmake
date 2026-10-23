# SPDX-License-Identifier: GPL-3.0-or-later
# INTEGRITY 5.0.8, the commercial Green Hills INTEGRITY. Included by
# cmake/Platform.cmake; the contract it fills in is described at the top of
# that file, and docs/platform-ghs.md is the review that shaped this.
#
# STATUS: UNBUILT. The sources exist and have never been compiled against the
# INTEGRITY 5.0.8 headers or run on a board: no GHS toolchain and no INTEGRITY
# installation exist on this project's machines. INTEGRITY 5.0.8 is POSIX, so
# the implementation is the shared posix/ sources plus ghs508/os_ghs508.cpp
# (the per-system functions) and ghs508/paths_ghs508.cpp. Configure with
# -DADL_ACCEPT_UNBUILT_PLATFORM=ON to build, and expect to fix what INTEGRITY's
# POSIX does differently; the candidates are listed in docs/platform-ghs.md.

set(ADL_PLATFORM_DISPLAY_NAME "INTEGRITY 5.0.8 (GHS)")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS unbuilt)

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES
    ghs508/paths_ghs508.cpp
    ghs508/os_ghs508.cpp
    posix/posix_common.cpp
    posix/clock_posix.cpp
    posix/memory_posix.cpp
    posix/files_posix.cpp
    posix/threads_posix.cpp
    posix/network_posix.cpp
    posix/host_posix.cpp
    detail/ring_queue.cpp)

# The installation, the board package and the GHS toolchain. QUIET: a fragment
# is also included by tests/platform/check_fragments.cmake in a bare cmake -P
# process, where a package that only a real build needs must not fail the
# check. cmake/Platform.cmake turns ADL_PLATFORM_PACKAGE_FOUND into the error
# a real build deserves. Integrity::Runtime carries the include directories,
# definitions, options and libraries (cmake/FindIntegrity.cmake).
find_package(Integrity QUIET)
set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")
if(Integrity_FOUND)
    set(ADL_PLATFORM_LINK_LIBRARIES Integrity::Runtime)
endif()
set(ADL_PLATFORM_PACKAGE_FOUND "${Integrity_FOUND}")
set(ADL_PLATFORM_PACKAGE_NAME "the INTEGRITY 5.0.8 installation and the GHS toolchain")
set(ADL_PLATFORM_PACKAGE_HINT
    "Set INTEGRITY_ROOT to the installation (the directory holding include/INTEGRITY.h), "
    "INTEGRITY_BSP_ROOT to the board package, and INTEGRITY_TOOLCHAIN_ROOT to MULTI, "
    "or put ccppc/cxppc on PATH (docs/platform-ghs.md).")

# No Qt application on INTEGRITY in this project.
function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
