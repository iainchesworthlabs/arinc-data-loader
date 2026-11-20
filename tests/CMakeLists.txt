# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# clang-tidy analyzes the shipped code (src/). The tests are not tidied:
# Catch2's REQUIRE/CHECK macros expand to code the checks cannot see through
# (a REQUIRE(has_value()) followed by *value is the house style), and the
# sanitizer legs run them. Clear the launcher before any test target exists.
if(ADL_ENABLE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "")
endif()

add_executable(adl_unit_tests
    unit/arguments_test.cpp
    unit/arinc615a_golden_test.cpp
    unit/arinc615a_robustness_test.cpp
    unit/batch_file_test.cpp
    unit/check_value_type_test.cpp
    unit/compatibility_test.cpp
    unit/crc_test.cpp
    unit/data_locations_test.cpp
    unit/digest_test.cpp
    unit/find_test.cpp
    unit/golden_test.cpp
    unit/header_file_test.cpp
    unit/integrity_checker_test.cpp
    unit/limits_test.cpp
    unit/load_test.cpp
    unit/media_files_test.cpp
    unit/media_label_test.cpp
    unit/media_set_test.cpp
    unit/media_tree_test.cpp
    unit/naming_test.cpp
    unit/portable_name_test.cpp
    unit/part_number_test.cpp
    unit/paths_test.cpp
    unit/protocol_files_test.cpp
    unit/public_api_test.cpp
    unit/robustness_test.cpp
    unit/tftp_options_test.cpp
    unit/tftp_test.cpp
    unit/translations_test.cpp)

# libs/platform is here for the platform's own header-only rules (detail/data_locations.hpp), which
# hold no operating system and are tested on every host.
target_include_directories(adl_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/golden"
    "${PROJECT_SOURCE_DIR}/libs/platform")

# The Qt Linguist catalogues the translation rules read as files (they need no Qt).
target_compile_definitions(adl_unit_tests PRIVATE
    ADL_UI_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/libs/ui/translations"
    ADL_APP_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/apps/app/translations"
    ADL_MEDIA_GEN_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/apps/media_gen/translations"
    ADL_MOCK_TARGET_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/apps/mock_target/translations")

target_link_libraries(adl_unit_tests
    PRIVATE
        adl::arinc615a
        adl::arinc665
        adl::media_tree
        adl::platform
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

# An optimised GCC build reports "potential null pointer dereference" for every indexing of a
# std::span made from a std::vector in tests/support/edit.hpp (the helpers that damage a file on
# purpose). The vector is not null there - a null pointer would be an empty one, which the tests
# never index - and the Debug legs, which are the ones CI compiles, do not optimise enough to see
# it, so the warning first appeared in the Release build. It is turned off for this target only, in a
# target of its own linked after adl::warnings, because the flags of linked targets are given in
# link order and a -Wno- before -Wnull-dereference would turn it back on.
add_library(adl_unit_test_flags INTERFACE)
target_compile_options(adl_unit_test_flags INTERFACE
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-null-dereference>")
target_link_libraries(adl_unit_tests PRIVATE adl_unit_test_flags)

adl_deploy_sanitizer_runtime(adl_unit_tests)

catch_discover_tests(adl_unit_tests PROPERTIES LABELS "unit")

# The fake platform: a clock, network, file system, memory and threads that run in
# memory and on a clock that only moves when told to (tests/fake). The engine is
# tested against it; the conformance tests hold it to the real hosts' behaviour.
add_library(adl_fake_platform STATIC
    fake/fake_host.cpp
    fake/fake_network.cpp
    fake/fake_storage.cpp
    fake/fake_threading.cpp
    fake/fake_world.cpp)
target_include_directories(adl_fake_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/fake")
target_link_libraries(adl_fake_platform
    PUBLIC
        adl::platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The simulated target (a target of 615A on the fake network, with the second TFTP
# implementation of libs/arinc615a_mini, which shares no code with the engine). The engine tests
# run whole operations against it, and so do the application's.
add_library(adl_sim_target STATIC
    engine/sim_target.cpp)
target_include_directories(adl_sim_target PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine")
target_link_libraries(adl_sim_target
    PUBLIC
        adl_fake_platform
        adl::arinc615a
        adl::arinc615a_mini
    PRIVATE
        adl::arinc665
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The loader core (libs/loader_core): what the loader does without a window, run without one, on the
# fake platform's files.
add_executable(adl_loader_core_tests
    loader_core/address_test.cpp
    loader_core/download_directory_test.cpp
    loader_core/gate_test.cpp
    loader_core/media_set_check_test.cpp
    loader_core/media_source_test.cpp
    loader_core/operation_plan_test.cpp
    loader_core/record_json_test.cpp
    loader_core/recorder_test.cpp
    loader_core/recorder_wording_test.cpp)

target_include_directories(adl_loader_core_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support")

# What the record's tests read as files: the window's translation catalogue (its English is what the recorder
# is held to), the golden files of the operation record, and the page that documents the record's keys.
target_compile_definitions(adl_loader_core_tests PRIVATE
    ADL_APP_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/apps/app/translations"
    ADL_LOADER_CORE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/loader_core/golden"
    ADL_FILE_REFERENCE_PAGE="${PROJECT_SOURCE_DIR}/docs/file-reference.md")

target_link_libraries(adl_loader_core_tests
    PRIVATE
        adl_fake_platform
        adl_sim_target
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::loader_core
        adl::media_tree
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_loader_core_tests)

# The cases that run a whole operation against the simulated target are integration tests (tagged
# [integration]); the rest are unit tests.
catch_discover_tests(adl_loader_core_tests TEST_SPEC "~[integration]" PROPERTIES LABELS "unit")
catch_discover_tests(adl_loader_core_tests TEST_SPEC "[integration]" PROPERTIES LABELS "integration")

# The same library run whole: run_operation against the simulated target on the fake platform, so the
# runner, the engine and the second TFTP implementation are in it. An integration test.
add_executable(adl_loader_core_run_tests
    loader_core/run_test.cpp)

target_include_directories(adl_loader_core_run_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support")

target_link_libraries(adl_loader_core_run_tests
    PRIVATE
        adl_fake_platform
        adl_sim_target
        adl::loader_core
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_loader_core_run_tests)

catch_discover_tests(adl_loader_core_run_tests PROPERTIES LABELS "integration")

# The command-line program adl_load (apps/load): its options and its events, which need no host.
add_executable(adl_load_tests
    load/events_test.cpp
    load/options_seeds_test.cpp
    load/options_test.cpp
    load/reason_test.cpp)

target_include_directories(adl_load_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support")

# The seeds of the options fuzzer, which the tests replay as files.
target_compile_definitions(adl_load_tests PRIVATE
    ADL_LOAD_FUZZ_DIR="${PROJECT_SOURCE_DIR}/apps/load/fuzz")

target_link_libraries(adl_load_tests
    PRIVATE
        adl_load_cli
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_load_tests)

catch_discover_tests(adl_load_tests PROPERTIES LABELS "unit")

# The same program run whole: adl_load's run function on the fake platform against the simulated target, so the
# runner, the engine and the second TFTP implementation are in it. Integration tests, with no socket.
add_executable(adl_load_command_tests
    load/download_test.cpp
    load/find_test.cpp
    load/information_test.cpp
    load/record_test.cpp
    load/upload_test.cpp
    load/usage_test.cpp)

target_include_directories(adl_load_command_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/load"
    "${CMAKE_CURRENT_SOURCE_DIR}/support")

# The records and the other output that the tests compare with files that have been read and kept.
target_compile_definitions(adl_load_command_tests PRIVATE
    ADL_LOAD_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/load/golden")

target_link_libraries(adl_load_command_tests
    PRIVATE
        adl_fake_platform
        adl_load_cli
        adl_sim_target
        adl::arinc665
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_load_command_tests)

catch_discover_tests(adl_load_command_tests PROPERTIES LABELS "integration")

# The 615A engine: the TFTP transfer and the data loader state machines, the runner over the
# fake platform, and the simulated target the whole of an operation is run against.
add_executable(adl_engine_tests
    engine/data_loader_test.cpp
    engine/directory_files_test.cpp
    engine/find_client_test.cpp
    engine/operations_test.cpp
    engine/tftp_transfer_test.cpp)

target_include_directories(adl_engine_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/support")

target_link_libraries(adl_engine_tests
    PRIVATE
        adl_fake_platform
        adl_sim_target
        adl::arinc615a
        adl::arinc615a_runner
        adl::arinc665
        adl::platform
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_engine_tests)

catch_discover_tests(adl_engine_tests PROPERTIES LABELS "integration")

# The platform layer: the portable types, the fake, and the conformance suite that runs
# the same checks against the fake and against the real host this build selected.
add_executable(adl_platform_tests
    conformance/clock_memory_test.cpp
    conformance/files_test.cpp
    conformance/network_test.cpp
    conformance/threads_ipc_test.cpp
    unit/fake_platform_test.cpp
    unit/memory_test.cpp
    unit/platform_types_test.cpp)

target_include_directories(adl_platform_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/conformance")

target_link_libraries(adl_platform_tests
    PRIVATE
        adl_fake_platform
        adl::platform
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_platform_tests)

# The contract checks run against the real host as well as the fake, so the program is both a
# unit suite (the fake and the portable types) and an integration suite (the real host).
catch_discover_tests(adl_platform_tests PROPERTIES LABELS "unit;integration")

# Every platform fragment satisfies the platform contract, on whichever host
# this runs (a build only includes the one fragment for its own platform).
add_test(NAME platform.fragments
    COMMAND "${CMAKE_COMMAND}"
        "-DREPO_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/platform/check_fragments.cmake")
set_tests_properties(platform.fragments PROPERTIES LABELS "system")

# The selector learns the target the way docs/platform-isolation.md says (an explicit
# ADL_TARGET_PLATFORM for a cross build), and refuses a platform with no code to
# build. It configures the project several times, so it is not the quickest test.
add_test(NAME platform.selector
    COMMAND "${CMAKE_COMMAND}"
        "-DREPO_ROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/selector"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DCOMPILER=${CMAKE_CXX_COMPILER}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/platform/check_selector.cmake")
set_tests_properties(platform.selector PROPERTIES LABELS "system")

# No preprocessor conditionals or defines, no OS headers outside the platform
# section, no OS tests in CMake outside the selector. CI runs it as its own job
# too; this is the same check, so a developer sees it fail locally.
find_program(ADL_PWSH NAMES pwsh)
if(ADL_PWSH)
    add_test(NAME platform.isolation
        COMMAND "${ADL_PWSH}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/checks/check_platform_isolation.ps1"
            -Root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(platform.isolation PROPERTIES LABELS "system")
endif()

# Every relative link and #anchor in the Markdown files resolves (documentation, README, the user's
# guide and its pictures). No network: links to other sites are not followed.
if(ADL_PWSH)
    add_test(NAME docs.links
        COMMAND "${ADL_PWSH}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/checks/check_doc_links.ps1"
            -Root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(docs.links PROPERTIES LABELS "system")

    # Every repository path that the Markdown files name in a code span exists, so a page cannot go
    # on naming a file after it has moved. The allow-list of placeholders is in the script.
    add_test(NAME docs.paths
        COMMAND "${ADL_PWSH}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/checks/check_doc_paths.ps1"
            -Root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(docs.paths PROPERTIES LABELS "system")
endif()

# The bill of materials names the libraries by hand (tools/release/make_sbom.py), and the development
# package ships the ones that call adl_install_library. A library on one side only fails here, not on the
# day of a release.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME release.sbom_libraries
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/release/check_sbom_libraries.py"
            "${PROJECT_SOURCE_DIR}")
    set_tests_properties(release.sbom_libraries PROPERTIES LABELS "system")
endif()

# The script that compares the benchmarks with the budget (tools/ci/perf_compare.py) is held to what it
# promises: a result over the limit, a missing or surplus entry and a few nanoseconds of noise.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME ci.perf_compare
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/test_perf_compare.py")
    set_tests_properties(ci.perf_compare PROPERTIES LABELS "system")

    # The checks of the documentation site (tools/ci/check_site_kit.py, check_site_address.py and
    # check_site_layout.py), the old-address pages and the layout hook are held to what they promise: each
    # passes a good tree and fails, with a reason, on a bad one. No MkDocs and no network; the palette
    # tests need Node and are skipped without it.
    add_test(NAME ci.site_checks
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/test_site_checks.py")
    set_tests_properties(ci.site_checks PROPERTIES LABELS "system")

    # Every message of the programs' code that says something to the person using them, and every exit code, is
    # described in the user's guide (tools/ci/check_messages.py, docs/ci.md), and the check itself is held to what it
    # promises. No network, no MkDocs.
    add_test(NAME docs.messages
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/check_messages.py"
            --root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(docs.messages PROPERTIES LABELS "system")
    add_test(NAME ci.check_messages
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/test_check_messages.py")
    set_tests_properties(ci.check_messages PROPERTIES LABELS "system")

    # No page says the tool is certified, qualified, compliant, approved or guaranteed without an exception the owner
    # approved (tools/ci/check_claims.py, tools/docs/claims_exceptions.json, docs/ci.md), and the links from the
    # standards' coverage tables to the tests resolve (tools/ci/check_traceability.py). Each check has its own test.
    add_test(NAME docs.claims
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/check_claims.py"
            --root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(docs.claims PROPERTIES LABELS "system")
    add_test(NAME ci.check_claims
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/test_check_claims.py")
    set_tests_properties(ci.check_claims PROPERTIES LABELS "system")
    add_test(NAME docs.traceability
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/check_traceability.py"
            --root "${PROJECT_SOURCE_DIR}")
    set_tests_properties(docs.traceability PROPERTIES LABELS "system")
    add_test(NAME ci.check_traceability
        COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/ci/test_check_traceability.py")
    set_tests_properties(ci.check_traceability PROPERTIES LABELS "system")
endif()

# The program that writes the sample media sets: it runs on a directory of its
# own (emptied first, since it never overwrites), and refuses what it cannot use.
add_test(NAME tools.write_payloads.clean
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads.clean PROPERTIES
    FIXTURES_SETUP write_payloads_output
    LABELS "system")

add_test(NAME tools.write_payloads
    COMMAND adl_write_payloads "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads PROPERTIES
    FIXTURES_REQUIRED write_payloads_output
    LABELS "system")

add_test(NAME tools.write_payloads.unknown_scenario
    COMMAND adl_write_payloads --scenario no-such-scenario "${CMAKE_CURRENT_BINARY_DIR}/payloads")
set_tests_properties(tools.write_payloads.unknown_scenario PROPERTIES
    WILL_FAIL TRUE
    LABELS "system")

add_test(NAME tools.write_payloads.no_output_directory COMMAND adl_write_payloads)
set_tests_properties(tools.write_payloads.no_output_directory PROPERTIES
    WILL_FAIL TRUE
    LABELS "system")

add_test(NAME tools.write_payloads.list COMMAND adl_write_payloads --list)
set_tests_properties(tools.write_payloads.list PROPERTIES LABELS "system")

# adl_load started as a process, for what needs no target (tests/load/check_program.cmake): --version, --help, wrong
# usage, and a media set that is not there.
add_test(NAME tools.adl_load
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:adl_load>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/adl_load"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/load/check_program.cmake")
set_tests_properties(tools.adl_load PROPERTIES LABELS "system")

# The same program with a media set, a download folder and records in a folder named with characters no Windows code
# page holds all of (tests/load/unicode_folder.cmake says how it is made sure the name reached the program whole).
add_test(NAME tools.adl_load.unicode_folder
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:adl_load>"
        "-DPAYLOADS=$<TARGET_FILE:adl_write_payloads>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/adl_load-unicode"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/load/unicode_folder.cmake")
set_tests_properties(tools.adl_load.unicode_folder PROPERTIES LABELS "system")

# The folder is named with an accented letter, Japanese and a character outside the Basic Multilingual Plane, which
# no Windows code page holds all of, and the program starts as a process with that name as its argument
# (tests/system/write_payloads_unicode.cmake says how it is made sure the name reached it whole).
add_test(NAME tools.write_payloads.unicode_folder
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:adl_write_payloads>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/payloads-unicode"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/system/write_payloads_unicode.cmake")
set_tests_properties(tools.write_payloads.unicode_folder PROPERTIES LABELS "system")

# The application's tests (apps/app), when it is built: they need Qt.
if(ADL_BUILD_APP)
    add_subdirectory("${PROJECT_SOURCE_DIR}/apps/app/tests" apps/app/tests)
    # The mock target's tests (apps/mock_target).
    add_subdirectory("${PROJECT_SOURCE_DIR}/apps/mock_target/tests" apps/mock_target/tests)
    # The media set generator's tests (apps/media_gen).
    add_subdirectory("${PROJECT_SOURCE_DIR}/apps/media_gen/tests" apps/media_gen/tests)
    # The loopback integration test: the loader's runner over real sockets against the mock
    # target's engine on 127.0.0.1 (needs adl::mock_core, so it is behind ADL_BUILD_APP too).
    add_subdirectory(loopback)
    # The window's UI suite, with Qt Quick Test (tests/ui).
    add_subdirectory(ui)
    # The system smoke tests: the built application programs started as processes (tests/system).
    add_subdirectory(system)
endif()

# The development package (cmake/AdlInstall.cmake): install its component into a scratch prefix and
# build a program against it with find_package(adl). Only for an optimised build: the Debug presets
# compile the libraries with the sanitizers, and a program that links such a library has to be
# built with them too, which is not what a user of the package does. The same holds for the
# sanitizer presets (ICL_SANITIZERS), which build RelWithDebInfo.
if(ADL_PLATFORM_DESKTOP AND CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$" AND NOT ICL_SANITIZERS)
    add_test(NAME system.install_consumer
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DWORK_DIR=${CMAKE_BINARY_DIR}/install-consumer"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DDOC_DIR=${ADL_PLATFORM_INSTALL_DOCDIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/check_install.cmake")
    set_tests_properties(system.install_consumer PROPERTIES TIMEOUT 300 LABELS "system")
endif()

# The three tiers by label, so a developer can run one of them without remembering which
# executables hold which tests:
#
#   cmake --build <build dir> --target check-unit
#   cmake --build <build dir> --target check-integration
#   cmake --build <build dir> --target check-system
foreach(tier unit integration system)
    add_custom_target(check-${tier}
        # The scratch space is under the build tree, as it is for `ctest --preset`: a test that
        # makes a temporary directory must not depend on the account's %TEMP% being writable.
        COMMAND "${CMAKE_COMMAND}" -E env
            "TEMP=${CMAKE_BINARY_DIR}"
            "TMP=${CMAKE_BINARY_DIR}"
            "TMPDIR=${CMAKE_BINARY_DIR}"
            "${CMAKE_CTEST_COMMAND}" --output-on-failure --no-tests=error -L "${tier}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Running the ${tier} tests"
        USES_TERMINAL)
endforeach()
