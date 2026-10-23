# SPDX-License-Identifier: GPL-3.0-or-later
# QNX Neutrino. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.
#
# STATUS: UNBUILT. The sources named below exist and have never been compiled
# against the QNX headers or run on QNX: nothing in this repository or its CI
# can do either. They are the shared POSIX implementations in posix/ plus
# qnx/os_qnx.cpp (the few places QNX differs) and qnx/paths_qnx.cpp, written
# from the QNX documentation. Configure with -DADL_ACCEPT_UNBUILT_PLATFORM=ON to
# build them, and expect to fix what the compiler finds. docs/platform-abstraction.md
# ("Status of each platform") says what is known and not known.

set(ADL_PLATFORM_DISPLAY_NAME "QNX Neutrino")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS unbuilt)

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES
    qnx/paths_qnx.cpp
    qnx/os_qnx.cpp
    posix/posix_common.cpp
    posix/clock_posix.cpp
    posix/memory_posix.cpp
    posix/files_posix.cpp
    posix/threads_posix.cpp
    posix/network_posix.cpp
    posix/host_posix.cpp
    detail/ring_queue.cpp)

# QNX keeps its BSD socket calls in libsocket.
set(ADL_PLATFORM_LINK_LIBRARIES socket)
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# No Qt application on QNX in this project.
function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
