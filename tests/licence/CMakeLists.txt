# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
# The licence library's tests (libs/licence): the signature check against the vectors of RFC 8032 and Project
# Wycheproof, the signed envelope, the payloads, every state a licence can be in, the strict JSON reader
# behind them, and the files an independent writer made (tests/golden/make_golden_licence.py). They sign with
# the published test key of RFC 8032 and nothing else: no real key exists in this repository.

add_executable(adl_licence_tests
    golden_test.cpp
    json_test.cpp
    licence_test.cpp
    signature_test.cpp
    words_test.cpp)

target_include_directories(adl_licence_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/golden"
    # The strict JSON reader is internal to the library; its own tests and the Wycheproof test read it.
    "${PROJECT_SOURCE_DIR}/libs/licence/src")

# Where the Wycheproof vectors are.
target_compile_definitions(adl_licence_tests PRIVATE
    ADL_LICENCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

target_link_libraries(adl_licence_tests
    PRIVATE
        adl::licence_test_keys
        adl::monocypher
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_licence_tests)

catch_discover_tests(adl_licence_tests PROPERTIES LABELS "unit")

# ---------------------------------------------------------------------------
# The owner's licence tool (tools/licence)
# ---------------------------------------------------------------------------
# In process: the rules of its own parts and the commands run against streams (unit).
add_executable(adl_licence_owner_tests owner_test.cpp)
target_link_libraries(adl_licence_owner_tests
    PRIVATE
        adl_licence_owner_core
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)
adl_deploy_sanitizer_runtime(adl_licence_owner_tests)
catch_discover_tests(adl_licence_owner_tests PROPERTIES LABELS "unit")

# As a process: the built program is started with arguments and its output and files are read (system). Every
# command and refusal, a key made and used from end to end, names outside the code page, the manifest of test
# licences judged by `verify`, and the test licences made again from their requests (test_owner_tool.py).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME licence.owner_tool
        COMMAND "${Python3_EXECUTABLE}" -X utf8 -I "${CMAKE_CURRENT_SOURCE_DIR}/test_owner_tool.py"
            --tool "$<TARGET_FILE:adl_licence_owner>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
    set_tests_properties(licence.owner_tool PROPERTIES TIMEOUT 300 LABELS "system")

    # Writes tests/licence/data/licences again from tests/licence/data/requests, byte for byte (make_test_licences.py
    # says how). Not part of `all`: run it when a request changes, then commit what it wrote.
    add_custom_target(licence_test_licences
        COMMAND "${Python3_EXECUTABLE}" -X utf8 -I "${CMAKE_CURRENT_SOURCE_DIR}/make_test_licences.py"
            --tool "$<TARGET_FILE:adl_licence_owner>"
        DEPENDS adl_licence_owner
        COMMENT "Making the test licences again from tests/licence/data/requests"
        VERBATIM)
endif()
