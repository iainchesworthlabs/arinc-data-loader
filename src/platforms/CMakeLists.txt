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
target_include_directories(adl_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/interfaces")
target_compile_features(adl_platform PUBLIC cxx_std_23)
target_compile_definitions(adl_platform PRIVATE ${ADL_PLATFORM_COMPILE_DEFINITIONS})
target_link_libraries(adl_platform
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage
        ${ADL_PLATFORM_LINK_LIBRARIES})
