# ---------------------------------------------------------------------------
# Coverage.cmake
#
# Defines an INTERFACE target `adl::coverage` that, when ADL_ENABLE_COVERAGE is
# on, turns on gcov-style source-based coverage instrumentation (GCC/Clang)
# via --coverage. Off by default: only the dedicated linux-gcc-coverage preset
# turns it on, so normal dev/CI builds pay no instrumentation cost.
#
# Link it PRIVATE into every first-party target whose coverage should be
# measured (the library and the test executables).
# ---------------------------------------------------------------------------

option(ADL_ENABLE_COVERAGE "Enable gcov/llvm-cov source coverage instrumentation" OFF)

add_library(adl_coverage INTERFACE)
add_library(adl::coverage ALIAS adl_coverage)

if(ADL_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # -fprofile-update=atomic: the code under test may be
        # exercised from several threads in the tests, and gcov's default
        # non-atomic counter updates then race. gcov derives the counts of arcs
        # it does not instrument by subtraction, so a lost update shows up as a
        # negative count and gcovr aborts the report (gcc bug 68080).
        target_compile_options(adl_coverage INTERFACE --coverage -fprofile-update=atomic -fno-inline)
        target_link_options(adl_coverage INTERFACE --coverage)
    else()
        message(WARNING
            "ADL_ENABLE_COVERAGE is on but ${CMAKE_CXX_COMPILER_ID} is not "
            "GCC/Clang; coverage instrumentation is not supported on this "
            "compiler and will be skipped.")
    endif()
endif()
