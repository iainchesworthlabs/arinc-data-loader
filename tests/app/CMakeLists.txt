# The application's tests (src/app): QtTest, headless. The logic is tested without a window
# against the simulated target on the fake platform (adl_sim_target), and the window's QML is
# loaded once, offscreen, to prove it compiles and binds to the model.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

set(CMAKE_AUTOMOC ON)

# The logic: addresses, models, and whole jobs through LoaderModel on the engine's thread.
add_executable(adl_app_tests
    address_test.cpp
    loader_model_test.cpp
    main.cpp
    media_source_test.cpp
    models_test.cpp
    notice_text_test.cpp
    test_classes.hpp
    upload_fixture.hpp)

target_include_directories(adl_app_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_app_tests
    PRIVATE
        adl::app_core
        adl_sim_target
        adl::arinc665
        adl::media_tree
        adl::samples
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
set_tests_properties(app.tests PROPERTIES TIMEOUT 120)

# The window: Main.qml with the real model, offscreen, a FIND and an Information operation run
# while it shows, and every QML warning an error.
add_executable(adl_app_qml_test qml_test.cpp)
target_include_directories(adl_app_qml_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_app_qml_test
    PRIVATE
        adl::app_core
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

adl_platform_deploy_qt_runtime(adl_app_qml_test "${PROJECT_SOURCE_DIR}/src/app/qml")
adl_deploy_sanitizer_runtime(adl_app_qml_test)
add_dependencies(adl_app_qml_test adl_app_tests)

add_test(NAME app.qml
    COMMAND adl_app_qml_test "${PROJECT_SOURCE_DIR}/src/app/qml/Main.qml")
# The offscreen platform plugin is not one the deployment step copies beside the program, so it is
# looked for in the Qt kit's own plugin directory.
set_tests_properties(app.qml PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
