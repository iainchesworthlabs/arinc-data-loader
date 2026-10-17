# Zephyr. Included by cmake/Platform.cmake; the contract it fills in is described
# at the top of that file.
#
# STATUS: DESIGN ONLY. There is no Zephyr code in this repository. Zephyr builds
# a product as one image with its own CMake package (find_package(Zephyr)), Kconfig
# and devicetree, so a port is most likely a Zephyr module that supplies the
# sources below to that build rather than a library this project's CMake links.
# See "Zephyr" in docs/platform-abstraction.md.
#
# Zephyr has no CMake system name here: name it with -DADL_TARGET_PLATFORM=zephyr.

set(ADL_PLATFORM_DISPLAY_NAME "Zephyr")
set(ADL_PLATFORM_DESKTOP OFF)
set(ADL_PLATFORM_STATUS design-only)

set(ADL_PLATFORM_QT_SEARCH_ROOTS "")
set(ADL_PLATFORM_QT_ARCH_DIRS "")

set(ADL_PLATFORM_SOURCES "")
set(ADL_PLATFORM_LINK_LIBRARIES "")
set(ADL_PLATFORM_COMPILE_DEFINITIONS "")

function(adl_platform_configure_app target)
endfunction()

function(adl_platform_deploy_qt_runtime target qml_dir)
endfunction()
