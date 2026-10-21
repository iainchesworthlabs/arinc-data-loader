# The window's UI suite, with Qt Quick Test: every tst_*.qml file under qml/ is one TestCase, run
# offscreen against the application's real Main.qml and LoaderModel. Clicks, typing, menu-free
# buttons and the dialogs' fields are Qt Quick Test events on the real items, so the QML handlers,
# bindings and the application core they call are what is exercised; the model runs the engine on
# its thread against the simulated target on the fake platform (adl_sim_target).

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test QuickTest)

set(CMAKE_AUTOMOC ON)

# The window QML is compiled into the application module, but this suite loads Main.qml as a
# component so it can create the window itself and inject the harness model. A direct file load
# cannot see the module's shared components, so the build copies the shared and window QML into
# one directory with a qmldir that declares the Theme singleton. The copy also gives the suite its
# own import directory, separate from the source tree a developer may be editing.
set(ADL_UI_TEST_QML_DIR "${CMAKE_CURRENT_BINARY_DIR}/window")
file(MAKE_DIRECTORY "${ADL_UI_TEST_QML_DIR}")
file(GLOB ADL_UI_TEST_SHARED_QML CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/ui/qml/*.qml")
set(ADL_UI_TEST_WINDOW_QML
    "${PROJECT_SOURCE_DIR}/src/app/qml/Main.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/UploadPanel.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/DownloadPanel.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/ActivityDock.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/FirstRunScreen.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/PreferencesDialog.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/MediaSetPropertiesDialog.qml"
    "${PROJECT_SOURCE_DIR}/src/app/qml/TargetPropertiesDialog.qml"
)
file(WRITE "${ADL_UI_TEST_QML_DIR}/qmldir" "singleton Theme 1.0 Theme.qml\n")
add_custom_target(adl_ui_test_qml ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ADL_UI_TEST_QML_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${ADL_UI_TEST_SHARED_QML} ${ADL_UI_TEST_WINDOW_QML} "${ADL_UI_TEST_QML_DIR}"
    COMMENT "Copying the window QML for the UI suite")

add_executable(adl_ui_tests
    main.cpp
    ui_harness.cpp
    ui_harness.hpp
    ui_setup.hpp)

target_include_directories(adl_ui_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support"
    "${PROJECT_SOURCE_DIR}/tests/app")

target_compile_definitions(adl_ui_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    ADL_MAIN_QML="${ADL_UI_TEST_QML_DIR}/Main.qml")

adl_add_ui_support(adl_ui_tests)
add_dependencies(adl_ui_tests adl_ui_test_qml)

target_link_libraries(adl_ui_tests
    PRIVATE
        adl::app_core
        adl_sim_target
        adl::arinc665
        adl::media_tree
        adl::samples
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickTest
        Qt6::Test
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_deploy_qt_runtime(adl_ui_tests "${PROJECT_SOURCE_DIR}/src/app/qml")
adl_deploy_sanitizer_runtime(adl_ui_tests)

# Every program is built into the one bin directory, and the platform's deployment step copies
# Qt's runtime into it: two of those at once would write the same files, so they take turns.
add_dependencies(adl_ui_tests adl_app_tests)

# One test per QML file, so each starts with a model of its own (a media set read by one file
# cannot colour what the next one sees). A test that hangs fails after five minutes instead of
# holding the run. The offscreen platform plugin is not one the deployment step copies beside the
# program, so it is looked for in the Qt kit's own plugin directory.
foreach(test_file tst_main_window tst_dialogs tst_first_run tst_window_close tst_keyboard tst_navigation tst_accessibility tst_layout tst_tones)
    add_test(NAME ui.${test_file}
        COMMAND adl_ui_tests -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/${test_file}.qml")
    set_tests_properties(ui.${test_file} PROPERTIES
        TIMEOUT 300
        LABELS "ui;system"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
endforeach()

# The window geometry code the three programs share (src/ui/window_state.cpp), on a bare window:
# restore, clamp, off-screen and maximised. QtTest, offscreen, with the settings in a scratch
# directory.
add_executable(adl_window_state_tests
    window_state_test.cpp)
target_include_directories(adl_window_state_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/support")
adl_add_ui_support(adl_window_state_tests)
target_link_libraries(adl_window_state_tests
    PRIVATE
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        adl::warnings
        adl::sanitizers
        adl::coverage)
adl_platform_deploy_qt_runtime(adl_window_state_tests "")
adl_deploy_sanitizer_runtime(adl_window_state_tests)
add_dependencies(adl_window_state_tests adl_ui_tests)
add_test(NAME ui.window_state COMMAND adl_window_state_tests)
set_tests_properties(ui.window_state PROPERTIES
    TIMEOUT 120
    LABELS "ui;unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
