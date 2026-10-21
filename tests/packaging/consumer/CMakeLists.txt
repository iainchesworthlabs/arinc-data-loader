# A program built the way a user of the development package builds one: find_package(adl) against an
# install tree, and nothing from the source tree. tests/packaging/check_install.cmake installs the
# development component into a scratch prefix, configures this project against it, builds it and runs it.
cmake_minimum_required(VERSION 3.28)
project(AdlInstallConsumer LANGUAGES CXX)

find_package(adl 0.1 REQUIRED)

add_executable(consumer main.cpp)
target_compile_features(consumer PRIVATE cxx_std_23)
target_link_libraries(consumer
    PRIVATE
        adl::arinc665
        adl::arinc615a
        adl::arinc615a_runner
        adl::media_tree
        adl::platform)

# The example of docs/libraries.md, built here so that it cannot drift from the installed package. It is
# built and linked, not run: it needs a media set on disk.
add_executable(check_media_set check_media_set.cpp)
target_compile_features(check_media_set PRIVATE cxx_std_23)
target_link_libraries(check_media_set PRIVATE adl::arinc665 adl::media_tree)
