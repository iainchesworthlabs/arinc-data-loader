# SPDX-License-Identifier: GPL-3.0-or-later
# What the loader does that is not showing a window: the files of a media set on disk and the index an
# upload reads them through, the check that turns a media set into a report, the folder a download is
# written into, and the target's address. No Qt: the loader's application (apps/app/core) calls it, and
# so can a program with no window. It builds on the ARINC 665 and 615A libraries, the media tree and
# the platform layer, and it is exported with them in the development package. It also holds the record of
# an operation, the recorder that fills it from the engine's callbacks, and the writer of the file it is
# kept in.

add_library(adl_loader_core STATIC
    src/address.cpp
    src/download_directory.cpp
    src/media_set_check.cpp
    src/media_source.cpp
    src/record_json.cpp
    src/recorder.cpp)
add_library(adl::loader_core ALIAS adl_loader_core)

target_include_directories(adl_loader_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(adl_loader_core PUBLIC cxx_std_23)
target_link_libraries(adl_loader_core
    PUBLIC
        adl::arinc665
        adl::arinc615a
        adl::media_tree
        adl::platform
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::tracy>
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

# The libFuzzer harness for what this library parses, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# Only the top-level project installs (cmake/AdlInstall.cmake); a project of its own that builds this
# library for a target has no install rules to add it to.
if(COMMAND adl_install_library)
    adl_install_library(adl_loader_core INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
