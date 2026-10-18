# The Qt application: a thin layer over the libraries. Qt is the prebuilt kit (QT_ROOT_DIR, or the
# aqtinstall layout each platform fragment lists), found by cmake/FindQt6.cmake - not vcpkg.
#
#   core/   the logic without a window: the engine's worker thread, the models, LoaderModel
#   qml/    the window
#   main.cpp
#
# Qt code lives here and nowhere else.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# AUTOMOC and the policies of Qt 6.12 (QML modules get their resource prefix and their qmldir the
# current way), for this directory only.
qt_standard_project_setup(REQUIRES 6.12)

add_subdirectory(core)

qt_add_executable(adl_app main.cpp)
qt_add_qml_module(adl_app
    URI ArincDataLoader
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Panel.qml
        qml/LogView.qml)

target_compile_features(adl_app PRIVATE cxx_std_23)
target_link_libraries(adl_app
    PRIVATE
        adl::app_core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_configure_app(adl_app)
adl_platform_deploy_qt_runtime(adl_app "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_app)
