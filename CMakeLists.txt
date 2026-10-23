# SPDX-License-Identifier: GPL-3.0-or-later
# CMake 4 is required; the version range keeps policy behaviour current up to 4.4.
cmake_minimum_required(VERSION 3.28...4.4)

# ---------------------------------------------------------------------------
# vcpkg, in manifest mode, pinned by "builtin-baseline" in vcpkg.json.
#
# Which checkout of the labs' vcpkg fork a configure uses - the one named by
# VCPKG_ROOT, or one shared between your checkouts through ICL_VCPKG_STORE - is
# decided in cmake/icl/IclVcpkgRoot.cmake (vendored from icl-common-infra; see
# cmake/icl/SOURCE), which sets CMAKE_TOOLCHAIN_FILE. The presets deliberately
# do not. The project prefix is ADL, so ADL_VCPKG_ROOT and ADL_VCPKG_STORE
# override the shared variables for this project.
# ---------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl/IclVcpkgRoot.cmake")
icl_resolve_vcpkg("${CMAKE_CURRENT_SOURCE_DIR}" ADL)

# ---------------------------------------------------------------------------
# Select vcpkg manifest features BEFORE project() (the vcpkg toolchain reads
# VCPKG_MANIFEST_FEATURES when it is loaded at project() time). Qt is a
# prebuilt dependency found via CMAKE_PREFIX_PATH (see cmake/FindQt6.cmake),
# not built by vcpkg.
# ---------------------------------------------------------------------------
if(NOT DEFINED ADL_BUILD_TESTS OR ADL_BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

# Tracy is opt-in: the one switch (declared in cmake/Tracy.cmake) both makes
# the profiler client resolvable and turns the instrumentation on.
if(ADL_ENABLE_TRACY)
    list(APPEND VCPKG_MANIFEST_FEATURES "profiling")
endif()

project(ArincDataLoader
    VERSION 0.1.0
    DESCRIPTION "An ARINC 615A and 665 data loader, written from the standards"
    HOMEPAGE_URL "https://github.com/iainchesworthlabs/arinc-data-loader"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global language settings: C++23 only, no compiler extensions.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Put all runtime artefacts in one place.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# The one place the target OS is decided (cmake/Platform.cmake); every other
# CMake file reads what the platform's fragment declared.
include(Platform)
include(AdlInstall)
include(CompilerWarnings)
include(StaticAnalysis)
include(Sanitizers)
include(Fuzzing)
# Vendored from icl-common-infra (cmake/icl/SOURCE): ICL_SANITIZERS picks a sanitizer set for every
# configuration (the linux-asan-ubsan, linux-tsan and windows-msvc-asan presets), and icl::hardening
# adds the stack protector, control-flow protection, RELRO and the like to every target below.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl/IclSanitizers.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl/IclHardening.cmake")
# BUILD_INTERFACE keeps the dependency out of the installed adl package's exported targets.
link_libraries("$<BUILD_INTERFACE:icl::hardening>")
include(Coverage)
include(Tracy)
include(AdlUi)
include(Docs)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ADL_BUILD_TESTS "Build the unit, integration and system test suites" ON)
# The three Qt applications (apps/app, apps/media_gen, apps/mock_target) are the source side's
# programs, so they build by default on a desktop - the only place Qt exists. A target build
# (ADL_TARGET_PLATFORM names an RTOS, or an embedded toolchain sets it) has no Qt and no use for
# them and leaves them off. The default also needs a kit that can actually be found: a desktop
# without Qt configures the libraries alone rather than failing, and an explicit
# -DADL_BUILD_APP=ON still fails loudly in apps/app. Qt is the prebuilt kit found through
# QT_ROOT_DIR or the search roots each platform's fragment lists (cmake/FindQt6.cmake).
if(ADL_PLATFORM_DESKTOP)
    find_package(Qt6 6.12 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 LinguistTools)
endif()
if(ADL_PLATFORM_DESKTOP AND Qt6_FOUND)
    set(_adl_app_default ON)
else()
    set(_adl_app_default OFF)
endif()
option(ADL_BUILD_APP "Build the Qt applications (default: on a desktop with Qt, off otherwise)"
    ${_adl_app_default})
unset(_adl_app_default)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
# Everything that differs between operating systems, behind the interfaces in
# libs/platform/interfaces. Compiled from the sources the selected platform's
# platform.cmake names.
add_subdirectory(libs/platform)

# Tests are registered from the source directories too (the translation checks sit beside the
# catalogues they check), so testing has to be on before any of them is added.
if(ADL_BUILD_TESTS OR ADL_ENABLE_FUZZING)
    enable_testing()
endif()

# The ARINC 665 library holds the media set, load and integrity-check logic. It
# is GUI-free and platform-free.
add_subdirectory(libs/arinc665)

# The ARINC 615A library: the TFTP packets and protocol files, as bytes in and out.
# Platform-free, and it builds no ARINC 665 model.
add_subdirectory(libs/arinc615a)

# A second, independent TFTP implementation (shared by the simulated target of the tests and
# the mock target application), so the engine is checked against a second reading of the
# standard and not against itself. Platform-free: it uses only the codec.
add_subdirectory(libs/arinc615a_mini)

# The 615A runner: opens the sockets the engine asks for and drives its state machines over
# adl::platform. The only 615A code that calls the platform.
add_subdirectory(libs/arinc615a_runner)

# A media set on a disk: a directory tree with one directory per member. Plain
# std::filesystem, so it needs nothing from the platform section either.
add_subdirectory(libs/media_tree)

# Sample loads and media sets, for the tests and for the program below.
add_subdirectory(libs/samples)

# adl_write_payloads: writes the sample media sets to a directory, for trying
# other loaders on them.
add_subdirectory(apps/write_payloads)

# The Qt application: a thin layer over the libraries above, and the only place Qt is used.
if(ADL_BUILD_APP)
    add_subdirectory(libs/ui)
    add_subdirectory(apps/app)
    # The mock target: a Qt application that behaves as an ARINC 615A target over real
    # sockets, for demonstrating the loader's dialogues end to end.
    add_subdirectory(apps/mock_target)
    # The media set generator: a second Qt application that turns files chosen by the
    # operator into a valid ARINC 665 media set the loader can open.
    add_subdirectory(apps/media_gen)
endif()

if(ADL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# What `cmake --install` installs and CPack packs (cmake/AdlInstall.cmake, cmake/AdlPackage.cmake):
# the programs and the libraries other programs can build on, on the desktops. A target build
# (an RTOS) ships nothing from here.
if(ADL_PLATFORM_DESKTOP)
    adl_install_licences()
    adl_install_package_config()
    include(AdlPackage)
endif()

message(STATUS "ARINC Data Loader ${PROJECT_VERSION} for ${ADL_PLATFORM_DISPLAY_NAME}")
message(STATUS "  Build tests : ${ADL_BUILD_TESTS}")
message(STATUS "  Build app   : ${ADL_BUILD_APP}")
if(ADL_BUILD_APP)
    message(STATUS "  Applications: adl_app, adl_media_gen, adl_mock_target")
elseif(ADL_PLATFORM_DESKTOP)
    message(STATUS "  Applications: off on this desktop (-DADL_BUILD_APP=ON builds adl_app, adl_media_gen, adl_mock_target)")
else()
    message(STATUS "  Applications: off for a target build (the source side's programs need a desktop and Qt)")
endif()
message(STATUS "  Tracy       : ${ADL_ENABLE_TRACY}")
