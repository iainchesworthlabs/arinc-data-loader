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
    unit/arinc615a_golden_test.cpp
    unit/arinc615a_robustness_test.cpp
    unit/batch_file_test.cpp
    unit/check_value_type_test.cpp
    unit/compatibility_test.cpp
    unit/crc_test.cpp
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

target_include_directories(adl_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/golden")

# The Qt Linguist catalogues the translation rules read as files (they need no Qt).
target_compile_definitions(adl_unit_tests PRIVATE
    ADL_UI_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/src/ui/translations"
    ADL_APP_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/src/app/translations"
    ADL_MEDIA_GEN_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/src/media_gen/translations"
    ADL_MOCK_TARGET_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/src/mock_target/translations")

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
# implementation of src/arinc615a_mini, which shares no code with the engine). The engine tests
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

# The application's tests (src/app), when it is built: they need Qt.
if(ADL_BUILD_APP)
    add_subdirectory(app)
    # The mock target's tests (src/mock_target).
    add_subdirectory(mock_target)
    # The media set generator's tests (src/media_gen).
    add_subdirectory(media_gen)
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
# built with them too, which is not what a user of the package does.
if(ADL_PLATFORM_DESKTOP AND CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
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
