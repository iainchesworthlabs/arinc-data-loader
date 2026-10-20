# ---------------------------------------------------------------------------
# AdlPackage.cmake - the CPack settings, shared by every desktop platform.
#
# Included last by the top-level CMakeLists.txt, once the install rules (cmake/AdlInstall.cmake) exist.
# What differs between platforms comes from the selected platform's fragment: the generators for the
# runtime and the development components (ADL_PLATFORM_PACKAGE_*_GENERATORS) and the settings only that
# generator has (adl_platform_configure_packaging()). Nothing here asks which operating system this
# is.
#
#   cmake --preset <release preset> && cmake --build --preset <release preset>
#   cpack --config build/<preset>/CPackConfig.cmake -B build/<preset>/packages
#
# produces, in build/<preset>/packages:
#
#   arinc-data-loader-<version>-<system>-<arch>.<zip|tar.gz|dmg>       the runtime component
#   arinc-data-loader-dev-<version>-<system>-<arch>.<zip|tar.gz>       the development component
#
# <version> is ADL_RELEASE_VERSION: the project's version, or with -DADL_RELEASE_VERSION=0.2.0-rc.1 a
# pre-release's (CPack's own version stays the numeric one).
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# The machine, as a package name carries it (CMAKE_SYSTEM_PROCESSOR is AMD64 on Windows, x86_64 on
# Linux, arm64 on an Apple-silicon Mac).
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _adl_machine)
if(_adl_machine MATCHES "^(amd64|x86_64|x64)$")
    set(ADL_PACKAGE_ARCH "x86_64")
elseif(_adl_machine MATCHES "^(arm64|aarch64)$")
    set(ADL_PACKAGE_ARCH "arm64")
else()
    set(ADL_PACKAGE_ARCH "${_adl_machine}")
endif()
set(_adl_package_suffix "${ADL_RELEASE_VERSION}-${ADL_PLATFORM}-${ADL_PACKAGE_ARCH}")

set(CPACK_PACKAGE_NAME "arinc-data-loader")
set(CPACK_PACKAGE_VENDOR "iainchesworthlabs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "arinc-data-loader-${_adl_package_suffix}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ARINC Data Loader")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_VERBATIM_VARIABLES ON)
# Not CPACK_STRIP_FILES: on macOS the bundles are signed (ad hoc) by the deployment step, and
# stripping a signed binary afterwards invalidates the signature.
set(CPACK_SOURCE_GENERATOR "")

# One archive per component, named for the package it is.
set(CPACK_ARCHIVE_RUNTIME_FILE_NAME "arinc-data-loader-${_adl_package_suffix}")
set(CPACK_ARCHIVE_DEVELOPMENT_FILE_NAME "arinc-data-loader-dev-${_adl_package_suffix}")

set(CPACK_COMPONENTS_ALL runtime development)
set(CPACK_GENERATOR ${ADL_PLATFORM_PACKAGE_RUNTIME_GENERATORS} ${ADL_PLATFORM_PACKAGE_DEVELOPMENT_GENERATORS})
list(REMOVE_DUPLICATES CPACK_GENERATOR)

# CPack settings only this platform has (sets variables in this scope).
adl_platform_configure_packaging()

# Which component each generator packs is decided per generator, when CPack runs it.
configure_file("${CMAKE_CURRENT_LIST_DIR}/AdlCPackOptions.cmake.in"
    "${CMAKE_BINARY_DIR}/AdlCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/AdlCPackOptions.cmake")

include(CPack)

cpack_add_component(runtime
    DISPLAY_NAME "Applications"
    DESCRIPTION "The data loader, the media set generator and the mock target, with the Qt files they need, and the command-line tool adl_write_payloads."
    REQUIRED)
cpack_add_component(development
    DISPLAY_NAME "Development files"
    DESCRIPTION "The ARINC 665 and ARINC 615A libraries, the media tree and the platform interfaces, with their headers and the adl CMake package.")
