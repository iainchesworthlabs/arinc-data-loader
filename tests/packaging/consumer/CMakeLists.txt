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
