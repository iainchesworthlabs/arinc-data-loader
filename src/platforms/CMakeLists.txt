# The platform library: one implementation of every interfaces/ header, for the
# platform cmake/Platform.cmake selected. Which files those are, and what they
# link against, is declared by that platform's platform.cmake; nothing here
# asks which operating system this is.
add_library(adl_platform STATIC)
add_library(adl::platform ALIAS adl_platform)

set(_adl_platform_sources "")
foreach(_source IN LISTS ADL_PLATFORM_SOURCES)
    list(APPEND _adl_platform_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
endforeach()

target_sources(adl_platform PRIVATE ${_adl_platform_sources})

# MSVC's Code Analysis takes C6101 from the Windows SDK's inline
# WSAGetIPUserMtu wrapper: the wrapper's getsockopt fills *Mtu on success, which
# the analyzer does not model. It is the only finding in SDK code this project
# does not own; disable it for the one translation unit that includes it. A
# source property has to be set in the directory that adds the target, which is
# why this is here and not in the Windows fragment.
if(MSVC AND ADL_ENABLE_MSVC_ANALYSIS)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/windows/network_windows.cpp"
        PROPERTIES COMPILE_OPTIONS /wd6101)
endif()
target_include_directories(adl_platform
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/interfaces"
    # The implementations include each other's internal headers (detail/, posix/,
    # windows/); the public interfaces above are all that users of the library see.
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(adl_platform PUBLIC cxx_std_23)
target_compile_definitions(adl_platform PRIVATE ${ADL_PLATFORM_COMPILE_DEFINITIONS})
target_link_libraries(adl_platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage
        ${ADL_PLATFORM_LINK_LIBRARIES})
