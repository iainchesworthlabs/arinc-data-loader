# SPDX-License-Identifier: GPL-3.0-or-later
# A second, independent TFTP implementation (lock-step blocks, a two second time-out and one
# retry, the options the loader uses, WAIT and ABORT), deliberately plain, sharing no code with
# the engine, so that the engine is checked against a second reading of RFC 1350 and ARINC
# 615A-3 section 5.3.2 and not against itself. It speaks in the engine's Endpoint but uses only
# the codec for the packets.
#
# Written for the tests (tests/engine's simulated target), and now also the TFTP of the mock
# target application (apps/mock_target), whose engine runs it over real sockets through
# adl::platform.

add_library(adl_arinc615a_mini STATIC
    include/adl/arinc615a_mini/mini_tftp.hpp
    src/mini_tftp.cpp)
add_library(adl::arinc615a_mini ALIAS adl_arinc615a_mini)

target_include_directories(adl_arinc615a_mini PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_arinc615a_mini PUBLIC cxx_std_23)
target_link_libraries(adl_arinc615a_mini
    PUBLIC
        adl::arinc615a
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
