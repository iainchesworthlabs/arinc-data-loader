# The mock target application: a Qt Quick window over the mock engine (core/), which behaves as
# an ARINC 615A target over adl::platform's real UDP sockets, so the data loader application
# (src/app) can be demonstrated against it end to end: FIND, Information, Upload, Download and
# aborts. It is a mock for demonstrations, not a real embedded target.
#
#   core/       the logic without a window: the 615A target engine, the TargetLink seam over the
#               platform's sockets, and MockTargetModel on a worker thread
#   qml/        the window
#   translations/   the window's Qt Linguist catalogues, one per language
#   main.cpp
#
# The window is a static QML module of its own, as the data loader's is, so the window's test
# links and loads exactly the compiled module the application does.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 LinguistTools)

# AUTOMOC and the policies of Qt 6.12 (QML modules get their resource prefix and their qmldir the
# current way), for this directory only.
qt_standard_project_setup(REQUIRES 6.12)

add_subdirectory(core)

adl_prepare_ui_qml()

qt_add_library(adl_mock_target_qml STATIC)
qt_add_qml_module(adl_mock_target_qml
    URI ArincMockTarget
    VERSION 1.0
    QML_FILES
        qml/TargetWindow.qml
        ${ADL_UI_QML_FILES})
target_link_libraries(adl_mock_target_qml
    PUBLIC
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2)

# The catalogues (cmake/AdlUi.cmake explains the pipeline): this window's own strings; the shared
# components' strings are in src/ui/translations.
adl_add_app_translations(adl_mock_target_qml adl_mock_target
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/translations"
    SOURCE_TARGETS adl_mock_core
    SOURCES
        qml/TargetWindow.qml)

qt_add_executable(adl_mock_target main.cpp)

target_compile_features(adl_mock_target PRIVATE cxx_std_23)
target_link_libraries(adl_mock_target
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,adl_mock_target_qml>
        adl::mock_core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_add_ui_support(adl_mock_target)

adl_platform_configure_app(adl_mock_target)
adl_platform_deploy_qt_runtime(adl_mock_target "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_mock_target)
