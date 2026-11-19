# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# Platform.cmake - the ONE place the target operating system is decided.
#
# This file maps the toolchain's own flags (WIN32, APPLE, UNIX) to a platform
# name, then includes that platform's fragment:
#
#     libs/platform/<name>/platform.cmake
#
# Everything the platform contributes - its source files, system libraries,
# compile definitions and deployment steps - is declared in that fragment, and
# every other CMake file reads it through the variables and hooks below. No
# other CMake file tests the OS, and no source file tests it either
# (docs/platform-isolation.md; tools/checks/check_platform_isolation.ps1 holds
# the source side of that in CI).
#
# Adding a platform is therefore two edits: a libs/platform/<name>/ directory
# (its implementations plus a platform.cmake that fills in the contract below),
# and one branch in the selector here.
#
# How the selector learns the target
# ----------------------------------
# A desktop build needs nothing: WIN32, APPLE and UNIX say which it is. An RTOS
# build is a cross build, and CMake has no system name for most of them
# (FreeRTOS, Zephyr and VxWorks toolchains say "Generic", or whatever the
# toolchain file chose), so the target is named explicitly:
#
#     cmake -DADL_TARGET_PLATFORM=freertos --toolchain <toolchain file> ...
#
# ADL_TARGET_PLATFORM, when set, wins over detection. It is the one cache
# variable that names a platform, and this file the one place that reads it.
# The names it accepts are the branches below; QNX is also recognised from
# CMAKE_SYSTEM_NAME.
#
# A platform kept outside the repository
# --------------------------------------
# ADL_PLATFORM_DIR, a cache variable that is empty unless someone sets it, points at a directory
# outside the source tree that holds a platform's platform.cmake and its sources:
#
#     cmake -DADL_TARGET_PLATFORM=<name> -DADL_PLATFORM_DIR=<directory> ...
#
# ADL_TARGET_PLATFORM then gives the platform's name, which need not be one of the branches
# below, and the fragment is <directory>/platform.cmake in place of
# libs/platform/<name>/platform.cmake. Nothing else about the selection changes: the fragment
# is held to the contract below, and its ADL_PLATFORM_STATUS is acted on as for any platform.
# Without ADL_PLATFORM_DIR nothing here differs from what it was.
#
# Inside the build these variables are set for the fragment and for every other CMake file:
#   ADL_PLATFORM_ROOT   The directory libs/platform of this repository, the home of the shared
#                       sources (posix/, detail/) and the interface headers (interfaces/).
#   ADL_PLATFORM_DIR    The directory of the selected platform: the one in the cache when it is set,
#                       and libs/platform/<name> when it is not.
#   ADL_PLATFORM_EXTERNAL
#                       ON when ADL_PLATFORM_DIR was given, OFF otherwise.
#
# A path in ADL_PLATFORM_SOURCES that is relative is relative to ADL_PLATFORM_ROOT, as ever, so
# a fragment outside the repository names the shared sources the way the in-tree ones do
# (posix/posix_common.cpp) and its own files by absolute path (${ADL_PLATFORM_DIR}/paths.cpp).
# docs/platform-abstraction.md says what is and is not checked for such a platform.
#
# A platform whose fragment says ADL_PLATFORM_STATUS "unbuilt" has code nobody has
# compiled; one that says "design-only" has none. Both are refused unless
# ADL_ACCEPT_UNBUILT_PLATFORM is ON, and a design-only one is refused even then,
# because there is nothing to build (docs/platform-abstraction.md says what each
# platform has).
#
# The contract a fragment fills in (checked by cmake/PlatformContract.cmake)
# ------------------------------------------------------------------------
# Facts:
#   ADL_PLATFORM_DISPLAY_NAME     How the platform is written in the version
#                                 block ("Windows", "macOS", "Linux").
#   ADL_PLATFORM_STATUS           "supported": built and tested by CI;
#                                 "experimental": built, and its tests run, in
#                                 CI on a simulator only (no target hardware);
#                                 "unbuilt": code exists that nobody has
#                                 compiled; "design-only": no code, only the
#                                 documented design.
#   ADL_PLATFORM_DESKTOP          ON where installers, the Qt application and
#                                 the third-party notices exist.
#   ADL_PLATFORM_QT_SEARCH_ROOTS, ADL_PLATFORM_QT_ARCH_DIRS
#                                 Where cmake/FindQt6.cmake looks for an
#                                 aqtinstall-style Qt when none is on
#                                 CMAKE_PREFIX_PATH.
#
# Optional fact:
#   ADL_PLATFORM_PROJECT          Set by a platform whose image is built by a
#                                 project of its own (Zephyr's find_package,
#                                 an RTOS simulator's main) and not by this
#                                 one: where that project is. This project
#                                 refuses to configure for it and says so.
#
# Facts a desktop platform (ADL_PLATFORM_DESKTOP ON) adds, for the third-party notices the
# applications show (cmake/AdlUi.cmake, notices/README.md):
#   ADL_PLATFORM_NOTICE_FRAGMENTS    Fragments (notices/fragments/<name>.txt) this platform
#                                    adds after the Qt section, e.g. the runtime files
#                                    Qt's deployment tool places on Windows. May be empty.
#   ADL_PLATFORM_NOTICE_QT_DEPLOYMENT
#                                    One sentence on where the programs get Qt from.
#   ADL_PLATFORM_NOTICE_EXPECTED     Strings, separated by "|", the generated notices must
#                                    contain on this platform (tests/system/check_notices.cmake).
#
# ... and for the packages built from the install tree (cmake/AdlInstall.cmake,
# cmake/AdlPackage.cmake):
#   ADL_PLATFORM_INSTALL_BINDIR      Where the programs go in the install tree ("." for a portable
#                                    folder, "bin", "tools" beside application bundles).
#   ADL_PLATFORM_INSTALL_DOCDIR      Where LICENSE.txt, NOTICES.txt and README.txt go.
#   ADL_PLATFORM_PACKAGE_RUNTIME_GENERATORS
#   ADL_PLATFORM_PACKAGE_DEVELOPMENT_GENERATORS
#                                    CPack generators for the runtime component (the programs, with
#                                    the Qt files they need) and the development component (the
#                                    libraries, their headers and the CMake package).
#
# Build inputs (a relative path is relative to libs/platform, ADL_PLATFORM_ROOT; an
# absolute path is used as it is, which is how a platform outside the repository names
# its own files):
#   ADL_PLATFORM_SOURCES          Compiled into adl_platform: one
#                                 implementation of every interfaces/ header.
#   ADL_PLATFORM_LINK_LIBRARIES   System libraries adl_platform needs.
#   ADL_PLATFORM_COMPILE_DEFINITIONS
#                                 Compile definitions for adl_platform (the
#                                 only target that includes OS headers).
#
# Hooks (every platform defines all of them, even as no-ops):
#   adl_platform_configure_app(<target>)
#                                 What makes <target>, the Qt application, a
#                                 program of this desktop: a GUI-subsystem
#                                 executable on Windows, an application bundle
#                                 on macOS.
#   adl_platform_deploy_qt_runtime(<target> <qml_dir>)
#                                 Puts Qt's runtime next to the built program,
#                                 including the QML modules imported by the
#                                 QML sources in <qml_dir> ("" if it has none).
#
# Hooks a desktop platform adds (a platform with ADL_PLATFORM_DESKTOP ON defines both):
#   adl_platform_install_app(<target> <qml_dir>)
#                                 What `cmake --install` does for <target> besides installing it:
#                                 the Qt runtime it needs to start on a machine with no Qt (Qt's
#                                 deploy script), the licence files inside a bundle, a desktop
#                                 entry and icon. It installs into the "runtime" component.
#   adl_platform_configure_packaging()
#                                 Sets, in the parent scope, the CPack variables only this platform
#                                 has (a disk image's format, ...). May be empty.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# Phones and the browser are not targets (docs/roadmap.md). Saying so here is
# clearer than a missing-fragment error further down.
if(ANDROID OR IOS OR EMSCRIPTEN)
    message(FATAL_ERROR
        "Only Windows, macOS and Linux desktops are supported targets today, with "
        "QNX, INTEGRITY, VxWorks, FreeRTOS and Zephyr designed for "
        "(docs/platform-abstraction.md, docs/platform-ghs.md).")
endif()

set(ADL_TARGET_PLATFORM "" CACHE STRING
    "The platform to build for when the toolchain does not say (an RTOS cross build): windows, linux, macos, qnx, ghs508, ghs178b, vxworks, freertos or zephyr")
set_property(CACHE ADL_TARGET_PLATFORM PROPERTY STRINGS
    "" windows linux macos qnx ghs508 ghs178b vxworks freertos zephyr)
option(ADL_ACCEPT_UNBUILT_PLATFORM
    "Allow building for a platform whose code has never been compiled (status unbuilt)" OFF)
set(ADL_PLATFORM_DIR "" CACHE PATH
    "A directory outside the source tree that holds the platform ADL_TARGET_PLATFORM names (its platform.cmake and its sources). Empty: the platform is libs/platform/<name>")

# The directory libs/platform of this repository: the shared sources and the interface headers
# an outside platform builds on.
set(ADL_PLATFORM_ROOT "${PROJECT_SOURCE_DIR}/libs/platform")

# Read before anything below sets the variable of the same name (the directory of the selected
# platform, which is this one when it is given and libs/platform/<name> when it is not).
set(_adl_platform_dir_given "${ADL_PLATFORM_DIR}")
set(ADL_PLATFORM_EXTERNAL OFF)

if(NOT _adl_platform_dir_given STREQUAL "")
    set(ADL_PLATFORM_EXTERNAL ON)
    if(ADL_TARGET_PLATFORM STREQUAL "")
        message(FATAL_ERROR
            "ADL_PLATFORM_DIR is '${_adl_platform_dir_given}', but ADL_TARGET_PLATFORM is not set. "
            "Name the platform that directory holds: -DADL_TARGET_PLATFORM=<name>.")
    endif()
    if(NOT ADL_TARGET_PLATFORM MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR
            "ADL_TARGET_PLATFORM is '${ADL_TARGET_PLATFORM}', which cannot name a platform in "
            "ADL_PLATFORM_DIR. Use lower-case letters, digits and underscores, starting with a letter.")
    endif()
    # A relative path that CMake has not already made absolute (it does for -D on the command
    # line) is taken from the source directory.
    file(TO_CMAKE_PATH "${_adl_platform_dir_given}" _adl_platform_dir_given)
    get_filename_component(_adl_platform_dir_given "${_adl_platform_dir_given}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${_adl_platform_dir_given}")
        message(FATAL_ERROR
            "ADL_PLATFORM_DIR is '${_adl_platform_dir_given}', which is not a directory.")
    endif()
    if(NOT EXISTS "${_adl_platform_dir_given}/platform.cmake")
        message(FATAL_ERROR
            "ADL_PLATFORM_DIR is '${_adl_platform_dir_given}', which has no platform.cmake. "
            "The directory of a platform holds its fragment, the file that fills in the contract "
            "described at the top of cmake/Platform.cmake, and the sources the fragment names.")
    endif()
    set(ADL_PLATFORM "${ADL_TARGET_PLATFORM}")
    set(ADL_PLATFORM_DIR "${_adl_platform_dir_given}")
elseif(ADL_TARGET_PLATFORM STREQUAL "windows")
    set(ADL_PLATFORM windows)
elseif(ADL_TARGET_PLATFORM STREQUAL "macos")
    set(ADL_PLATFORM macos)
elseif(ADL_TARGET_PLATFORM STREQUAL "linux")
    set(ADL_PLATFORM linux)
elseif(ADL_TARGET_PLATFORM STREQUAL "qnx")
    set(ADL_PLATFORM qnx)
elseif(ADL_TARGET_PLATFORM STREQUAL "ghs508")
    set(ADL_PLATFORM ghs508)
elseif(ADL_TARGET_PLATFORM STREQUAL "ghs178b")
    set(ADL_PLATFORM ghs178b)
elseif(ADL_TARGET_PLATFORM STREQUAL "vxworks")
    set(ADL_PLATFORM vxworks)
elseif(ADL_TARGET_PLATFORM STREQUAL "freertos")
    set(ADL_PLATFORM freertos)
elseif(ADL_TARGET_PLATFORM STREQUAL "zephyr")
    set(ADL_PLATFORM zephyr)
elseif(NOT ADL_TARGET_PLATFORM STREQUAL "")
    message(FATAL_ERROR
        "ADL_TARGET_PLATFORM is '${ADL_TARGET_PLATFORM}', which is not a platform. "
        "The choices are windows, linux, macos, qnx, ghs508, ghs178b, vxworks, "
        "freertos and zephyr.")
elseif(WIN32)
    set(ADL_PLATFORM windows)
elseif(APPLE)
    set(ADL_PLATFORM macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(ADL_PLATFORM qnx)
elseif(UNIX)
    set(ADL_PLATFORM linux)
else()
    message(FATAL_ERROR
        "Cannot tell which platform this is. For an RTOS or other cross build, name it: "
        "-DADL_TARGET_PLATFORM=<windows|linux|macos|qnx|ghs508|ghs178b|vxworks|freertos|zephyr>. "
        "To add a platform, add libs/platform/<name>/ with a platform.cmake (the contract is at "
        "the top of cmake/Platform.cmake) and a branch for it in this file.")
endif()

if(ADL_PLATFORM_EXTERNAL)
    set(_adl_platform_where "${ADL_PLATFORM_DIR}/")
else()
    set(ADL_PLATFORM_DIR "${ADL_PLATFORM_ROOT}/${ADL_PLATFORM}")
    set(_adl_platform_where "libs/platform/${ADL_PLATFORM}/")
endif()
unset(_adl_platform_dir_given)
include("${ADL_PLATFORM_DIR}/platform.cmake")

# A fragment that forgets part of the contract should fail here, by name, not
# as an empty source list or a missing-command error three files away.
include(PlatformContract)
adl_platform_check_contract("${ADL_PLATFORM_DIR}/platform.cmake" "${ADL_PLATFORM_ROOT}")

# A platform whose program is built by a project of its own is not built by this one.
if(ADL_PLATFORM_PROJECT)
    message(FATAL_ERROR
        "The ${ADL_PLATFORM_DISPLAY_NAME} platform is built by a project of its own, not by "
        "this one: ${ADL_PLATFORM_PROJECT}. docs/building.md says how.")
endif()

# A platform nobody has built is not silently built.
if(ADL_PLATFORM_STATUS STREQUAL "design-only")
    message(FATAL_ERROR
        "The ${ADL_PLATFORM_DISPLAY_NAME} platform is a design only: ${_adl_platform_where} "
        "has no implementation to build. See docs/platform-abstraction.md and "
        "docs/platform-ghs.md for what a port needs.")
elseif(ADL_PLATFORM_STATUS STREQUAL "unbuilt" AND NOT ADL_ACCEPT_UNBUILT_PLATFORM)
    message(FATAL_ERROR
        "The ${ADL_PLATFORM_DISPLAY_NAME} platform has code that has never been compiled or run "
        "(docs/platform-abstraction.md, 'Status of each platform'). To try it anyway, "
        "configure with -DADL_ACCEPT_UNBUILT_PLATFORM=ON.")
endif()

# A platform that needs a vendor package says whether it found it, so a build
# without the INTEGRITY installation fails here by name rather than in the
# first source file that includes a header which is not there. A fragment in a
# bare cmake -P process (tests/platform/check_fragments.cmake) must not fail
# for a package that only a real build needs.
if(DEFINED ADL_PLATFORM_PACKAGE_FOUND AND NOT ADL_PLATFORM_PACKAGE_FOUND
   AND NOT CMAKE_SCRIPT_MODE_FILE)
    message(FATAL_ERROR
        "The ${ADL_PLATFORM_DISPLAY_NAME} platform needs ${ADL_PLATFORM_PACKAGE_NAME}, "
        "which was not found. ${ADL_PLATFORM_PACKAGE_HINT}")
endif()

unset(_adl_platform_where)
