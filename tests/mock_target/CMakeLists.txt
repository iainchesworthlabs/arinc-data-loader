# The mock target's tests (src/mock_target): the engine headless against the loader's real
# engine (DataLoader and Runner over the fake platform, on one fake network), the model on its
# worker thread, and the window loaded offscreen.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

set(CMAKE_AUTOMOC ON)

# The engine: whole operations of the loader's real engine against the mock target over the
# fake platform, and the injection controls.
add_executable(adl_mock_tests
    engine_test.cpp
    fake_link.hpp
    loader_rig.hpp)
target_include_directories(adl_mock_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_tests
    PRIVATE
        adl::mock_core
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

# The engine test uses only the Qt-free part of the core (MockEngine and TargetLink), so the
# linker pulls in no Qt symbols and there is nothing to deploy.
adl_deploy_sanitizer_runtime(adl_mock_tests)

# Every program is built into the one bin directory, and the platform's deployment step copies
# Qt's runtime into it: two of those at once would write the same files, so they take turns
# (the application first, then these).
add_dependencies(adl_mock_tests adl_mock_target)

add_test(NAME mock.engine COMMAND adl_mock_tests)
set_tests_properties(mock.engine PROPERTIES TIMEOUT 120)

# The model: MockTargetModel on its worker thread, against the loader's engine over the fake
# platform, headless.
add_executable(adl_mock_model_test
    main.cpp
    model_test.cpp
    test_classes.hpp)
target_include_directories(adl_mock_model_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_model_test
    PRIVATE
        adl::mock_core
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::samples
        Qt6::Test
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_deploy_qt_runtime(adl_mock_model_test "")
adl_deploy_sanitizer_runtime(adl_mock_model_test)

# Every program is built into the one bin directory, and the platform's deployment step copies
# Qt's runtime into it: two of those at once would write the same files, so they take turns
# (the application first, then these).
add_dependencies(adl_mock_model_test adl_mock_target)

# A test that hangs fails after two minutes instead of holding the run.
add_test(NAME mock.model COMMAND adl_mock_model_test)
set_tests_properties(mock.model PROPERTIES TIMEOUT 120)

# The window: TargetWindow.qml with the real model, offscreen, started and stopped through the
# window's own buttons, and every QML warning an error.
add_executable(adl_mock_qml_test qml_test.cpp)
target_include_directories(adl_mock_qml_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_mock_qml_test
    PRIVATE
        adl::mock_core
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::samples
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_deploy_qt_runtime(adl_mock_qml_test "${PROJECT_SOURCE_DIR}/src/mock_target/qml")
adl_deploy_sanitizer_runtime(adl_mock_qml_test)
add_dependencies(adl_mock_qml_test adl_mock_model_test)

add_test(NAME mock.qml
    COMMAND adl_mock_qml_test "${PROJECT_SOURCE_DIR}/src/mock_target/qml/TargetWindow.qml")
# The offscreen platform plugin is not one the deployment step copies beside the program, so it is
# looked for in the Qt kit's own plugin directory.
set_tests_properties(mock.qml PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
