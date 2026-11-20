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
if(NOT DEFINED ADL_BUILD_TESTS OR ADL_BUILD_TESTS OR ADL_BUILD_BENCHMARKS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

# Tracy is opt-in: the one switch (declared in cmake/Tracy.cmake) both makes
# the profiler client resolvable and turns the instrumentation on.
if(ADL_ENABLE_TRACY)
    list(APPEND VCPKG_MANIFEST_FEATURES "profiling")
endif()

project(ArincDataLoader
    VERSION 0.1.2
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
include(Perf)
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
if(ADL_BUILD_TESTS OR ADL_ENABLE_FUZZING OR ADL_BUILD_BENCHMARKS)
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

# What the loader does without a window: the media set's files and their index, the media set check,
# the download folder and the target's address. No Qt; apps/app/core calls it.
add_subdirectory(libs/loader_core)

# The licence library: the signed licence file, its states and the checks a program asks. Platform-free.
# Its Ed25519 is Monocypher, vendored in third_party/monocypher, which is C: the language is enabled here,
# and only on a desktop, because a target build has no use for a licence and may have no C compiler.
if(ADL_PLATFORM_DESKTOP)
    enable_language(C)
    add_subdirectory(third_party/monocypher)
    add_subdirectory(libs/licence)
    # The same library over the platform layer: the store's file interface and the folders. GPL only.
    add_subdirectory(libs/licence_platform)
    # The commands of adl_licence (show, import, remove, verify), over the store and the platform layer. GPL only.
    add_subdirectory(libs/licence_cli)
    # The owner's licence tool: built with the other tools, never installed (tools/licence/CMakeLists.txt).
    add_subdirectory(tools/licence)
    # What the scenario files of the mock target and of the loader have in common: the random numbers and the
    # shared reading of the file. It reads JSON with the licence library, so it is a desktop library too.
    add_subdirectory(libs/scenario_base)
endif()

# The mock target's logic without a window and without Qt: its 615A engine and the link that runs
# it over adl::platform's sockets. The mock target application builds on it below, and so do the
# tests. Not installed and not part of the adl package.
add_subdirectory(libs/mock_target_core)

# Sample loads and media sets, for the tests and for the program below.
add_subdirectory(libs/samples)

# adl_write_payloads: writes the sample media sets to a directory, for trying
# other loaders on them.
add_subdirectory(apps/write_payloads)

# adl_mock: the mock ARINC 615A target as a command, with no window and no Qt, for test rigs and CI
# jobs.
add_subdirectory(apps/mock)

# adl_licence: shows, imports, removes and checks the licence on this computer, from a command line.
if(ADL_PLATFORM_DESKTOP)
    add_subdirectory(apps/licence)
endif()

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
