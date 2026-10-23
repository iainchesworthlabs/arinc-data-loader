# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# CompilerWarnings.cmake
#
# Defines an INTERFACE target `adl::warnings` that turns on a strict,
# cross-compiler warning set with "warnings as errors". Link it PRIVATE-ly
# into every first-party target (library, app, tests). Third-party code (Qt,
# vcpkg packages) is pulled in as SYSTEM headers by their package configs, so
# these flags never fire on dependency code.
# ---------------------------------------------------------------------------

add_library(adl_warnings INTERFACE)
add_library(adl::warnings ALIAS adl_warnings)

# The mandated baseline is -Wall -Wextra -Werror; the remaining flags catch the
# classes of mistake (shadowing, dubious conversions, missing overrides) that a
# generator is most likely to introduce.
set(ADL_GNU_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wformat=2)

set(ADL_MSVC_WARNINGS
    /W4
    /WX
    /permissive-)

# clang-cl reports CXX_COMPILER_ID "Clang" (it would otherwise get
# ADL_GNU_CLANG_WARNINGS below), but its -Wall alias is bound to
# MSVC's /Wall semantics - "warning level 4 and everything disabled by
# default" - not plain clang's moderate -Wall. That silently turns on
# off-by-default groups like -Wc++98-compat, which then fail the build on
# ordinary C++23 code (scoped enums, [[nodiscard]], nested namespaces, ...).
# /W4 is what clang-cl itself recommends in place of -Wall for this reason.
set(ADL_CLANG_CL_WARNINGS
    /W4
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wformat=2)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(adl_warnings INTERFACE ${ADL_CLANG_CL_WARNINGS})
else()
    target_compile_options(adl_warnings INTERFACE
        "$<$<CXX_COMPILER_ID:MSVC>:${ADL_MSVC_WARNINGS}>"
        "$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${ADL_GNU_CLANG_WARNINGS}>")
endif()

# GCC 16.2 at -O3 reports a false positive from -Wrestrict for a std::string concatenation
# (std::string{directory} + "/" + name in apps/app/core/src/download_directory.cpp: "accessing
# 9223372036854775806 bytes ... overlaps"), where the compiler assumes the longest possible
# length for a string_view. The warning stays visible; it is not an error.
target_compile_options(adl_warnings INTERFACE "$<$<CXX_COMPILER_ID:GNU>:-Wno-error=restrict>")
