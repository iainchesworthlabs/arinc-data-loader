# Media sets on a disk: writes the members of a media set to a directory tree
# and reads one back. It uses std::filesystem only, which is the same on every
# platform the project targets, so it needs nothing from libs/platform; the
# ARINC 665 library stays free of file I/O and this is the layer that has it.

add_library(adl_media_tree STATIC
    src/media_tree.cpp)
add_library(adl::media_tree ALIAS adl_media_tree)

target_include_directories(adl_media_tree
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(adl_media_tree PUBLIC cxx_std_23)
target_link_libraries(adl_media_tree
    PUBLIC
        adl::arinc665
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::tracy>
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

# Only the top-level project installs (cmake/AdlInstall.cmake); a project of its own that builds this
# library for a target (tests/platform/freertos) has no install rules to add it to.
if(COMMAND adl_install_library)
    adl_install_library(adl_media_tree INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
