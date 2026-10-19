# ---------------------------------------------------------------------------
# Platform.cmake - the ONE place the target operating system is decided.
#
# This file maps the toolchain's own flags (WIN32, APPLE, UNIX) to a platform
# name, then includes that platform's fragment:
#
#     src/platforms/<name>/platform.cmake
#
# Everything the platform contributes - its source files, system libraries,
# compile definitions and deployment steps - is declared in that fragment, and
# every other CMake file reads it through the variables and hooks below. No
# other CMake file tests the OS, and no source file tests it either
# (docs/platform-isolation.md; tools/checks/check_platform_isolation.ps1 holds
# the source side of that in CI).
#
# Adding a platform is therefore two edits: a src/platforms/<name>/ directory
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
# Build inputs (paths are relative to src/platforms):
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

if(ADL_TARGET_PLATFORM STREQUAL "windows")
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
        "To add a platform, add src/platforms/<name>/ with a platform.cmake (the contract is at "
        "the top of cmake/Platform.cmake) and a branch for it in this file.")
endif()

set(ADL_PLATFORM_DIR "${PROJECT_SOURCE_DIR}/src/platforms/${ADL_PLATFORM}")
include("${ADL_PLATFORM_DIR}/platform.cmake")

# A fragment that forgets part of the contract should fail here, by name, not
# as an empty source list or a missing-command error three files away.
include(PlatformContract)
adl_platform_check_contract("${ADL_PLATFORM_DIR}/platform.cmake"
    "${PROJECT_SOURCE_DIR}/src/platforms")

# A platform nobody has built is not silently built.
if(ADL_PLATFORM_STATUS STREQUAL "design-only")
    message(FATAL_ERROR
        "The ${ADL_PLATFORM_DISPLAY_NAME} platform is a design only: src/platforms/${ADL_PLATFORM}/ "
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
