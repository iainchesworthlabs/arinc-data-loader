# Shared QML: one design system for the three ARINC applications.
#
# The QML files are not a separate module. Each application's qt_add_qml_module includes them
# with a root resource alias, so the types are in that application's module and a view uses
#     Theme.accentFill
# without an import. One source, three compiled copies, and no second type registry to go stale.
#
# A view must not name a colour or a pixel size: Theme.qml owns both. Adding a component here
# makes it available to all three windows.

include(Notices)

set(ADL_UI_QML_DIR "${CMAKE_SOURCE_DIR}/src/ui/qml")
set(ADL_UI_FONT_DIR "${CMAKE_SOURCE_DIR}/src/ui/fonts")
set(ADL_UI_NOTICES_DIR "${CMAKE_SOURCE_DIR}/notices")
set(ADL_UI_TRANSLATIONS_DIR "${CMAKE_SOURCE_DIR}/src/ui/translations")

# The languages the three applications ship a catalogue for besides English (whose catalogue holds
# only the plural forms of the source strings). The table of names the picker shows is in
# src/ui/language_manager.cpp; a language is added to both.
set(ADL_TRANSLATED_LANGUAGES de es fr he ja yi zh)
set(ADL_UI_NOTICES_FILE "${CMAKE_BINARY_DIR}/notices/NOTICES.txt")

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
    "${ADL_UI_QML_DIR}/LanguagePicker.qml"
    "${ADL_UI_QML_DIR}/LicencesDialog.qml"
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

# ---- third-party notices ----------------------------------------------------
# The file About > Licences shows and the build places beside the executables: what this build
# ships from others (Qt under the LGPL, the Archivo typeface under the OFL, the runtime files
# Qt's deployment tool adds, Tracy in a profiling build), each with the text of its terms. Written
# once per configure by the first application that asks, from the versions CMake knows, and
# embedded in every application as :/adl/notices/NOTICES.txt (notices/README.md says how to add a
# component). Needs Qt found in the calling scope, for its version.
function(adl_generate_ui_notices)
    get_property(_adl_notices_done GLOBAL PROPERTY ADL_UI_NOTICES_DONE)
    if(_adl_notices_done)
        return()
    endif()
    set_property(GLOBAL PROPERTY ADL_UI_NOTICES_DONE TRUE)

    if(NOT Qt6_VERSION)
        message(FATAL_ERROR "adl_generate_ui_notices: Qt6 must be found before the first "
                            "application asks for its notices")
    endif()
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" _adl_qt_series "${Qt6_VERSION}")

    # What differs by platform is the platform's own to say (src/platforms/<name>/platform.cmake).
    set(_adl_fragments header qt ${ADL_PLATFORM_NOTICE_FRAGMENTS} fonts)
    if(ADL_ENABLE_TRACY)
        list(APPEND _adl_fragments tracy)
    endif()
    list(APPEND _adl_fragments standards)

    if(Tracy_VERSION)
        set(_adl_tracy_version "${Tracy_VERSION}")
    else()
        set(_adl_tracy_version "(version not reported by its package)")
    endif()

    adl_generate_notices("${ADL_UI_NOTICES_FILE}"
        FRAGMENT_DIR "${ADL_UI_NOTICES_DIR}/fragments"
        FRAGMENTS ${_adl_fragments}
        TOKENS
            "VERSION=${PROJECT_VERSION}"
            "PLATFORM=${ADL_PLATFORM_DISPLAY_NAME}"
            "QT_VERSION=${Qt6_VERSION}"
            "QT_SERIES=${_adl_qt_series}"
            "QT_DEPLOYMENT=${ADL_PLATFORM_NOTICE_QT_DEPLOYMENT}"
            "TRACY_VERSION=${_adl_tracy_version}"
        FILES
            "LGPL3=${ADL_UI_NOTICES_DIR}/licences/LGPL-3.0.txt"
            "OFL=${ADL_UI_FONT_DIR}/OFL.txt"
            "MESA_MIT=${ADL_UI_NOTICES_DIR}/licences/MIT-mesa.txt"
            "DXC_NCSA=${ADL_UI_NOTICES_DIR}/licences/NCSA-dxc.txt"
            "TRACY_BSD=${ADL_UI_NOTICES_DIR}/licences/BSD-3-Clause-Tracy.txt")

    # NOTICES.txt and LICENSE.txt beside the executables, so the Qt libraries deployed there travel
    # with the licence that asks for it. One target for all the programs: they share the bin
    # directory, and two copies of one file at once would collide.
    add_custom_target(adl_notices ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ADL_UI_NOTICES_FILE}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/NOTICES.txt"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/LICENSE.txt"
        COMMENT "Placing NOTICES.txt and LICENSE.txt beside the executables")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/LICENSE")
endfunction()

# ---- translations -----------------------------------------------------------
# Each application has a catalogue of its own (<directory>/<base>_<code>.ts for the languages in
# ADL_TRANSLATED_LANGUAGES, and <base>_en.ts which holds only the plural forms English needs),
# and the interface the three share has one more, src/ui/translations/adl_ui_<code>.ts, so a
# string a shared component draws is translated once. LanguageManager installs both.
#
# Setting the files up: `cmake --build <dir> --target update_translations` extracts the strings
# from the sources into every catalogue (an entry that is new arrives `unfinished` and a test
# fails until it has a translation); lrelease compiles them, and they are embedded under :/i18n.
# The tests read the .ts files too (tests/unit/translations_test.cpp) and the `translations.*`
# tests rerun lupdate on a copy to see that nothing has drifted from the sources.
function(adl_translation_files out directory base)
    set(files "")
    foreach(language IN LISTS ADL_TRANSLATED_LANGUAGES)
        list(APPEND files "${directory}/${base}_${language}.ts")
    endforeach()
    set(${out} ${files} PARENT_SCOPE)
endfunction()

# A test that runs the catalogues' lupdate project over a copy of the catalogues and fails if the
# result differs from the committed files in anything but the <location> lines: a message the
# sources gained, lost or reworded without the catalogues following. It reuses the project file the
# lupdate target writes (the same sources, headers and include paths, so the same contexts), with
# the catalogues replaced by copies. NAME is the test name suffix, LUPDATE_TARGET the target whose
# project it runs.
function(adl_add_translation_check name)
    cmake_parse_arguments(arg "" "BASE;DIRECTORY;LUPDATE_TARGET" "" ${ARGN})
    adl_translation_files(ts_files "${arg_DIRECTORY}" "${arg_BASE}")
    list(JOIN ts_files "|" ts_files_arg)
    add_test(NAME translations.current.${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DLUPDATE=$<TARGET_FILE:Qt6::lupdate>"
            "-DLCONVERT=$<TARGET_FILE:Qt6::lconvert>"
            "-DPROJECT_CMAKE=${CMAKE_CURRENT_BINARY_DIR}/.lupdate/${arg_LUPDATE_TARGET}_project.cmake"
            "-DPROJECT_GENERATOR=${_Qt6_LINGUIST_TOOLS_DIR}/GenerateLUpdateProject.cmake"
            "-DTS_FILES=${ts_files_arg}"
            "-DPLURALS_TS_FILE=${arg_DIRECTORY}/${arg_BASE}_en.ts"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/translations_check_${name}"
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckTranslations.cmake")
    set_tests_properties(translations.current.${name} PROPERTIES
        TIMEOUT 120
        LABELS "unit")
endfunction()

# The shared interface's catalogue, built once for all the applications: lupdate over the shared
# QML, lrelease into ${CMAKE_BINARY_DIR}/i18n. adl_add_ui_support embeds the result.
function(adl_generate_ui_translations)
    get_property(_adl_translations_done GLOBAL PROPERTY ADL_UI_TRANSLATIONS_DONE)
    if(_adl_translations_done)
        return()
    endif()
    set_property(GLOBAL PROPERTY ADL_UI_TRANSLATIONS_DONE TRUE)

    find_package(Qt6 6.12 REQUIRED COMPONENTS LinguistTools)
    adl_translation_files(ts_files "${ADL_UI_TRANSLATIONS_DIR}" adl_ui)
    set(plurals_ts_file "${ADL_UI_TRANSLATIONS_DIR}/adl_ui_en.ts")
    set(ui_sources ${ADL_UI_QML_FILES})

    qt_add_lupdate(
        SOURCES ${ui_sources}
        TS_FILES ${ts_files}
        PLURALS_TS_FILE "${plurals_ts_file}"
        OPTIONS -no-obsolete
        LUPDATE_TARGET adl_ui_lupdate)
    qt_add_lrelease(
        TS_FILES ${ts_files} "${plurals_ts_file}"
        LRELEASE_TARGET adl_ui_lrelease
        QM_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/i18n"
        QM_FILES_OUTPUT_VARIABLE qm_files)
    set_property(GLOBAL PROPERTY ADL_UI_QM_FILES "${qm_files}")
    adl_add_translation_check(ui BASE adl_ui DIRECTORY "${ADL_UI_TRANSLATIONS_DIR}"
        LUPDATE_TARGET adl_ui_lupdate)
endfunction()

# The application's own catalogues on its QML module: lupdate over the sources of `qml_target`
# (minus the shared QML, whose strings are in adl_ui) and of the SOURCE_TARGETS (the application's
# core library: lupdate has to see its headers to know which namespace a class with tr() is in, so
# its files are given as a target and not one by one); lrelease; and the compiled files embedded in
# `qml_target` under :/i18n. `base` is the catalogue name LanguageManager is given. `SOURCES` are
# any files besides, the QML of the window.
function(adl_add_app_translations qml_target base)
    cmake_parse_arguments(arg "" "DIRECTORY" "SOURCES;SOURCE_TARGETS" ${ARGN})
    find_package(Qt6 6.12 REQUIRED COMPONENTS LinguistTools)
    adl_translation_files(ts_files "${arg_DIRECTORY}" "${base}")

    # The shared QML is a source of the module (it is compiled into it) but its strings belong to
    # the shared catalogue.
    set_property(TARGET ${qml_target} PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION
        ${ADL_UI_QML_FILES})

    # -no-obsolete: an entry whose source string is gone is not kept as translation memory (git
    # history is that); a catalogue lists what the window says.
    qt_add_translations(${qml_target}
        IMMEDIATE_CALL
        SOURCE_TARGETS ${qml_target} ${arg_SOURCE_TARGETS}
        LUPDATE_OPTIONS -no-obsolete
        LUPDATE_TARGET ${base}_lupdate
        LRELEASE_TARGET ${base}_lrelease
        TS_FILES ${ts_files}
        PLURALS_TS_FILE "${arg_DIRECTORY}/${base}_en.ts"
        SOURCES ${arg_SOURCES})

    adl_add_translation_check(${base} BASE ${base} DIRECTORY "${arg_DIRECTORY}"
        LUPDATE_TARGET ${base}_lupdate)
endfunction()

# Adds the bundled faces, the registration code, the third-party notices and the include
# directory to an application target. Call after the target exists.
function(adl_add_ui_support target)
    adl_generate_ui_notices()
    adl_generate_ui_translations()
    # LanguageManager is a QObject: its moc output is compiled into every target that has it.
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    qt_add_resources(${target} "${target}_adl_ui_notices"
        PREFIX "/adl/notices"
        BASE "${CMAKE_BINARY_DIR}/notices"
        FILES "${ADL_UI_NOTICES_FILE}")
    qt_add_resources(${target} "${target}_adl_ui_fonts"
        PREFIX "/adl/fonts"
        BASE "${ADL_UI_FONT_DIR}"
        FILES
            "${ADL_UI_FONT_DIR}/Archivo-Regular.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-Medium.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-SemiBold.ttf"
            "${ADL_UI_FONT_DIR}/Archivo-ExtraBold.ttf"
            "${ADL_UI_FONT_DIR}/OFL.txt")
    get_property(_adl_ui_qm_files GLOBAL PROPERTY ADL_UI_QM_FILES)
    set_source_files_properties(${_adl_ui_qm_files} PROPERTIES GENERATED TRUE)
    qt_add_resources(${target} "${target}_adl_ui_i18n"
        PREFIX "/i18n"
        BASE "${CMAKE_BINARY_DIR}/i18n"
        FILES ${_adl_ui_qm_files})
    add_dependencies(${target} adl_ui_lrelease)
    target_sources(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/ui/include/adl/ui/language_manager.hpp"
        "${CMAKE_SOURCE_DIR}/src/ui/language_manager.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/fonts.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/window_state.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/version.cpp")
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src/ui/include"
        "${ADL_UI_GENERATED_INCLUDE_DIR}")
endfunction()
