# Linux. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "Linux")
set(ADL_PLATFORM_DESKTOP ON)

set(ADL_PLATFORM_QT_SEARCH_ROOTS
    "$ENV{HOME}/Qt" "/opt/Qt" "/usr/lib/qt6" "/usr/lib/x86_64-linux-gnu/qt6")
set(ADL_PLATFORM_QT_ARCH_DIRS gcc_64)

set(ADL_PLATFORM_SOURCES
    linux/paths_linux.cpp)

set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

# An ordinary executable is already what a Linux desktop launches.
function(adl_platform_configure_app target)
endfunction()

# Nothing to deploy: Qt comes from the system or from CMAKE_PREFIX_PATH and is
# found at run time the same way.
function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
