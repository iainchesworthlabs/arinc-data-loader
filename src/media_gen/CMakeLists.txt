# The second Qt application: a window over the libraries with which an operator picks files from
# disk and generates an ARINC 665 media set as the directory tree the data loader (src/app)
# uploads to a target.
#
#   core/   the logic without a window: the generator's worker thread and the models
#   qml/    the window
#   main.cpp
#
# Qt code lives here and in src/app.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# AUTOMOC and the policies of Qt 6.12 (QML modules get their resource prefix and their qmldir the
# current way), for this directory only.
qt_standard_project_setup(REQUIRES 6.12)

add_subdirectory(core)

qt_add_executable(adl_media_gen main.cpp)
qt_add_qml_module(adl_media_gen
    URI ArincMediaGen
    VERSION 1.0
    QML_FILES
        qml/GeneratorWindow.qml)

target_compile_features(adl_media_gen PRIVATE cxx_std_23)
target_link_libraries(adl_media_gen
    PRIVATE
        adl::gen_core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_platform_configure_app(adl_media_gen)
adl_platform_deploy_qt_runtime(adl_media_gen "${CMAKE_CURRENT_SOURCE_DIR}/qml")
adl_deploy_sanitizer_runtime(adl_media_gen)
