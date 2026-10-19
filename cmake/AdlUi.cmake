# Shared QML: one design system for the three ARINC applications.
#
# The QML files are not a separate module. Each application's qt_add_qml_module includes them
# with a root resource alias, so the types are in that application's module and a view uses
#     Theme.accentFill
# without an import. One source, three compiled copies, and no second type registry to go stale.
#
# A view must not name a colour or a pixel size: Theme.qml owns both. Adding a component here
# makes it available to all three windows.

set(ADL_UI_QML_DIR "${CMAKE_SOURCE_DIR}/src/ui/qml")
set(ADL_UI_FONT_DIR "${CMAKE_SOURCE_DIR}/src/ui/fonts")

set(ADL_UI_QML_FILES
    "${ADL_UI_QML_DIR}/Theme.qml"
    "${ADL_UI_QML_DIR}/AppButton.qml"
    "${ADL_UI_QML_DIR}/AppCheckBox.qml"
    "${ADL_UI_QML_DIR}/AppComboBox.qml"
    "${ADL_UI_QML_DIR}/AppDialog.qml"
    "${ADL_UI_QML_DIR}/AppHeader.qml"
    "${ADL_UI_QML_DIR}/AppSpinBox.qml"
    "${ADL_UI_QML_DIR}/AppStatusBar.qml"
    "${ADL_UI_QML_DIR}/AppTextField.qml"
    "${ADL_UI_QML_DIR}/BusyMark.qml"
    "${ADL_UI_QML_DIR}/Card.qml"
    "${ADL_UI_QML_DIR}/CheckTile.qml"
    "${ADL_UI_QML_DIR}/EmptyState.qml"
    "${ADL_UI_QML_DIR}/FocusRing.qml"
    "${ADL_UI_QML_DIR}/LogView.qml"
    "${ADL_UI_QML_DIR}/RailBlock.qml"
    "${ADL_UI_QML_DIR}/SectionHeader.qml"
    "${ADL_UI_QML_DIR}/SegmentedControl.qml"
    "${ADL_UI_QML_DIR}/StatTile.qml"
    "${ADL_UI_QML_DIR}/StatusChip.qml"
    "${ADL_UI_QML_DIR}/TaskProgress.qml")

# A QML module's resource alias is its path relative to the module; these files live outside the
# application's directory, so name each one explicitly or the qmldir would point at ../ui/qml.
# Source-file properties are directory-scoped in CMake, so this runs in each application's
# directory, before that application's qt_add_qml_module call.
function(adl_prepare_ui_qml)
    foreach(ui_file IN LISTS ADL_UI_QML_FILES)
        get_filename_component(ui_name "${ui_file}" NAME)
        set_source_files_properties("${ui_file}" PROPERTIES QT_RESOURCE_ALIAS "${ui_name}")
    endforeach()

    # Theme is a singleton. This must be set before qt_add_qml_module reads the file, or the
    # generated qmldir registers it as an ordinary type and every Theme.* binding is undefined.
    set_source_files_properties("${ADL_UI_QML_DIR}/Theme.qml"
        PROPERTIES
            QT_RESOURCE_ALIAS "Theme.qml"
            QT_QML_SINGLETON_TYPE TRUE)
endfunction()

# Adds the bundled faces, the registration code and the include directory to an application
# target. Call after the target exists.
function(adl_add_ui_support target)
    qt_add_resources(${target} "${target}_adl_ui_fonts"
        PREFIX "/adl/fonts"
        BASE "${ADL_UI_FONT_DIR}"
        FILES
            "${ADL_UI_FONT_DIR}/Archivo-Regular.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-Medium.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-SemiBold.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-ExtraBold.ttf"
            "${ADL_UI_FONT_DIR}/OFL.txt")
    target_sources(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/ui/fonts.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/window_state.cpp")
    target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}/src/ui/include")
endfunction()
