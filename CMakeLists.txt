# CMake 4 is required; the version range keeps policy behaviour current up to 4.4.
cmake_minimum_required(VERSION 3.28...4.4)

# ---------------------------------------------------------------------------
# Vendored vcpkg (git submodule at deps/vcpkg).
#
# The dependency provider is pinned by the submodule so builds are fully
# reproducible without a machine-wide vcpkg or a VCPKG_ROOT env var. The
# CMakePresets point CMAKE_TOOLCHAIN_FILE at deps/vcpkg/scripts/buildsystems.
# If the submodule was not checked out, initialise it here so a fresh clone
# configures without extra steps.
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/vcpkg/scripts/buildsystems/vcpkg.cmake")
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        message(STATUS "Initialising vendored vcpkg submodule (deps/vcpkg)")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --depth 1 -- deps/vcpkg
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE ADL_SUBMODULE_RESULT)
        if(NOT ADL_SUBMODULE_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Failed to initialise the vcpkg submodule. Run:\n"
                "  git submodule update --init --depth 1 deps/vcpkg")
        endif()
    endif()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/vcpkg/scripts/buildsystems/vcpkg.cmake")
    message(FATAL_ERROR
        "Vendored vcpkg not found at deps/vcpkg.\n"
        "Clone with submodules (git clone --recurse-submodules ...) or run:\n"
        "  git submodule update --init --depth 1 deps/vcpkg")
endif()

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
    DESCRIPTION "An ARINC 615A and 665 compliant data loader"
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
include(CompilerWarnings)
include(Sanitizers)
include(Coverage)
include(Tracy)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ADL_BUILD_TESTS "Build the unit test suite" ON)
# The Qt application (src/app). Off by default so that the libraries build on a machine without Qt;
# CI turns it on. Qt is the prebuilt kit found through QT_ROOT_DIR (cmake/FindQt6.cmake).
option(ADL_BUILD_APP "Build the Qt application" OFF)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
# Everything that differs between operating systems, behind the interfaces in
# src/platforms/interfaces. Compiled from the sources the selected platform's
# platform.cmake names.
add_subdirectory(src/platforms)

# The ARINC 665 library holds the media set, load and integrity-check logic. It
# is GUI-free and platform-free.
add_subdirectory(src/arinc665)

# The ARINC 615A library: the TFTP packets and protocol files, as bytes in and out.
# Platform-free, and it builds no ARINC 665 model.
add_subdirectory(src/arinc615a)

# A second, independent TFTP implementation (shared by the simulated target of the tests and
# the mock target application), so the engine is checked against a second reading of the
# standard and not against itself. Platform-free: it uses only the codec.
add_subdirectory(src/arinc615a_mini)

# The 615A runner: opens the sockets the engine asks for and drives its state machines over
# adl::platform. The only 615A code that calls the platform.
add_subdirectory(src/arinc615a_runner)

# A media set on a disk: a directory tree with one directory per member. Plain
# std::filesystem, so it needs nothing from the platform section either.
add_subdirectory(src/media_tree)

# Sample loads and media sets, for the tests and for the program below.
add_subdirectory(src/samples)

# adl_write_payloads: writes the sample media sets to a directory, for trying
# other loaders on them.
add_subdirectory(src/write_payloads)

# The Qt application: a thin layer over the libraries above, and the only place Qt is used.
if(ADL_BUILD_APP)
    add_subdirectory(src/app)
    # The mock target: a Qt application that behaves as an ARINC 615A target over real
    # sockets, for demonstrating the loader's dialogues end to end.
    add_subdirectory(src/mock_target)
endif()

if(ADL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "ARINC Data Loader ${PROJECT_VERSION} for ${ADL_PLATFORM_DISPLAY_NAME}")
message(STATUS "  Build tests : ${ADL_BUILD_TESTS}")
message(STATUS "  Build app   : ${ADL_BUILD_APP}")
message(STATUS "  Tracy       : ${ADL_ENABLE_TRACY}")
