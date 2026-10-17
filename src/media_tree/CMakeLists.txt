# Media sets on a disk: writes the members of a media set to a directory tree
# and reads one back. It uses std::filesystem only, which is the same on every
# platform the project targets, so it needs nothing from src/platforms; the
# ARINC 665 library stays free of file I/O and this is the layer that has it.

add_library(adl_media_tree STATIC
    src/media_tree.cpp)
add_library(adl::media_tree ALIAS adl_media_tree)

target_include_directories(adl_media_tree PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_media_tree PUBLIC cxx_std_23)
target_link_libraries(adl_media_tree
    PUBLIC
        adl::arinc665
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
