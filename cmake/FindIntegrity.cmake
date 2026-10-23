# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# FindIntegrity.cmake - find a Green Hills INTEGRITY 5.x installation, the
# board package that goes with it, and the GHS toolchain that builds for it.
#
#   find_package(Integrity)
#
# This is a find module, not a package config: INTEGRITY distributions are
# installed by the vendor's installer and carry no CMake files. It finds two
# things that are usually in different places:
#
#   INTEGRITY_ROOT      the installation (the vendor's "int508" directory):
#                       include/INTEGRITY.h, the POSIX and socket headers, and
#                       the runtime libraries. The .gpj files call this -os_dir.
#   INTEGRITY_BSP_ROOT  the board package. On this project's machines that is
#                       C:\Repos\common\integrity\int508: bsp/, target/ and the
#                       prebuilt rsimp2a/ (or riu5554/) runtime.
#
# Inputs (all cache variables, all optional):
#   INTEGRITY_ROOT            installation root
#   INTEGRITY_BSP_ROOT        board package root
#   INTEGRITY_BSP             board name, default rsimp2a
#   INTEGRITY_TARGET_CPU      default ppc7455
#   INTEGRITY_TOOLCHAIN_ROOT  MULTI installation holding ccppc/cxppc
#   INTEGRITY_VERSION         default 5.0.8 (also derived from an intNNN root)
#
# Results:
#   Integrity_FOUND, Integrity_VERSION
#   Integrity_INCLUDE_DIRS, Integrity_LIBRARY_DIRS, Integrity_LIBRARIES
#   Integrity_BSP_INCLUDE_DIRS, Integrity_BSP_SOURCES
#   Integrity_LINKER_SCRIPT
#   Integrity_COMPILE_DEFINITIONS, Integrity_COMPILE_OPTIONS
#   Integrity_C_COMPILER, Integrity_CXX_COMPILER, Integrity_ASSEMBLER,
#   Integrity_LINKER, Integrity_BUILD_TOOL
#   Integrity::Runtime  (imported INTERFACE target; project mode only)
#
# The BSP sources are exposed for the product's image build (the .gpj/.bld that
# links kernel + BSP + tasks). They are not compiled into adl_platform: libbsp
# belongs to the kernel, and the loader is a task or an RTP.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

set(INTEGRITY_ROOT "" CACHE PATH "INTEGRITY 5.x installation (the vendor's int5xx directory)")
set(INTEGRITY_BSP_ROOT "" CACHE PATH "INTEGRITY board package (bsp/, target/, rsimp2a/)")
set(INTEGRITY_BSP "rsimp2a" CACHE STRING "The board: rsimp2a or riu5554")
set(INTEGRITY_TARGET_CPU "ppc7455" CACHE STRING "The PowerPC core the BSP is built for")
set(INTEGRITY_TOOLCHAIN_ROOT "" CACHE PATH "Green Hills MULTI installation holding ccppc/cxppc")
set(INTEGRITY_VERSION "5.0.8" CACHE STRING "INTEGRITY version")

set(_integrity_root_hints
    "${INTEGRITY_ROOT}"
    "$ENV{INTEGRITY_ROOT}"
    "$ENV{GHS_ROOT}/int508"
    "C:/ghs/int508"
    "C:/GHS/int508")

set(_integrity_bsp_hints
    "${INTEGRITY_BSP_ROOT}"
    "$ENV{INTEGRITY_BSP_ROOT}"
    "C:/Repos/common/integrity/int508")

set(_integrity_toolchain_hints
    "${INTEGRITY_TOOLCHAIN_ROOT}"
    "$ENV{INTEGRITY_TOOLCHAIN_ROOT}"
    "$ENV{GHS_ROOT}"
    "C:/ghs"
    "C:/GHS")

# The headers are the installation's, never the board package's (which has no
# INTEGRITY.h). NO_DEFAULT_PATH keeps a host's /usr/include out of it.
find_path(Integrity_INCLUDE_DIR
    NAMES INTEGRITY.h
    HINTS ${_integrity_root_hints}
    PATH_SUFFIXES include INTEGRITY-include
    NO_DEFAULT_PATH
    DOC "The INTEGRITY installation's include directory")

find_program(Integrity_C_COMPILER
    NAMES ccppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin
    DOC "The Green Hills PowerPC C compiler")
find_program(Integrity_CXX_COMPILER
    NAMES cxppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin
    DOC "The Green Hills PowerPC C++ compiler")
find_program(Integrity_ASSEMBLER
    NAMES asppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin
    DOC "The Green Hills PowerPC assembler")
find_program(Integrity_LINKER
    NAMES lnppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin
    DOC "The Green Hills PowerPC linker")
find_program(Integrity_BUILD_TOOL
    NAMES gbuild
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin
    DOC "gbuild, which builds an INTEGRITY image from a .gpj")

# Which C++20/23 standard headers the located toolchain lacks, and the shims
# that stand in for them (libs/platform/ghs/std/); the shim directory goes
# first on the include path so it shadows the missing header.
include(GhsStdShims)
ghs_configure_std_shims(Integrity)

# Libraries: the board package's prebuilt runtime comes first, the installation
# second. NO_DEFAULT_PATH so a host libc is never mistaken for the target's.
# The INTEGRITY runtime is a shared object even on a Windows host, which the
# host's default suffix list does not include.
set(_integrity_lib_hints
    "${INTEGRITY_BSP_ROOT}/${INTEGRITY_BSP}"
    "${INTEGRITY_BSP_ROOT}"
    "${INTEGRITY_ROOT}/lib"
    "${INTEGRITY_ROOT}")
set(CMAKE_FIND_LIBRARY_SUFFIXES_TMP "${CMAKE_FIND_LIBRARY_SUFFIXES}")
if(CMAKE_FIND_LIBRARY_SUFFIXES_TMP)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES_TMP ".so")
else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES_TMP ".so" ".a" ".lib")
endif()
set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES_TMP}")
unset(CMAKE_FIND_LIBRARY_SUFFIXES_TMP)

foreach(_name INTEGRITY posix socket net c)
    string(TOUPPER "${_name}" _upper)
    find_library(Integrity_LIBRARY_${_upper}
        NAMES ${_name}
        HINTS ${_integrity_lib_hints}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
    if(Integrity_LIBRARY_${_upper})
        list(APPEND Integrity_LIBRARIES "${Integrity_LIBRARY_${_upper}}")
    endif()
endforeach()

# The board package, when it is there. Its absence is not an error: a product
# may install only the runtime.
if(INTEGRITY_BSP_ROOT)
    if(EXISTS "${INTEGRITY_BSP_ROOT}/bsp")
        list(APPEND Integrity_BSP_INCLUDE_DIRS "${INTEGRITY_BSP_ROOT}/bsp")
        file(GLOB_RECURSE Integrity_BSP_SOURCES
            "${INTEGRITY_BSP_ROOT}/bsp/*.c"
            "${INTEGRITY_BSP_ROOT}/bsp/*.ppc")
    endif()
    find_file(Integrity_LINKER_SCRIPT
        NAMES default.ld default_256MBFlash.ld
        HINTS "${INTEGRITY_BSP_ROOT}"
        PATH_SUFFIXES "${INTEGRITY_BSP}" "target/ppc/${INTEGRITY_BSP}"
        NO_DEFAULT_PATH)
endif()

# The definitions the vendor's own build files pass. __ppc and __ppc7455 are
# the compiler's; the rest are the board's and the OS's.
set(Integrity_COMPILE_DEFINITIONS __INTEGRITY)
if(INTEGRITY_BSP STREQUAL "rsimp2a")
    list(APPEND Integrity_COMPILE_DEFINITIONS __rsimp2a__)
elseif(INTEGRITY_BSP STREQUAL "riu5554")
    list(APPEND Integrity_COMPILE_DEFINITIONS __riu5554__)
endif()
set(Integrity_COMPILE_OPTIONS "-cpu=${INTEGRITY_TARGET_CPU}")

# int508 -> 5.0.8, when the caller did not say. The captures are saved before
# the second regex: any REGEX operation rewrites CMAKE_MATCH_<n>.
if(INTEGRITY_ROOT MATCHES "int([0-9])([0-9][0-9])$")
    set(_integrity_major "${CMAKE_MATCH_1}")
    set(_integrity_minor "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "^0" "" _integrity_minor "${_integrity_minor}")
    set(Integrity_VERSION "${_integrity_major}.0.${_integrity_minor}")
    unset(_integrity_major)
    unset(_integrity_minor)
endif()

set(Integrity_INCLUDE_DIRS "")
foreach(_shim_dir IN LISTS Integrity_STD_SHIM_DIRS)
    list(APPEND Integrity_INCLUDE_DIRS "${_shim_dir}")
endforeach()
if(Integrity_INCLUDE_DIR)
    list(APPEND Integrity_INCLUDE_DIRS "${Integrity_INCLUDE_DIR}")
endif()
set(Integrity_LIBRARY_DIRS "${INTEGRITY_BSP_ROOT}/${INTEGRITY_BSP}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Integrity
    REQUIRED_VARS Integrity_INCLUDE_DIR Integrity_CXX_COMPILER
    VERSION_VAR Integrity_VERSION
    REASON_FAILURE_MESSAGE "Set INTEGRITY_ROOT to the INTEGRITY 5.x installation (the directory holding include/INTEGRITY.h) and INTEGRITY_TOOLCHAIN_ROOT to the Green Hills MULTI installation, or put ccppc/cxppc on PATH.")

# Imported targets cannot be created in cmake -P (script) mode, which is how
# tests/platform/check_fragments.cmake includes every fragment.
if(Integrity_FOUND AND NOT CMAKE_SCRIPT_MODE_FILE AND NOT TARGET Integrity::Runtime)
    add_library(Integrity::Runtime INTERFACE IMPORTED)
    set_target_properties(Integrity::Runtime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Integrity_INCLUDE_DIRS}"
        INTERFACE_COMPILE_DEFINITIONS "${Integrity_COMPILE_DEFINITIONS}"
        INTERFACE_COMPILE_OPTIONS "${Integrity_COMPILE_OPTIONS}"
        INTERFACE_LINK_LIBRARIES "${Integrity_LIBRARIES}")
endif()

mark_as_advanced(
    Integrity_INCLUDE_DIR
    Integrity_LIBRARY_INTEGRITY
    Integrity_LIBRARY_POSIX
    Integrity_LIBRARY_SOCKET
    Integrity_LIBRARY_NET
    Integrity_LIBRARY_C
    Integrity_LINKER_SCRIPT)
