# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# Fuzzing.cmake
#
# ADL_ENABLE_FUZZING builds the libFuzzer harnesses under libs/<lib>/fuzz and instruments every
# target for them (AddressSanitizer, UndefinedBehaviorSanitizer and libFuzzer's coverage hooks).
# libFuzzer is Clang's, so the option needs Clang. It cannot be combined with the other sanitizer
# switches, Tracy or coverage; the linux-fuzz preset sets it on its own.
#
# adl_add_fuzz_target(<name> <library>) is called from a fuzz directory that holds <name>.cpp,
# corpus/<name>/ (seed inputs) and regressions/<name>/ (inputs that once crashed it). It adds the
# executable adl_fuzz_<name> and the CTest test fuzz.regress.<name>, which replays both directories
# once (-runs=0) and takes seconds. Fuzzing itself is `adl_fuzz_<name> corpus/<name>` for as long
# as you like (docs/fuzzing.md).
# ---------------------------------------------------------------------------

option(ADL_ENABLE_FUZZING "Build the libFuzzer harnesses (Clang only) and instrument the libraries for them" OFF)
# The sanitizers the harnesses are built with. ClusterFuzzLite builds one sanitizer per run and passes
# it in CFLAGS (.clusterfuzzlite/build.sh), so it sets this empty.
set(ADL_FUZZING_SANITIZERS "address,undefined" CACHE STRING "Comma-separated -fsanitize= list for the libFuzzer harnesses; empty adds none")

if(ADL_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR MSVC)
        message(FATAL_ERROR "ADL_ENABLE_FUZZING needs Clang (libFuzzer): the compiler is ${CMAKE_CXX_COMPILER_ID}.")
    endif()
    foreach(_adl_other IN ITEMS ICL_SANITIZERS ADL_ENABLE_TRACY ADL_ENABLE_COVERAGE)
        if(${_adl_other})
            message(FATAL_ERROR "ADL_ENABLE_FUZZING and ${_adl_other} cannot both be on. Use separate presets.")
        endif()
    endforeach()
    unset(_adl_other)
    set(_adl_fuzz_compile fuzzer-no-link)
    if(ADL_FUZZING_SANITIZERS)
        string(APPEND _adl_fuzz_compile ",${ADL_FUZZING_SANITIZERS}")
        add_link_options(-fsanitize=${ADL_FUZZING_SANITIZERS})
    endif()
    add_compile_options(
        -fsanitize=${_adl_fuzz_compile}
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer
        -O1)
    unset(_adl_fuzz_compile)
    message(STATUS "Fuzzing enabled: libFuzzer harnesses with sanitizers '${ADL_FUZZING_SANITIZERS}'")
endif()

function(adl_add_fuzz_target name library)
    set(target adl_fuzz_${name})
    add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_link_libraries(${target} PRIVATE ${library} adl::warnings)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    add_test(NAME fuzz.regress.${name}
        COMMAND ${target} -runs=0
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}"
            "${CMAKE_CURRENT_SOURCE_DIR}/regressions/${name}")
    set_tests_properties(fuzz.regress.${name} PROPERTIES TIMEOUT 120 LABELS "fuzz")
endfunction()
