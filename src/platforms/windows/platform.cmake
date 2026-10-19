# Windows. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "Windows")
set(ADL_PLATFORM_DESKTOP ON)
set(ADL_PLATFORM_STATUS supported)

# The third-party notices (notices/README.md): Qt's deployment tool places the Qt DLLs beside the
# executables, with the runtime files that came with the kit.
set(ADL_PLATFORM_NOTICE_FRAGMENTS windows-runtime)
set(ADL_PLATFORM_NOTICE_QT_DEPLOYMENT
    "Qt's deployment tool places the Qt6*.dll libraries, the plugins and the QML modules they load beside the executables.")
set(ADL_PLATFORM_NOTICE_EXPECTED "opengl32sw.dll|University of Illinois/NCSA|vc_redist.x64.exe")

# C:/Repos/common/qt is the labs' shared toolchain tree (the same kit the sibling projects'
# env.ps1 points QT_ROOT_DIR at); it lets an IDE that does not run that script - Visual Studio's
# CMake integration, for one - find the kit without the environment variable.
set(ADL_PLATFORM_QT_SEARCH_ROOTS "C:/Qt" "$ENV{USERPROFILE}/Qt" "C:/Repos/common/qt")
set(ADL_PLATFORM_QT_ARCH_DIRS msvc2022_64 msvc2019_64)

set(ADL_PLATFORM_SOURCES
    windows/paths_windows.cpp
    windows/windows_common.cpp
    windows/clock_windows.cpp
    windows/memory_windows.cpp
    windows/files_windows.cpp
    windows/threads_windows.cpp
    windows/network_windows.cpp
    windows/host_windows.cpp
    detail/ring_queue.cpp)

# Winsock. Everything else the platform uses is in kernel32.
set(ADL_PLATFORM_LINK_LIBRARIES ws2_32)

# <windows.h> without the parts and macros nobody asked for (min/max, the
# rarely used APIs) and with the wide-character API as the default. Passed by
# the build, so no source file declares them.
#
# _WIN32_WINNT names the oldest Windows the platform layer runs on, Windows 10
# (SetThreadDescription, GetSystemTimePreciseAsFileTime, WSAPoll).
set(ADL_PLATFORM_COMPILE_DEFINITIONS
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)

# A graphical program starts without a console window.
function(adl_platform_configure_app target)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON)
endfunction()

# Qt's DLLs are not next to the executable or on PATH by default, so it fails
# to start (STATUS_DLL_NOT_FOUND) unless launched from a shell that already has
# Qt's bin directory on PATH. windeployqt copies what the executable needs (Qt
# DLLs, platform and style plugins) next to it after every build, so it runs
# standalone however it is launched.
#
# <qml_dir> is the directory of the program's QML sources ("" for a program
# with none): windeployqt reads the imports in it to find which QML modules the
# program needs, and without it the window fails with "module QtQuick is not
# installed".
function(adl_platform_deploy_qt_runtime target qml_dir)
    find_program(ADL_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS "${Qt6_DIR}/../../../bin")
    if(NOT ADL_WINDEPLOYQT_EXECUTABLE)
        message(WARNING
            "windeployqt was not found; ${target} will only start from a shell "
            "that has Qt's bin directory on PATH.")
        return()
    endif()

    set(_arguments --no-translations)
    if(NOT "${qml_dir}" STREQUAL "")
        list(APPEND _arguments --qmldir "${qml_dir}")
    endif()
    # The applications deploy into one bin directory, and two windeployqt runs at once
    # write the same QML modules and plugins over each other (the likely cause of a clean
    # build failing on CI). Their link steps, which carry this command, share a pool of one.
    get_property(_pools GLOBAL PROPERTY JOB_POOLS)
    if(NOT "adl_qt_deploy=1" IN_LIST _pools)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS adl_qt_deploy=1)
    endif()
    set_property(TARGET ${target} PROPERTY JOB_POOL_LINK adl_qt_deploy)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${ADL_WINDEPLOYQT_EXECUTABLE}" ${_arguments} "$<TARGET_FILE:${target}>"
        COMMENT "Deploying the Qt runtime beside ${target}")
endfunction()
