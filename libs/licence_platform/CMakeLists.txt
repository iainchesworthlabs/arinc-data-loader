# SPDX-License-Identifier: GPL-3.0-or-later
# The licence library over this project's platform layer: the file interface the store asks of its caller,
# filled in with adl::platform::FileSystem, and the folders ARINC Data Loader keeps its licences in. This is
# the part of the licence work that belongs to this product and is under the GPL alone; libs/licence, which
# it uses, is offered under the BSD 2-Clause licence as well (docs/licensing.md).
#
# Not part of the installed adl package: nothing outside this repository builds on it.

# The part that differs between systems: os/<platform>.cmake names the source that reads an environment
# variable, and CMake picks it by the platform's name, as libs/platform does. A platform with no file there
# gets the one that reads the environment as bytes.
set(ADL_LICENCE_PLATFORM_ENVIRONMENT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/os/posix/environment.cpp")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/os/${ADL_PLATFORM}.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/os/${ADL_PLATFORM}.cmake")
endif()

add_library(adl_licence_platform STATIC
    src/files.cpp
    src/locations.cpp
    src/stamp.cpp
    "${ADL_LICENCE_PLATFORM_ENVIRONMENT_SOURCE}")
add_library(adl::licence_platform ALIAS adl_licence_platform)

target_include_directories(adl_licence_platform PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(adl_licence_platform PUBLIC cxx_std_23)
target_link_libraries(adl_licence_platform
    PUBLIC
        adl::licence
        adl::platform
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)
