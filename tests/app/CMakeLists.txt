# The application's tests (src/app): QtTest, headless. The logic is tested without a window
# against the simulated target on the fake platform (adl_sim_target), and the window's QML is
# loaded once, offscreen, to prove it compiles and binds to the model.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

set(CMAKE_AUTOMOC ON)

# The logic: addresses, models, and whole jobs through LoaderModel on the engine's thread.
add_executable(adl_app_tests
    address_test.cpp
    controls_test.cpp
    download_directory_test.cpp
    engine_worker_test.cpp
    faulty_files.hpp
    faulty_host.hpp
    loader_model_test.cpp
    main.cpp
    media_source_test.cpp
    models_test.cpp
    notice_text_test.cpp
    settings_test.cpp
    test_classes.hpp
    upload_fixture.hpp
    window_state_test.cpp
    "${PROJECT_SOURCE_DIR}/src/ui/window_state.cpp")

target_include_directories(adl_app_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support"
    "${PROJECT_SOURCE_DIR}/src/ui/include")
target_link_libraries(adl_app_tests
    PRIVATE
        adl::app_core
        adl_sim_target
        adl::arinc665
        adl::media_tree
        adl::samples
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_deploy_qt_runtime(adl_app_tests "")
adl_deploy_sanitizer_runtime(adl_app_tests)

# Every program is built into the one bin directory, and the platform's deployment step copies
# Qt's runtime into it: two of those at once would write the same files, so they take turns
# (the application first, then these two).
add_dependencies(adl_app_tests adl_app)

# A test that hangs (a debug runtime's dialog waiting for an answer on a machine with a desktop,
# a job that never ends) fails after two minutes instead of holding the run.
add_test(NAME app.tests COMMAND adl_app_tests)
set_tests_properties(app.tests PROPERTIES
    TIMEOUT 120
    LABELS "unit")

# The window: the same compiled QML module the application links (adl_app_qml), loaded offscreen
# with the real model, through the first-run screen and then a FIND, an Information operation, a
# media set check, an Upload and a media defined Download - with every QML warning an error.
add_executable(adl_app_qml_test
    qml_test.cpp
    "${PROJECT_SOURCE_DIR}/src/app/frame_profiler.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/frame_profiler.hpp"
    "${PROJECT_SOURCE_DIR}/src/app/language_manager.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/language_manager.hpp")
target_include_directories(adl_app_qml_test PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support"
    "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(adl_app_qml_test
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,adl_app_qml>
        adl::app_core
        adl::tracy
        adl_sim_target
        adl::arinc665
        adl::media_tree
        adl::samples
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_add_ui_support(adl_app_qml_test)

adl_platform_deploy_qt_runtime(adl_app_qml_test "${PROJECT_SOURCE_DIR}/src/app/qml")
adl_deploy_sanitizer_runtime(adl_app_qml_test)
add_dependencies(adl_app_qml_test adl_app_tests)

# The window's test can save what the window looks like: one picture per run, in the scheme the
# environment asks for (ADL_DARK=1 for the dark one).
add_test(NAME app.qml COMMAND adl_app_qml_test)
# The offscreen platform plugin is not one the deployment step copies beside the program, so it is
# looked for in the Qt kit's own plugin directory.
set_tests_properties(app.qml PROPERTIES
    TIMEOUT 120
    LABELS "system;ui"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
