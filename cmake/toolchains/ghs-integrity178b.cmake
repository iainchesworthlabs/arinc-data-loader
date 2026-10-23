# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# ghs-integrity178b.cmake - cross toolchain for INTEGRITY-178B on PowerPC.
#
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ghs-integrity178b.cmake
#
# The board in C:\Repos\common\integrity\int178B is built by
# target/rsimp2a-bae.bld with :toolchain=ghs, :processor=ppc,
# :ppc_cputype=ppc7400, :defines=INT_178B CLIBS_178B NO_TRACEPOINTS,
# :nostdinc=true and -Xnosmallprologue. The definitions are applied by
# FindIntegrity178; this file supplies the compiler and the CPU.
#
# -nostdinc is deliberately not set: it is right for the certified C subset the
# vendor builds, but the platform layer is C++ and needs a hosted standard
# library (see docs/platform-ghs.md, "Open questions"). A product that has a
# C-only build can add it through Integrity178_COMPILE_OPTIONS.
# ---------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ppc)

set(ADL_TARGET_PLATFORM ghs178b CACHE STRING "The platform this toolchain targets" FORCE)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(INTEGRITY_TOOLCHAIN_ROOT)
    set(_ghs_bin "${INTEGRITY_TOOLCHAIN_ROOT}/bin")
else()
    set(_ghs_bin "")
endif()

if(_ghs_bin AND EXISTS "${_ghs_bin}/ccppc")
    set(CMAKE_C_COMPILER "${_ghs_bin}/ccppc" CACHE FILEPATH "Green Hills PowerPC C compiler")
    set(CMAKE_CXX_COMPILER "${_ghs_bin}/cxppc" CACHE FILEPATH "Green Hills PowerPC C++ compiler")
    set(CMAKE_ASM_COMPILER "${_ghs_bin}/asppc" CACHE FILEPATH "Green Hills PowerPC assembler")
else()
    set(CMAKE_C_COMPILER ccppc CACHE FILEPATH "Green Hills PowerPC C compiler")
    set(CMAKE_CXX_COMPILER cxppc CACHE FILEPATH "Green Hills PowerPC C++ compiler")
    set(CMAKE_ASM_COMPILER asppc CACHE FILEPATH "Green Hills PowerPC assembler")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17")
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=c++20")
set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std=c++23")

set(CMAKE_C_FLAGS_INIT "-cpu=ppc7400")
set(CMAKE_CXX_FLAGS_INIT "-cpu=ppc7400")
set(CMAKE_ASM_FLAGS_INIT "-cpu=ppc7400")

# The vendor's map carries .picbase/.pidbase and the checksum sections; the
# image link uses it. Kept here for the .gpj/.bld step rather than a CMake rule.
set(ADL_INTEGRITY_LINKER_DIRECTIVES "${INTEGRITY178_ROOT}/INTEGRITY.ld"
    CACHE FILEPATH "The INTEGRITY-178B linker directives")

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_OBJCOPY "" CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
