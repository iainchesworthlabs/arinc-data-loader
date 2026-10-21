# The second Qt application: a window over the libraries with which an operator picks files from
# disk and generates an ARINC 665 media set as the directory tree the data loader (src/app)
# uploads to a target.
#
#   core/       the logic without a window: the generator's worker thread and the models
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

qt_add_library(adl_media_gen_qml STATIC)
qt_add_qml_module(adl_media_gen_qml
    URI ArincMediaGen
    VERSION 1.0
    QML_FILES
        qml/GeneratorWindow.qml
        ${ADL_UI_QML_FILES})
target_link_libraries(adl_media_gen_qml
    PUBLIC
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2)

# The catalogues (cmake/AdlUi.cmake explains the pipeline): this window's own strings; the shared
# components' strings are in src/ui/translations.
adl_add_app_translations(adl_media_gen_qml adl_media_gen
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/translations"
    SOURCE_TARGETS adl_gen_core
    SOURCES
        qml/GeneratorWindow.qml)

qt_add_executable(adl_media_gen main.cpp)

target_compile_features(adl_media_gen PRIVATE cxx_std_23)
target_link_libraries(adl_media_gen
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,adl_media_gen_qml>
        adl::gen_core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_add_ui_support(adl_media_gen)

adl_platform_configure_app(adl_media_gen)
adl_platform_deploy_qt_runtime(adl_media_gen "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_install_application(adl_media_gen "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_media_gen)
