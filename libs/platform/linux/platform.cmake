# SPDX-License-Identifier: GPL-3.0-or-later
# Linux. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.

set(ADL_PLATFORM_DISPLAY_NAME "Linux")
set(ADL_PLATFORM_DESKTOP ON)
set(ADL_PLATFORM_STATUS supported)

# The third-party notices (notices/README.md): Qt is the installation the programs were built
# against, or one deployed with them; nothing else is placed beside them.
set(ADL_PLATFORM_NOTICE_FRAGMENTS "")
set(ADL_PLATFORM_NOTICE_QT_DEPLOYMENT
    "The programs load the Qt shared libraries of the Qt installation they were built against, or the ones deployed with them.")
set(ADL_PLATFORM_NOTICE_EXPECTED "")

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

# ---- packaging (cmake/AdlInstall.cmake, cmake/AdlPackage.cmake) ----------------------------------
# A relocatable tree: bin, lib (the libraries and the Qt files deployed with the programs), plugins,
# qml, and the desktop entries and icons under share.
set(ADL_PLATFORM_INSTALL_BINDIR "bin")
set(ADL_PLATFORM_INSTALL_DOCDIR "share/doc/arinc-data-loader")

# What CPack makes: a tar.gz of the runtime component and one of the development component. No
# distribution package: one that carries its own Qt belongs to a maintainer who can keep it patched
# for a distribution's libraries, and an AppImage needs a build tool fetched at packaging time that
# this repository has no pin for yet (docs/releasing.md).
set(ADL_PLATFORM_PACKAGE_RUNTIME_GENERATORS TGZ)
set(ADL_PLATFORM_PACKAGE_DEVELOPMENT_GENERATORS TGZ)

# The program finds the Qt libraries deployed beside it (../lib); the desktop entry and the icon go
# where a desktop looks (share/applications, share/icons) once the tree is unpacked under a prefix;
# and Qt's deployment script copies the Qt libraries, the platform plugins and the QML modules the
# program imports into the tree.
function(adl_platform_install_app target qml_dir)
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/${target}.desktop"
        DESTINATION "share/applications" COMPONENT runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/icons/${target}.svg"
        DESTINATION "share/icons/hicolor/scalable/apps" COMPONENT runtime)
    adl_qt_deploy_script(${target} deploy_script)
    install(SCRIPT "${deploy_script}" COMPONENT runtime)
endfunction()

# CPack settings only this platform has (none for a tar.gz).
function(adl_platform_configure_packaging)
endfunction()
