# Linux. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "Linux")
set(ADL_PLATFORM_DESKTOP ON)
set(ADL_PLATFORM_STATUS supported)

set(ADL_PLATFORM_QT_SEARCH_ROOTS
    "$ENV{HOME}/Qt" "/opt/Qt" "/usr/lib/qt6" "/usr/lib/x86_64-linux-gnu/qt6")
set(ADL_PLATFORM_QT_ARCH_DIRS gcc_64)

# The implementations shared with the other POSIX systems are in posix/; what
# differs on Linux is linux/os_linux.cpp.
set(ADL_PLATFORM_SOURCES
    linux/paths_linux.cpp
    linux/os_linux.cpp
    posix/posix_common.cpp
    posix/clock_posix.cpp
    posix/memory_posix.cpp
    posix/files_posix.cpp
    posix/threads_posix.cpp
    posix/network_posix.cpp
    posix/host_posix.cpp
    detail/ring_queue.cpp)

# pthread: part of libc since glibc 2.34, a separate library before.
set(ADL_PLATFORM_LINK_LIBRARIES pthread)
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# An ordinary executable is already what a Linux desktop launches.
function(adl_platform_configure_app target)
endfunction()

# Nothing to deploy: Qt comes from the system or from CMAKE_PREFIX_PATH and is
# found at run time the same way.
function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
