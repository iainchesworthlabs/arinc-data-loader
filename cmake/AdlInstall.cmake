# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# AdlInstall.cmake - what `cmake --install` puts where, and the CMake package.
#
# Two packages come out of one install tree, as two install components:
#
#   runtime       the three Qt applications (adl_app, adl_media_gen, adl_mock_target) with the Qt
#                 runtime they need, adl_write_payloads, README.txt, LICENSE.txt and NOTICES.txt.
#   development   the libraries other programs can build on, their headers and the CMake package
#                 (find_package(adl) with the adl::... targets), and LICENSE.txt and NOTICES.txt.
#
# The development package is deliberately the libraries a consumer can use without Qt or an
# operating system header in sight, and the interfaces the platform section implements:
#
#   adl::arinc665          the ARINC 665 media set, load and check-value library
#   adl::arinc615a         the ARINC 615A codec and the data loader's state machines
#   adl::arinc615a_runner  the runner that drives the engine over adl::platform's sockets
#   adl::media_tree        a media set as a directory tree on disk
#   adl::platform          the platform interfaces and this host's implementation of them
#
# Not shipped: adl::samples and adl::arinc615a_mini are the fixtures the tests and the mock target
# are built on (a second, independent TFTP implementation, kept apart from the engine on purpose),
# not an interface anyone should depend on; the code behind the three applications (apps/app/core,
# libs/ui) is their implementation, written for Qt and for those programs. Where the operating system
# differs is not decided here: the destinations that depend on it come from the platform's fragment
# (libs/platform/<name>/platform.cmake), the one place that knows which operating system this is.
#
# The packages are made by CPack from these rules (cmake/AdlPackage.cmake).
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# A consumer's include and link paths do not depend on the host's GNU conventions (lib64, ...):
# the libraries are in lib and the headers in include on every platform. The executables go where
# the platform's fragment says (Windows beside the Qt DLLs, Linux in bin).
set(CMAKE_INSTALL_LIBDIR "lib")
if(ADL_PLATFORM_DESKTOP)
    set(CMAKE_INSTALL_BINDIR "${ADL_PLATFORM_INSTALL_BINDIR}")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The version the packages are named by. CPack's own version has to be numeric, the project's; a
# release candidate's packages carry its suffix: -DADL_RELEASE_VERSION=0.2.0-rc.1 (the release
# workflow passes the tag's version).
set(ADL_RELEASE_VERSION "${PROJECT_VERSION}" CACHE STRING
    "The version in the names of the packages and in README.txt (default: the project's)")

# The CMake package is installed under lib/cmake/adl on every platform.
set(ADL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/adl")

# adl_install_library(<target> INCLUDE_DIR <dir>)
#
# Installs one of the static libraries and the headers under <dir> into the development component,
# and exports it as adl::<name> (the target adl_<name>, as its alias adl::<name> already says). The
# library's CMakeLists.txt wraps what only a build in this tree can use - its include directory in
# the source tree, and the interface targets that carry warnings, sanitizers and coverage - in
# $<BUILD_INTERFACE:> and $<BUILD_LOCAL_INTERFACE:>, and adds $<INSTALL_INTERFACE:include>, so the
# installed target names none of them.
function(adl_install_library target)
    if(NOT ADL_PLATFORM_DESKTOP)
        return()
    endif()
    cmake_parse_arguments(PARSE_ARGV 1 L "" "INCLUDE_DIR" "")
    if(NOT L_INCLUDE_DIR)
        message(FATAL_ERROR "adl_install_library(${target}): INCLUDE_DIR is required")
    endif()
    string(REGEX REPLACE "^adl_" "" export_name "${target}")
    set_target_properties(${target} PROPERTIES EXPORT_NAME "${export_name}")

    install(DIRECTORY "${L_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT development
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")
    install(TARGETS ${target}
        EXPORT adlTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT development)
endfunction()

# adl_install_package_config()
#
# Writes and installs adlConfig.cmake, adlConfigVersion.cmake and the exported targets, for the
# libraries adl_install_library was called on. Call once, after the last of them. A version before
# 1.0 promises no compatibility across a minor version, which is what SameMinorVersion says.
function(adl_install_package_config)
    set(config_in "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/adlConfig.cmake.in")
    set(generated "${CMAKE_BINARY_DIR}/generated/adl-package")
    configure_package_config_file("${config_in}" "${generated}/adlConfig.cmake"
        INSTALL_DESTINATION "${ADL_INSTALL_CMAKEDIR}")
    write_basic_package_version_file("${generated}/adlConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion)
    install(EXPORT adlTargets
        NAMESPACE adl::
        DESTINATION "${ADL_INSTALL_CMAKEDIR}"
        COMPONENT development)
    install(FILES "${generated}/adlConfig.cmake" "${generated}/adlConfigVersion.cmake"
        DESTINATION "${ADL_INSTALL_CMAKEDIR}"
        COMPONENT development)
endfunction()

# adl_install_application(<target> <qml_dir>)
#
# Installs one of the Qt applications into the runtime component: the executable (an application
# bundle on macOS) where the platform keeps programs, and what that platform needs to make it start
# on a machine with no Qt (adl_platform_install_app: Qt's own deployment tool, run at install time on
# the installed program). <qml_dir> is the directory of the program's QML sources, the same one
# adl_platform_deploy_qt_runtime takes.
function(adl_install_application target qml_dir)
    install(TARGETS ${target}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime
        BUNDLE DESTINATION "." COMPONENT runtime)
    adl_platform_install_app(${target} "${qml_dir}")
endfunction()

# adl_qt_deploy_script(<target> <out-var>)
#
# Generates, with Qt's own deployment support, the script that copies the Qt files <target> needs into
# the install tree (windeployqt on Windows, macdeployqt on macOS, a copy of the shared libraries Qt
# uses on Linux), and puts its path in <out-var> for `install(SCRIPT ...)`. The platform fragments
# call it from adl_platform_install_app. The Qt translations are left out (the programs carry their own catalogues), and so are the
# QML debugging plugins: a released program has no use for a plugin that opens a debugging
# connection when it is asked to. The offscreen platform plugin is added to
# the one the system uses: it is a few kilobytes, and it lets the packages be started and tested on a
# machine with no display, which is how a person runs a program in a container or over ssh too.
# On Linux, what the system itself provides (libc, libstdc++, X11, OpenGL, fontconfig, ...) is left to
# the system: Qt's script copies only libraries from outside the system directories, which is Qt and its
# ICU.
function(adl_qt_deploy_script target out_var)
    qt_generate_deploy_qml_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT script
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
        EXCLUDE_PLUGIN_TYPES qmltooling
        INCLUDE_PLUGINS qoffscreen)
    set(${out_var} "${script}" PARENT_SCOPE)
endfunction()

# The licences travel with every package, and with the Qt libraries in particular: the LGPL asks for
# its text and the offer of Qt's source to accompany them. The runtime package carries them beside the
# programs; on macOS also inside each application bundle (adl_platform_install_app), since a
# bundle is what gets dragged out of the disk image.
function(adl_install_licences)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${ADL_PLATFORM_INSTALL_DOCDIR}"
        RENAME LICENSE.txt
        COMPONENT runtime)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${ADL_PLATFORM_INSTALL_DOCDIR}"
        RENAME LICENSE.txt
        COMPONENT development)
    if(ADL_UI_NOTICES_FILE AND EXISTS "${ADL_UI_NOTICES_FILE}")
        install(FILES "${ADL_UI_NOTICES_FILE}"
            DESTINATION "${ADL_PLATFORM_INSTALL_DOCDIR}"
            RENAME NOTICES.txt
            COMPONENT runtime)
        install(FILES "${ADL_UI_NOTICES_FILE}"
            DESTINATION "${ADL_PLATFORM_INSTALL_DOCDIR}"
            RENAME NOTICES.txt
            COMPONENT development)
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/packaging/README.txt.in"
        "${CMAKE_BINARY_DIR}/generated/README.txt" @ONLY NEWLINE_STYLE LF)
    install(FILES "${CMAKE_BINARY_DIR}/generated/README.txt"
        DESTINATION "${ADL_PLATFORM_INSTALL_DOCDIR}"
        COMPONENT runtime)
endfunction()
