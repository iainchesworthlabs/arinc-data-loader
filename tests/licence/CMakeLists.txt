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
