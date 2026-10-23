# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's logic, without the window: a 615A target engine (the protocol files served
# and written through adl::arinc615a's codecs, the TFTP of adl::arinc615a_mini), a TargetLink
# seam that runs it over adl::platform's real sockets, and MockTargetModel, which runs the engine
# on a worker thread for the window. Qt Core only (no GUI, no QML), so the engine is tested
# headless against the loader's own engine over the fake platform.

set(CMAKE_AUTOMOC ON)

add_library(adl_mock_core STATIC
    include/adl/mock/engine.hpp
    include/adl/mock/link.hpp
    include/adl/mock/model.hpp
    src/engine.cpp
    src/link.cpp
    src/model.cpp)
add_library(adl::mock_core ALIAS adl_mock_core)

target_include_directories(adl_mock_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_mock_core PUBLIC cxx_std_23)
# Qt's keyword macros (emit, signals, slots, foreach, forever) would clash with the platform
# layer's own `forever`; the mock target uses Q_EMIT, Q_SIGNALS and Q_SLOTS instead.
target_compile_definitions(adl_mock_core PUBLIC QT_NO_KEYWORDS)
target_link_libraries(adl_mock_core
    PUBLIC
        Qt6::Core
        adl::ui_message
        adl::arinc615a
        adl::arinc615a_mini
        adl::arinc665
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
