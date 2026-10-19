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

# A graphical program on macOS is an application bundle, with an identity of its own: the identifier
# is what the system knows it by, the name is what the Finder and the menu bar show. The oldest system
# the bundle runs on is CMAKE_OSX_DEPLOYMENT_TARGET, which the macOS presets set.
function(adl_platform_configure_app target)
    set(_names
        adl_app "ARINC Data Loader"
        adl_media_gen "ARINC Media Set Generator"
        adl_mock_target "ARINC Mock Target")
    list(FIND _names "${target}" _at)
    if(_at EQUAL -1)
        set(_display_name "${target}")
    else()
        math(EXPR _at "${_at} + 1")
        list(GET _names ${_at} _display_name)
    endif()
    string(REPLACE "_" "-" _identifier_leaf "${target}")
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME "${_display_name}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.iainchesworthlabs.arinc-data-loader.${_identifier_leaf}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Free software under the GNU General Public License, version 3 or later")
endfunction()

# Nothing to deploy beside the program in the build tree: a bundle there runs against the Qt
# installation it was built with (Qt's deploy support writes a qt.conf that points at it). What a
# bundle needs to run on another Mac is added when it is installed (adl_platform_install_app below):
# macdeployqt copies the frameworks, the plugins and the QML modules the program imports into it.
function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()

# ---- packaging (cmake/AdlInstall.cmake, cmake/AdlPackage.cmake) ----------------------------------
# The disk image holds the three bundles in its top directory, the command-line tool in tools and the
# licence files beside them.
set(ADL_PLATFORM_INSTALL_BINDIR "tools")
set(ADL_PLATFORM_INSTALL_DOCDIR ".")

# What CPack makes: a disk image (DragNDrop) of the runtime component and a tar.gz of the development
# component. The bundles are signed ad hoc by Qt's deployment tool, which is what an Apple-silicon Mac
# needs to run them at all; there is no Developer ID signature and no notarisation, so Gatekeeper asks
# the user to allow them the first time (docs/verifying-releases.md says how).
set(ADL_PLATFORM_PACKAGE_RUNTIME_GENERATORS DragNDrop)
set(ADL_PLATFORM_PACKAGE_DEVELOPMENT_GENERATORS TGZ)

# The licences go inside each bundle too (Contents/Resources): the bundle is what gets dragged out of
# the disk image, and the Qt frameworks inside it are what the LGPL's notice must accompany. Then Qt's
# deployment script runs macdeployqt on the installed bundle, copying the Qt frameworks, the platform
# plugins and the QML modules the program imports into it.
function(adl_platform_install_app target qml_dir)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${target}.app/Contents/Resources" RENAME LICENSE.txt COMPONENT runtime)
    install(FILES "${ADL_UI_NOTICES_FILE}"
        DESTINATION "${target}.app/Contents/Resources" RENAME NOTICES.txt COMPONENT runtime)
    adl_qt_deploy_script(${target} deploy_script)
    install(SCRIPT "${deploy_script}" COMPONENT runtime)
endfunction()

# CPack settings only this platform has: the disk image's format and volume name.
function(adl_platform_configure_packaging)
    set(CPACK_DMG_FORMAT "UDZO" PARENT_SCOPE)
    set(CPACK_DMG_VOLUME_NAME "ARINC Data Loader ${ADL_RELEASE_VERSION}" PARENT_SCOPE)
endfunction()
