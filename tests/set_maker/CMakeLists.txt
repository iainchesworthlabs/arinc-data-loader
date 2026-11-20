# SPDX-License-Identifier: GPL-3.0-or-later
# The media set generator without a window (libs/set_maker): a plan becomes a media set that the loader's own
# check accepts and whatever goes wrong leaves nothing behind; what a set holds is read as facts; the rules
# of the window are applied to a whole plan. Real files in a temporary folder; no Qt, no network.

add_executable(adl_set_maker_tests
    inspect_test.cpp
    make_test.cpp
    plan_test.cpp
    support.hpp
    validate_test.cpp)
target_include_directories(adl_set_maker_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(adl_set_maker_tests
    PRIVATE
        adl::set_maker
        adl::loader_core
        adl::media_tree
        adl::arinc665
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage
        Catch2::Catch2WithMain)

adl_deploy_sanitizer_runtime(adl_set_maker_tests)

foreach(tag make inspect plan validate)
    add_test(NAME set_maker.${tag} COMMAND adl_set_maker_tests "[${tag}]")
    set_tests_properties(set_maker.${tag} PROPERTIES
        TIMEOUT 120
        LABELS "unit")
endforeach()
