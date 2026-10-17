# ---------------------------------------------------------------------------
# PlatformContract.cmake - what a platform fragment
# (src/platforms/<name>/platform.cmake) has to declare, and the check that it
# did. The contract itself is described at the top of cmake/Platform.cmake.
#
# Shared by the selector (which checks the one fragment it includes, at
# configure time, so a platform that forgets part of the contract fails by
# name) and by tests/platform/check_fragments.cmake (which checks every
# fragment in the tree on every host, so the platforms a leg does not build
# are still held to it).
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

set(ADL_PLATFORM_CONTRACT_VARIABLES
    ADL_PLATFORM_DISPLAY_NAME
    ADL_PLATFORM_STATUS
    ADL_PLATFORM_DESKTOP
    ADL_PLATFORM_QT_SEARCH_ROOTS
    ADL_PLATFORM_QT_ARCH_DIRS
    ADL_PLATFORM_SOURCES
    ADL_PLATFORM_LINK_LIBRARIES
    ADL_PLATFORM_COMPILE_DEFINITIONS)

set(ADL_PLATFORM_CONTRACT_HOOKS
    adl_platform_configure_app
    adl_platform_deploy_qt_runtime)

# Checks the fragment that was just included (in the caller's scope):
#   - every contract variable is defined (an empty string is a definition);
#   - every hook exists;
#   - ADL_PLATFORM_DESKTOP is ON or OFF;
#   - ADL_PLATFORM_STATUS is supported, unbuilt or design-only, and a platform
#     that says design-only compiles nothing;
#   - every file the fragment compiles exists under <platforms_dir>.
# `fragment` only names it in the message.
function(adl_platform_check_contract fragment platforms_dir)
    foreach(_variable IN LISTS ADL_PLATFORM_CONTRACT_VARIABLES)
        if(NOT DEFINED ${_variable})
            message(FATAL_ERROR
                "${fragment} does not set ${_variable}. Set it to an empty "
                "string if the platform has nothing to say (the contract is "
                "described at the top of cmake/Platform.cmake).")
        endif()
    endforeach()

    foreach(_hook IN LISTS ADL_PLATFORM_CONTRACT_HOOKS)
        if(NOT COMMAND ${_hook})
            message(FATAL_ERROR
                "${fragment} does not define ${_hook}(). Define it as a "
                "no-op if the platform has nothing to do.")
        endif()
    endforeach()

    if(NOT ADL_PLATFORM_DESKTOP STREQUAL "ON" AND NOT ADL_PLATFORM_DESKTOP STREQUAL "OFF")
        message(FATAL_ERROR
            "${fragment}: ADL_PLATFORM_DESKTOP must be ON or OFF, not "
            "'${ADL_PLATFORM_DESKTOP}'.")
    endif()

    if(NOT ADL_PLATFORM_STATUS STREQUAL "supported" AND
       NOT ADL_PLATFORM_STATUS STREQUAL "unbuilt" AND
       NOT ADL_PLATFORM_STATUS STREQUAL "design-only")
        message(FATAL_ERROR
            "${fragment}: ADL_PLATFORM_STATUS must be supported, unbuilt or "
            "design-only, not '${ADL_PLATFORM_STATUS}'.")
    endif()
    if(ADL_PLATFORM_STATUS STREQUAL "design-only" AND NOT "${ADL_PLATFORM_SOURCES}" STREQUAL "")
        message(FATAL_ERROR
            "${fragment}: a design-only platform compiles nothing, but ADL_PLATFORM_SOURCES "
            "is '${ADL_PLATFORM_SOURCES}'. Say unbuilt if there is code.")
    endif()
    if(NOT ADL_PLATFORM_STATUS STREQUAL "design-only" AND "${ADL_PLATFORM_SOURCES}" STREQUAL "")
        message(FATAL_ERROR
            "${fragment}: ADL_PLATFORM_SOURCES is empty, so the platform is design-only, "
            "not '${ADL_PLATFORM_STATUS}'.")
    endif()

    foreach(_file IN LISTS ADL_PLATFORM_SOURCES)
        if(NOT EXISTS "${platforms_dir}/${_file}")
            message(FATAL_ERROR
                "${fragment} lists ${_file}, which does not exist under "
                "${platforms_dir}.")
        endif()
    endforeach()
endfunction()
