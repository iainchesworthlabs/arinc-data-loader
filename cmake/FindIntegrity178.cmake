# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# FindIntegrity178.cmake - find the INTEGRITY-178B kernel/BSP tree and the GHS
# toolchain that builds for it.
#
#   find_package(Integrity178)
#
# Unlike FindIntegrity (a vendor installation with an include/ directory), the
# 178B tree on this project's machines is a source drop:
#
#   C:\Repos\common\integrity\int178B
#     INTEGRITY-include/   INTEGRITY.h, fcntl.h, ipflite/, arinc615a/
#     ansi/ eecxx/         the C library and Embedded C++ subsets
#     bsp/ kernel/ asp/    the board and kernel sources
#     rsimp2a-bae/         libINTEGRITY.a, libipflite.a, libbsp.a
#     INTEGRITY.ld         the linker map
#
# A root may also be the un-merged rtos-i40-...-bae-.../rtos directory, which
# has the same layout one level down; the module accepts both.
#
# Inputs:
#   INTEGRITY178_ROOT         the tree (merged, or the package's rtos/ directory)
#   INTEGRITY178_BSP          board, default rsimp2a-bae (sim800 also exists)
#   INTEGRITY178_TARGET_CPU   default ppc7400
#   INTEGRITY_TOOLCHAIN_ROOT  MULTI installation holding ccppc/cxppc
#
# Results mirror FindIntegrity's: Integrity178_FOUND, _VERSION, _INCLUDE_DIRS,
# _LIBRARY_DIRS, _LIBRARIES, _LINKER_SCRIPT, _BSP_INCLUDE_DIRS,
# _COMPILE_DEFINITIONS, _COMPILE_OPTIONS, _C_COMPILER, _CXX_COMPILER,
# _ASSEMBLER, _LINKER, _BUILD_TOOL, and Integrity178::Runtime.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

set(INTEGRITY178_ROOT "" CACHE PATH "The INTEGRITY-178B tree (merged, or its rtos/ directory)")
set(INTEGRITY178_BSP "rsimp2a-bae" CACHE STRING "The board: rsimp2a-bae or sim800")
set(INTEGRITY178_TARGET_CPU "ppc7400" CACHE STRING "The PowerPC core: ppc7400, ppc8540 or ppc8548")
set(INTEGRITY_TOOLCHAIN_ROOT "" CACHE PATH "Green Hills MULTI installation holding ccppc/cxppc")
set(INTEGRITY178_VERSION "178B" CACHE STRING "INTEGRITY-178B")

set(_integrity178_root_hints
    "${INTEGRITY178_ROOT}"
    "$ENV{INTEGRITY178_ROOT}"
    "$ENV{GHS_ROOT}/int178B"
    "C:/ghs/int178B"
    "C:/GHS/int178B"
    "C:/Repos/common/integrity/int178B")

set(_integrity_toolchain_hints
    "${INTEGRITY_TOOLCHAIN_ROOT}"
    "$ENV{INTEGRITY_TOOLCHAIN_ROOT}"
    "$ENV{GHS_ROOT}"
    "C:/ghs"
    "C:/GHS")

# Resolve the tree: the root itself, or a package directory under it whose
# rtos/ has the layout.
set(_integrity178_tree "")
foreach(_hint IN LISTS _integrity178_root_hints)
    if(_integrity178_tree)
        break()
    endif()
    if(_hint AND EXISTS "${_hint}/INTEGRITY-include/INTEGRITY.h")
        set(_integrity178_tree "${_hint}")
        break()
    endif()
    if(_hint)
        file(GLOB _integrity178_packages "${_hint}/rtos-*/rtos")
        foreach(_package IN LISTS _integrity178_packages)
            if(EXISTS "${_package}/INTEGRITY-include/INTEGRITY.h")
                set(_integrity178_tree "${_package}")
                break()
            endif()
        endforeach()
    endif()
endforeach()

if(_integrity178_tree)
    set(INTEGRITY178_ROOT "${_integrity178_tree}" CACHE PATH
        "The INTEGRITY-178B tree (merged, or its rtos/ directory)" FORCE)
    set(Integrity178_INCLUDE_DIR "${_integrity178_tree}/INTEGRITY-include")
endif()

set(Integrity178_BSP_INCLUDE_DIRS "${INTEGRITY178_ROOT}/bsp")
file(GLOB_RECURSE Integrity178_BSP_SOURCES
    "${INTEGRITY178_ROOT}/bsp/*.c"
    "${INTEGRITY178_ROOT}/bsp/*.ppc")

find_program(Integrity178_C_COMPILER
    NAMES ccppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin)
find_program(Integrity178_CXX_COMPILER
    NAMES cxppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin)
find_program(Integrity178_ASSEMBLER
    NAMES asppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin)
find_program(Integrity178_LINKER
    NAMES lnppc
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin)
find_program(Integrity178_BUILD_TOOL
    NAMES gbuild
    HINTS ${_integrity_toolchain_hints}
    PATH_SUFFIXES bin)

# Which C++20/23 standard headers the located toolchain lacks, and the shims
# that stand in for them; the shim directory goes first on the include path so
# it shadows the missing header.
include(GhsStdShims)
ghs_configure_std_shims(Integrity178)

# The include set the vendor's own compile lines use (the .d files show
# ../INTEGRITY-include, ../ansi, ..\asp, ..\shared, ..\bsp), behind the shims
# the toolchain is short of.
set(Integrity178_INCLUDE_DIRS "")
foreach(_shim_dir IN LISTS Integrity178_STD_SHIM_DIRS)
    list(APPEND Integrity178_INCLUDE_DIRS "${_shim_dir}")
endforeach()
list(APPEND Integrity178_INCLUDE_DIRS
    "${INTEGRITY178_ROOT}/INTEGRITY-include"
    "${INTEGRITY178_ROOT}/ansi"
    "${INTEGRITY178_ROOT}/eecxx"
    "${INTEGRITY178_ROOT}/bsp")

set(Integrity178_LIBRARY_DIRS "${INTEGRITY178_ROOT}/${INTEGRITY178_BSP}")
foreach(_name INTEGRITY ipflite ipfliteclient bsp)
    string(TOUPPER "${_name}" _upper)
    find_library(Integrity178_LIBRARY_${_upper}
        NAMES ${_name}
        HINTS "${INTEGRITY178_ROOT}/${INTEGRITY178_BSP}"
        NO_DEFAULT_PATH)
    if(Integrity178_LIBRARY_${_upper})
        list(APPEND Integrity178_LIBRARIES "${Integrity178_LIBRARY_${_upper}}")
    endif()
endforeach()

find_file(Integrity178_LINKER_SCRIPT
    NAMES INTEGRITY.ld
    HINTS "${INTEGRITY178_ROOT}"
    NO_DEFAULT_PATH)

# The build file's own defines (target/rsimp2a-bae.bld): INT_178B, CLIBS_178B,
# NO_TRACEPOINTS, plus the Embedded C++ marker the eecxx headers key on.
set(Integrity178_COMPILE_DEFINITIONS
    __INTEGRITY
    INT_178B
    CLIBS_178B
    NO_TRACEPOINTS
    EMBEDDED)
set(Integrity178_COMPILE_OPTIONS "-cpu=${INTEGRITY178_TARGET_CPU}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Integrity178
    REQUIRED_VARS INTEGRITY178_ROOT Integrity178_INCLUDE_DIR Integrity178_CXX_COMPILER
    VERSION_VAR INTEGRITY178_VERSION
    REASON_FAILURE_MESSAGE "Set INTEGRITY178_ROOT to the INTEGRITY-178B tree (the directory holding INTEGRITY-include/INTEGRITY.h, or its rtos- directory) and INTEGRITY_TOOLCHAIN_ROOT to the Green Hills MULTI installation, or put ccppc/cxppc on PATH.")

if(Integrity178_FOUND AND NOT CMAKE_SCRIPT_MODE_FILE AND NOT TARGET Integrity178::Runtime)
    add_library(Integrity178::Runtime INTERFACE IMPORTED)
    set_target_properties(Integrity178::Runtime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Integrity178_INCLUDE_DIRS}"
        INTERFACE_COMPILE_DEFINITIONS "${Integrity178_COMPILE_DEFINITIONS}"
        INTERFACE_COMPILE_OPTIONS "${Integrity178_COMPILE_OPTIONS}"
        INTERFACE_LINK_LIBRARIES "${Integrity178_LIBRARIES}")
endif()

mark_as_advanced(
    Integrity178_INCLUDE_DIR
    Integrity178_LIBRARY_INTEGRITY
    Integrity178_LIBRARY_IPFLITE
    Integrity178_LIBRARY_IPFLITECLIENT
    Integrity178_LIBRARY_BSP
    Integrity178_LINKER_SCRIPT)
