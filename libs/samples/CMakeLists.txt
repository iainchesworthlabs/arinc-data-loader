# SPDX-License-Identifier: GPL-3.0-or-later
# Sample loads, batches and media sets: the scenario the golden tests compare
# with a second implementation, and the ARINC 615 and Boeing 777 layouts of
# Appendix F. The tests build their cases from them and so does the program that
# writes them to a directory (apps/write_payloads). Pure data, no I/O.

add_library(adl_samples STATIC
    src/scenarios.cpp)
add_library(adl::samples ALIAS adl_samples)

target_include_directories(adl_samples PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_samples PUBLIC cxx_std_23)
target_link_libraries(adl_samples
    PUBLIC
        adl::arinc665
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
