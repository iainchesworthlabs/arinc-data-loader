# FreeRTOS. Included by cmake/Platform.cmake; the contract it fills in is
# described at the top of that file.
#
# STATUS: DESIGN ONLY. There is no FreeRTOS code in this repository. FreeRTOS is a
# kernel, not an operating system: the network stack (FreeRTOS-Plus-TCP or lwIP)
# and the file system (FAT, littlefs) are separate components that a product
# chooses, and the configuration (configMAX_PRIORITIES, static or dynamic
# allocation, tick rate) is the product's. See "FreeRTOS" in
# docs/platform-abstraction.md. A port is therefore written against a chosen
# combination and not once for FreeRTOS.
#
# FreeRTOS has no CMake system name: name it with -DADL_TARGET_PLATFORM=freertos.

set(ADL_PLATFORM_DISPLAY_NAME "FreeRTOS")
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
