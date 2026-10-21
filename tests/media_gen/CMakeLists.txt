# The generator's tests (src/media_gen): QtTest, headless. The logic is tested without a window
# against media sets written to a temporary directory and read back with the libraries (and with
# the data loader's own reader, which proves the sets load), and the window's QML is loaded once,
# offscreen, to prove it compiles, binds to the model and generates a media set.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)

set(CMAKE_AUTOMOC ON)

# The logic: the form's validation, whole generations through GeneratorModel on the generator's
# thread (a verified single-member set, multi-member placement, a batch, a cancel), and what
# happens when the form is not valid.
add_executable(adl_gen_tests
    generator_test.cpp
    main.cpp
    test_classes.hpp)

target_include_directories(adl_gen_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_gen_tests
    PRIVATE
        adl::gen_core
        adl::app_core
        adl::arinc665
        adl::media_tree
        adl::samples
        Qt6::Test
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The compiled English catalogue (plural forms) is read from where the application's build puts it.
target_compile_definitions(adl_gen_tests PRIVATE
    ADL_I18N_DIR="${CMAKE_BINARY_DIR}/src/media_gen")
add_dependencies(adl_gen_tests adl_media_gen_lrelease)

adl_platform_deploy_qt_runtime(adl_gen_tests "")
adl_deploy_sanitizer_runtime(adl_gen_tests)

# Every program is built into the one bin directory, and the platform's deployment step copies
# Qt's runtime into it: two of those at once would write the same files, so they take turns
# (the application first, then these two).
add_dependencies(adl_gen_tests adl_media_gen)

# A test that hangs (a job that never ends) fails after two minutes instead of holding the run.
add_test(NAME media_gen.tests COMMAND adl_gen_tests)
set_tests_properties(media_gen.tests PROPERTIES
    TIMEOUT 120
    LABELS "unit")

# The window: GeneratorWindow.qml with the real model, offscreen, a generation started from the
# window's own Generate button, and every QML warning an error.
add_executable(adl_gen_qml_test qml_test.cpp)
target_include_directories(adl_gen_qml_test PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_gen_qml_test
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,adl_media_gen_qml>
        adl::gen_core
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

adl_add_ui_support(adl_gen_qml_test)

adl_platform_deploy_qt_runtime(adl_gen_qml_test "${PROJECT_SOURCE_DIR}/src/media_gen/qml")
adl_deploy_sanitizer_runtime(adl_gen_qml_test)
add_dependencies(adl_gen_qml_test adl_gen_tests)

add_test(NAME media_gen.qml COMMAND adl_gen_qml_test)
# The offscreen platform plugin is not one the deployment step copies beside the program, so it is
# looked for in the Qt kit's own plugin directory.
set_tests_properties(media_gen.qml PROPERTIES
    TIMEOUT 120
    LABELS "system;ui"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
