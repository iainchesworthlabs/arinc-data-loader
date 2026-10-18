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
        qml/LogView.qml
        qml/DownloadPanel.qml
        qml/PreferencesDialog.qml
        qml/MediaSetPropertiesDialog.qml
        qml/TargetPropertiesDialog.qml
        qml/AboutDialog.qml)

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

# The QML compiler writes a C++ loader into this target, and in an optimised build GCC reports a
# possible null dereference inside Qt's own QHash in it (a header we cannot change, in a file we do
# not write). It is the one warning turned off, for this target only. It is a target of its own,
# linked after adl::warnings, because the flags of linked targets are given in link order and a
# -Wno- before -Wnull-dereference would turn it back on.
add_library(adl_app_generated_code INTERFACE)
target_compile_options(adl_app_generated_code INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-null-dereference>")
target_link_libraries(adl_app PRIVATE adl_app_generated_code)

adl_platform_configure_app(adl_app)
adl_platform_deploy_qt_runtime(adl_app "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_app)
