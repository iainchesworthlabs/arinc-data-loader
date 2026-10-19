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

# ---- version and provenance -------------------------------------------------
# Stamped at configure time. The About dialogs print it; a source tarball without git gets
# "unknown" rather than failing. The header is generated once and shared by the three targets.
set(ADL_UI_VERSION "${PROJECT_VERSION}")
set(ADL_UI_VERSION_FULL "${PROJECT_VERSION}")
set(ADL_UI_GIT_COMMIT "unknown")
set(ADL_UI_GIT_COMMIT_FULL "unknown")
set(ADL_UI_GIT_BRANCH "unknown")
set(ADL_UI_GIT_DESCRIBE "v${PROJECT_VERSION}")
set(ADL_UI_GIT_DIRTY 0)

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE ADL_UI_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE ADL_UI_GIT_RESULT)
    if(ADL_UI_GIT_RESULT EQUAL 0)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
            OUTPUT_VARIABLE ADL_UI_GIT_COMMIT_FULL
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" describe --tags --always
            OUTPUT_VARIABLE ADL_UI_GIT_DESCRIBE
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
            RESULT_VARIABLE ADL_UI_DESCRIBE_RESULT)
        if(NOT ADL_UI_DESCRIBE_RESULT EQUAL 0)
            set(ADL_UI_GIT_DESCRIBE "${ADL_UI_GIT_COMMIT}")
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE ADL_UI_GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" status --porcelain
                    --untracked-files=no --ignore-submodules=all
            OUTPUT_VARIABLE ADL_UI_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(NOT ADL_UI_GIT_STATUS STREQUAL "")
            set(ADL_UI_GIT_DIRTY 1)
        endif()
    endif()
endif()

set(ADL_UI_BUILD_TARGET
    "${ADL_PLATFORM_DISPLAY_NAME} ${CMAKE_SYSTEM_PROCESSOR} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")

# A quote or backslash in a branch name must not break the generated string literals.
foreach(adl_ui_var IN ITEMS
        ADL_UI_VERSION ADL_UI_VERSION_FULL ADL_UI_GIT_COMMIT ADL_UI_GIT_COMMIT_FULL
        ADL_UI_GIT_BRANCH ADL_UI_GIT_DESCRIBE ADL_UI_BUILD_TARGET)
    string(REPLACE "\\" "\\\\" ${adl_ui_var} "${${adl_ui_var}}")
    string(REPLACE "\"" "\\\"" ${adl_ui_var} "${${adl_ui_var}}")
endforeach()

set(ADL_UI_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/adl_ui/include")
file(MAKE_DIRECTORY "${ADL_UI_GENERATED_INCLUDE_DIR}/adl/ui")
configure_file("${CMAKE_SOURCE_DIR}/src/ui/version.hpp.in"
               "${ADL_UI_GENERATED_INCLUDE_DIR}/adl/ui/build_info.hpp"
               @ONLY)

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
        "${CMAKE_SOURCE_DIR}/src/ui/window_state.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/version.cpp")
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/ui/include"
        "${ADL_UI_GENERATED_INCLUDE_DIR}")
endfunction()
