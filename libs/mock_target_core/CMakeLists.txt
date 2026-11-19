# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's logic without a window and without Qt: the engine that behaves as an ARINC 615A
# target (the protocol files served and written through adl::arinc615a's codecs, the TFTP of
# adl::arinc615a_mini) and the TargetLink seam that runs it over adl::platform's real sockets. The
# mock target application (apps/mock_target) puts a window and a worker thread on it; the tests run
# it over the loader's own engine on the fake platform and over loopback sockets.
#
# Not part of the adl package and not installed: it is the implementation of the mock target, not
# an interface for others to build on.

add_library(adl_mock_target_core STATIC
    include/adl/mock/engine.hpp
    include/adl/mock/link.hpp
    src/engine.cpp
    src/link.cpp)
add_library(adl::mock_target_core ALIAS adl_mock_target_core)

target_include_directories(adl_mock_target_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_mock_target_core PUBLIC cxx_std_23)
target_link_libraries(adl_mock_target_core
    PUBLIC
        adl::arinc615a
        adl::arinc615a_mini
        adl::arinc665
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
