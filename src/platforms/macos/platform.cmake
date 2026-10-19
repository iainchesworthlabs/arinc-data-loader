# macOS. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "macOS")
set(ADL_PLATFORM_DESKTOP ON)
set(ADL_PLATFORM_STATUS supported)

# The third-party notices (notices/README.md): Qt is the installation the programs were built
# against, or one deployed with them; nothing else is placed beside them.
set(ADL_PLATFORM_NOTICE_FRAGMENTS "")
set(ADL_PLATFORM_NOTICE_QT_DEPLOYMENT
    "The programs load the Qt frameworks of the Qt installation they were built against, or the ones deployed with them.")
set(ADL_PLATFORM_NOTICE_EXPECTED "")

set(ADL_PLATFORM_QT_SEARCH_ROOTS
    "$ENV{HOME}/Qt" "/opt/Qt" "/usr/local/opt/qt6" "/opt/homebrew/opt/qt6")
set(ADL_PLATFORM_QT_ARCH_DIRS macos clang_64)

# The implementations shared with the other POSIX systems are in posix/; what
# differs on macOS is macos/os_macos.cpp.
set(ADL_PLATFORM_SOURCES
    macos/paths_macos.cpp
    macos/os_macos.cpp
    posix/posix_common.cpp
    posix/clock_posix.cpp
    posix/memory_posix.cpp
    posix/files_posix.cpp
    posix/threads_posix.cpp
    posix/network_posix.cpp
    posix/host_posix.cpp
    detail/ring_queue.cpp)

set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# A graphical program on macOS is an application bundle.
function(adl_platform_configure_app target)
    set_target_properties(${target} PROPERTIES MACOSX_BUNDLE ON)
endfunction()

# Nothing to deploy yet: the application bundle gets macdeployqt with the
# packaging work (docs/roadmap.md).
function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
