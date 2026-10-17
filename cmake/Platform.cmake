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
# The contract a fragment fills in (checked by cmake/PlatformContract.cmake)
# ------------------------------------------------------------------------
# Facts:
#   ADL_PLATFORM_DISPLAY_NAME     How the platform is written in the version
#                                 block ("Windows", "macOS", "Linux").
#   ADL_PLATFORM_DESKTOP          ON where installers, the Qt application and
#                                 the third-party notices exist.
#   ADL_PLATFORM_QT_SEARCH_ROOTS, ADL_PLATFORM_QT_ARCH_DIRS
#                                 Where cmake/FindQt6.cmake looks for an
#                                 aqtinstall-style Qt when none is on
#                                 CMAKE_PREFIX_PATH.
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

# Phones and the browser are not targets (docs/roadmap.md), and the embedded
# target (VxWorks, for the loader library) is not built yet. Saying so here is
# clearer than a missing-fragment error further down.
if(ANDROID OR IOS OR EMSCRIPTEN)
    message(FATAL_ERROR
        "Only Windows, macOS and Linux desktops are supported targets today. "
        "An embedded (VxWorks) target is on the roadmap (docs/roadmap.md).")
endif()

if(WIN32)
    set(ADL_PLATFORM windows)
elseif(APPLE)
    set(ADL_PLATFORM macos)
elseif(UNIX)
    set(ADL_PLATFORM linux)
else()
    message(FATAL_ERROR
        "Unsupported target platform. Add src/platforms/<name>/ with a "
        "platform.cmake (see the contract at the top of cmake/Platform.cmake) "
        "and a branch for it in this file.")
endif()

set(ADL_PLATFORM_DIR "${PROJECT_SOURCE_DIR}/src/platforms/${ADL_PLATFORM}")
include("${ADL_PLATFORM_DIR}/platform.cmake")

# A fragment that forgets part of the contract should fail here, by name, not
# as an empty source list or a missing-command error three files away.
include(PlatformContract)
adl_platform_check_contract("${ADL_PLATFORM_DIR}/platform.cmake"
    "${PROJECT_SOURCE_DIR}/src/platforms")
