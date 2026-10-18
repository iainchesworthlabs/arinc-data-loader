# The application's logic, without the window: the engine on its own thread (a worker that runs the
# 615A runner's jobs and reports through signals), the models the window shows, and LoaderModel,
# the one object the window talks to. Qt Core only (no GUI, no QML), so it is tested headless.
# Everything that depends on the 615A, 665 and platform libraries sits here and not in the QML.

set(CMAKE_AUTOMOC ON)

add_library(adl_app_core STATIC
    include/adl/app/address.hpp
    include/adl/app/engine_worker.hpp
    include/adl/app/events.hpp
    include/adl/app/host_access.hpp
    include/adl/app/loader_model.hpp
    include/adl/app/media_set_summary.hpp
    include/adl/app/media_source.hpp
    include/adl/app/models.hpp
    include/adl/app/notice_text.hpp
    include/adl/app/text.hpp
    src/address.cpp
    src/engine_worker.cpp
    src/events.cpp
    src/host_access.cpp
    src/loader_model.cpp
    src/media_set_summary.cpp
    src/media_source.cpp
    src/models.cpp
    src/notice_text.cpp)
add_library(adl::app_core ALIAS adl_app_core)

target_include_directories(adl_app_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_app_core PUBLIC cxx_std_23)
# Qt's keyword macros (emit, signals, slots, foreach, forever) would clash with the platform
# layer's own `forever`; the application uses Q_EMIT, Q_SIGNALS and Q_SLOTS instead.
target_compile_definitions(adl_app_core PUBLIC QT_NO_KEYWORDS)
target_link_libraries(adl_app_core
    PUBLIC
        Qt6::Core
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::platform
    PRIVATE
        adl::media_tree
        adl::warnings
        adl::sanitizers
        adl::coverage)
