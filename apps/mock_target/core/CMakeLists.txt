# SPDX-License-Identifier: GPL-3.0-or-later
# The mock target's Qt side without the window: MockTargetModel, which runs the engine of
# libs/mock_target_core on a worker thread for the window, and the list models the window shows.
# Qt Core only (no GUI, no QML), so the model is tested headless.

set(CMAKE_AUTOMOC ON)

add_library(adl_mock_core STATIC
    include/adl/mock/model.hpp
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
        adl::mock_target_core
        adl::arinc615a
        adl::arinc615a_mini
        adl::arinc665
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
