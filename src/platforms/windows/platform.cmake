# Windows. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "Windows")
set(ADL_PLATFORM_DESKTOP ON)

set(ADL_PLATFORM_QT_SEARCH_ROOTS "C:/Qt" "$ENV{USERPROFILE}/Qt")
set(ADL_PLATFORM_QT_ARCH_DIRS msvc2022_64 msvc2019_64)

set(ADL_PLATFORM_SOURCES
    windows/paths_windows.cpp)

set(ADL_PLATFORM_LINK_LIBRARIES "")

# <windows.h> without the parts and macros nobody asked for (min/max, the
# rarely used APIs) and with the wide-character API as the default. Passed by
# the build, so no source file declares them.
set(ADL_PLATFORM_COMPILE_DEFINITIONS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

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
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${ADL_WINDEPLOYQT_EXECUTABLE}" ${_arguments} "$<TARGET_FILE:${target}>"
        COMMENT "Deploying the Qt runtime beside ${target}")
endfunction()
