# ---------------------------------------------------------------------------
# GhsStdShims.cmake - the C++23-or-bust policy for the GHS platforms, and the
# escape hatch that keeps the interfaces untouched.
#
# The platform interfaces (libs/platform/interfaces/adl/platform/) are C++23
# and stay C++23. A GHS toolchain whose standard library lacks one of the five
# C++20/23 headers they include gets a shim - a same-named header that
# provides exactly what this code base uses - and that shim's directory is
# prepended to the platform's include path, ahead of the toolchain's own
# headers, which is what makes the shim shadow the missing one. No interface
# header is ever changed for a toolchain.
#
#   ghs_configure_std_shims(<prefix>)   # prefix: Integrity or Integrity178
#
# With the located compiler, tries each of <expected>, <span>, <bit>,
# <compare> and <filesystem>; for every one the toolchain lacks it requires a
# shim of that name (an error by name if absent) and appends
# libs/platform/ghs/std/<header>/ to <prefix>_STD_SHIM_DIRS, so the caller
# prepends that list to <prefix>_INCLUDE_DIRS. Each shim lives in a directory
# of its own so that a toolchain which is short ONE header sees ONE shim:
# putting every shim on the path at once would shadow headers the toolchain
# does have, which is exactly what must not happen. Where there is no compiler
# - a bare cmake -P process, which is how
# tests/platform/check_fragments.cmake includes every fragment - nothing is
# checked and no shim is selected: a fragment is a declaration, and only a
# real build compiles anything.
#
# The shims cover only the C++20/23 headers the interfaces include. Anything
# older (<string_view>, <optional>, <variant>, <chrono> ...) is assumed to be
# present in a toolchain that supports the C++23 language; the premise is in
# docs/platform-ghs.md, "C++ standard and shims", and a toolchain that breaks
# it is a new set of shims, not a change to an interface.
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

function(ghs_configure_std_shims prefix)
    set(${prefix}_STD_SHIM_DIRS "" PARENT_SCOPE)
    if(NOT CMAKE_CXX_COMPILER OR CMAKE_SCRIPT_MODE_FILE)
        return()
    endif()

    include(CheckCXXSourceCompiles)
    set(_shim_root "${CMAKE_CURRENT_LIST_DIR}/../libs/platform/ghs/std")

    foreach(_header expected span bit compare filesystem)
        string(TOUPPER "${_header}" _upper)
        check_cxx_source_compiles(
            "#include <${_header}>\nint main() { return 0; }"
            ${prefix}_HAVE_STD_${_upper})
        if(NOT ${prefix}_HAVE_STD_${_upper})
            if(NOT EXISTS "${_shim_root}/${_header}/${_header}")
                message(FATAL_ERROR
                    "The GHS toolchain's standard library lacks <${_header}>, which the "
                    "platform interfaces use, and no shim exists at "
                    "${_shim_root}/${_header}/${_header}. Add one there "
                    "(docs/platform-ghs.md, 'C++ standard and shims'); the interfaces are "
                    "not to be changed for this.")
            endif()
            set(${prefix}_STD_SHIM_DIRS
                "${${prefix}_STD_SHIM_DIRS};${_shim_root}/${_header}"
                PARENT_SCOPE)
        endif()
    endforeach()
endfunction()
