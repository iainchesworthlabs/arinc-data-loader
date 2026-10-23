# The generator's logic, without the window: the worker that builds the media set on its own
# thread, the list of files the operator chose, and GeneratorModel, the one object the window
# talks to. Qt Core only (no GUI, no QML), so it is tested headless. Everything that depends on
# the 665 and media_tree libraries sits here and not in the QML.

set(CMAKE_AUTOMOC ON)

add_library(adl_gen_core STATIC
    include/adl/gen/generator_model.hpp
    include/adl/gen/generator_worker.hpp
    include/adl/gen/source_file_model.hpp
    src/generator_model.cpp
    src/generator_worker.cpp
    src/source_file_model.cpp)
add_library(adl::gen_core ALIAS adl_gen_core)

target_include_directories(adl_gen_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_gen_core PUBLIC cxx_std_23)
# Qt's keyword macros (emit, signals, slots, foreach, forever) would clash with the platform
# layer's own `forever`; the generator uses Q_EMIT, Q_SIGNALS and Q_SLOTS instead.
target_compile_definitions(adl_gen_core PUBLIC QT_NO_KEYWORDS)
target_link_libraries(adl_gen_core
    PUBLIC
        Qt6::Core
        adl::ui_message
        adl::arinc665
    PRIVATE
        adl::media_tree
        adl::warnings
        adl::sanitizers
        adl::coverage)
