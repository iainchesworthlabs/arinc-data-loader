# SPDX-License-Identifier: GPL-3.0-or-later
# The ARINC 665 library: the media set, load and integrity-check logic. It is
# GUI-free and platform-free: nothing here includes an operating system header,
# and what differs between operating systems arrives through
# libs/platform/interfaces.
#
# It is written with the embedded target on the roadmap in mind: failures are
# std::expected rather than exceptions, the CRCs are computed from compile-time
# tables, and nothing is created by a static initialiser. Not everything avoids
# the heap yet (PartNumber holds a std::string), so that is a direction, not a
# guarantee.

add_library(adl_arinc665 STATIC
    src/batch_file.cpp
    src/header_file.cpp
    src/integrity_checker.cpp
    src/load.cpp
    src/md5.cpp
    src/media_files.cpp
    src/media_label.cpp
    src/media_set.cpp
    src/naming.cpp
    src/part_number.cpp
    src/portable_name.cpp
    src/sha1.cpp
    src/wire.cpp)
add_library(adl::arinc665 ALIAS adl_arinc665)

target_include_directories(adl_arinc665
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(adl_arinc665 PUBLIC cxx_std_23)
target_link_libraries(adl_arinc665
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::tracy>
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

# The libFuzzer harnesses for what this library parses, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# The micro-benchmarks of this library, when they are asked for (cmake/Perf.cmake).
if(ADL_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# Only the top-level project installs (cmake/AdlInstall.cmake); a project of its own that builds this
# library for a target (tests/platform/freertos) has no install rules to add it to.
if(COMMAND adl_install_library)
    adl_install_library(adl_arinc665 INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
