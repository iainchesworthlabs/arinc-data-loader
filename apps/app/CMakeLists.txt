# SPDX-License-Identifier: GPL-3.0-or-later
# The Qt application: a thin layer over the libraries. Qt is the prebuilt kit (QT_ROOT_DIR, or the
# aqtinstall layout each platform fragment lists), found by cmake/FindQt6.cmake - not vcpkg.
#
#   core/   the logic without a window: the engine's worker thread, the models, LoaderModel
#   qml/    the window, its design tokens (Theme.qml) and the components they draw
#   main.cpp   the process: the window, and the language it is in (adl::ui::LanguageManager,
#              shared with the other two applications)
#
# Qt code lives here and nowhere else.

# The applications build by default on the desktops (ADL_BUILD_APP defaults to
# ADL_PLATFORM_DESKTOP), so a machine without Qt needs to be told: say what to do rather than let
# Qt's own "could not find" error stand alone.
find_package(Qt6 6.12 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 LinguistTools)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "The Qt applications are on by default on this desktop, and Qt 6.12 was not found.\n"
        "  - install the kit and point QT_ROOT_DIR at it (see docs/building.md), or\n"
        "  - configure with -DADL_BUILD_APP=OFF to build the libraries alone.")
endif()

# AUTOMOC and the policies of Qt 6.12 (QML modules get their resource prefix and their qmldir the
# current way), for this directory only.
qt_standard_project_setup(REQUIRES 6.12)

add_subdirectory(core)

set(ADL_APP_QML_FILES
    qml/Main.qml
    qml/UploadPanel.qml
    qml/DownloadPanel.qml
    qml/ActivityDock.qml
    qml/FirstRunScreen.qml
    qml/PreferencesDialog.qml
    qml/MediaSetPropertiesDialog.qml
    qml/TargetPropertiesDialog.qml
    ${ADL_UI_QML_FILES})

adl_prepare_ui_qml()

# The window is a static library of its own for one reason:
# a QML module is registered by static initialisers that nothing calls, so the application links
# this with the whole archive - and so does the window's test, which therefore loads the same
# compiled module the application does rather than a second copy of the files.
qt_add_library(adl_app_qml STATIC)
qt_add_qml_module(adl_app_qml
    URI ArincDataLoader
    VERSION 1.0
    QML_FILES ${ADL_APP_QML_FILES})
target_link_libraries(adl_app_qml
    PUBLIC
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2)

qt_add_executable(adl_app
    main.cpp
    frame_profiler.cpp
    frame_profiler.hpp)

# The catalogues (cmake/AdlUi.cmake explains the pipeline): the application's own strings, from its
# QML and the C++ below; the shared components' strings are in libs/ui/translations. Adding a
# language is a .ts file in translations/ and in libs/ui/translations, the language in
# ADL_TRANSLATED_LANGUAGES and its line in libs/ui/language_manager.cpp's table.
adl_add_app_translations(adl_app_qml adl_app
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/translations"
    SOURCE_TARGETS adl_app_core
    SOURCES
        qml/Main.qml
        qml/UploadPanel.qml
        qml/DownloadPanel.qml
        qml/ActivityDock.qml
        qml/FirstRunScreen.qml
        qml/PreferencesDialog.qml
        qml/MediaSetPropertiesDialog.qml
        qml/TargetPropertiesDialog.qml)

target_compile_features(adl_app PRIVATE cxx_std_23)
target_link_libraries(adl_app
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,adl_app_qml>
        adl::app_core
        adl::tracy
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_add_ui_support(adl_app)

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
adl_install_application(adl_app "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_app)
