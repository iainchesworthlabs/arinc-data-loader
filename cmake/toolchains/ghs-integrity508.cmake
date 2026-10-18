# ---------------------------------------------------------------------------
# ghs-integrity508.cmake - cross toolchain for INTEGRITY 5.0.8 on PowerPC.
#
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ghs-integrity508.cmake
#
# or the ghs508-ghs-* presets, which already name it. Configure needs either
# ccppc/cxppc on PATH or INTEGRITY_TOOLCHAIN_ROOT pointing at a MULTI install.
#
# CMake has the GHS compiler modules but no INTEGRITY platform module, so the
# system name is Generic and the target is named the way the selector expects:
# ADL_TARGET_PLATFORM=ghs508. The build makes static libraries and objects; the
# INTEGRITY image (kernel + BSP + tasks) is linked by the product's .gpj/.bld
# with gbuild, which needs the linker directives rather than a CMake link rule.
# ---------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ppc)

set(ADL_TARGET_PLATFORM ghs508 CACHE STRING "The platform this toolchain targets" FORCE)

# A try_compile that links would need the INTEGRITY linker directives and the
# runtime; compiling to an object is enough to identify the compiler.
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

# CMake's GHS module does not carry the C++ standard flags; name them here so
# that a compiler which supports the level gets it. Lower CMAKE_CXX_STANDARD
# (and drop the platform's C++23-only code behind a shim) if MULTI does not.
set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17")
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=c++20")
set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std=c++23")

set(CMAKE_C_FLAGS_INIT "-cpu=ppc7455")
set(CMAKE_CXX_FLAGS_INIT "-cpu=ppc7455")
set(CMAKE_ASM_FLAGS_INIT "-cpu=ppc7455")

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_OBJCOPY "" CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
