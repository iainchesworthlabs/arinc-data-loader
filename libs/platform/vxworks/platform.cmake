# Wind River VxWorks. Included by cmake/Platform.cmake; the contract it fills in
# is described at the top of that file.
#
# STATUS: DESIGN ONLY. There is no VxWorks code in this repository: no source
# file, no toolchain file, no build. The platform layer is shaped so that one can
# be added here (see "VxWorks" in docs/platform-abstraction.md for how each
# interface maps onto VxWorks, and for what is not known about the 178C profile).
# This fragment exists so that the selector knows the name, the contract test
# holds it to the contract, and a build for it fails with an explanation and not
# with a missing file.
#
# A port adds, to ADL_PLATFORM_SOURCES, one implementation of every header in
# libs/platform/interfaces/adl/platform/, and changes the status.

set(ADL_PLATFORM_DISPLAY_NAME "VxWorks")
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
