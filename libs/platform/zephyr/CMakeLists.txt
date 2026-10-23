# SPDX-License-Identifier: GPL-3.0-or-later
# The Zephyr module's build (zephyr/module.yml names this directory). Zephyr includes it
# while it configures the application's image; nothing else in this repository does.
#
# The sources are the ones platform.cmake lists, the same fragment the other platforms use,
# compiled as a Zephyr library with Zephyr's flags, include paths and CONFIG_ symbols.
# With CONFIG_ADL_LIBRARIES the platform-free libraries are built too (the loader's engine
# and the ARINC 665 model), as ordinary CMake targets linked into the image.

if(CONFIG_ADL_PLATFORM)
    set(_adl_platforms_dir "${CMAKE_CURRENT_LIST_DIR}/..")
    cmake_path(NORMAL_PATH _adl_platforms_dir)
    set(ADL_PLATFORM zephyr)
    include("${CMAKE_CURRENT_LIST_DIR}/platform.cmake")

    zephyr_library_named(adl_platform)
    add_library(adl::platform ALIAS adl_platform)

    foreach(_source IN LISTS ADL_PLATFORM_SOURCES)
        zephyr_library_sources("${_adl_platforms_dir}/${_source}")
    endforeach()

    # The warnings the other platforms are built with, except that Zephyr's headers are not
    # ours to fix: -isystem on the directories they come from keeps the warnings in them (C
    # casts in macros, statement expressions) out of the output. GCC ignores an -I that is
    # also given as -isystem, so this wins over the include path Zephyr sets. Done before
    # this module adds its own include directory, which should be warned about.
    list(PREPEND CMAKE_MODULE_PATH "${_adl_platforms_dir}/../../cmake")
    include(CompilerWarnings)
    target_link_libraries(adl_platform PRIVATE adl::warnings)
    get_target_property(_adl_zephyr_includes zephyr_interface INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_dir IN LISTS _adl_zephyr_includes)
        target_compile_options(adl_platform PRIVATE "SHELL:-isystem ${_dir}")
    endforeach()

    # Warnings stop the build only where CONFIG_ADL_WARNINGS_AS_ERRORS says so: -Wno-error comes
    # after adl::warnings' -Werror because this library is linked after it.
    add_library(adl_zephyr_tolerant INTERFACE)
    target_compile_options(adl_zephyr_tolerant INTERFACE -Wno-error)
    if(NOT CONFIG_ADL_WARNINGS_AS_ERRORS)
        target_link_libraries(adl_platform PRIVATE adl_zephyr_tolerant)
    endif()

    # The interfaces are what an application includes (adl/platform/*.hpp); the platforms
    # directory itself is for the implementations (detail/, zephyr/).
    zephyr_include_directories("${_adl_platforms_dir}/interfaces")
    target_include_directories(adl_platform PRIVATE "${_adl_platforms_dir}")

    if(CONFIG_ADL_LIBRARIES)
        set(_adl_root "${_adl_platforms_dir}/../..")
        cmake_path(NORMAL_PATH _adl_root)
        # The library targets link these four; Zephyr builds are not Debug builds and
        # take no sanitizer, coverage is a desktop concern, and profiling is off (adl::tracy
        # is then only the header that makes the instrumentation names do nothing).
        include(Sanitizers)
        include(Coverage)
        include(Tracy)
        add_subdirectory("${_adl_root}/libs/arinc665" adl_arinc665)
        add_subdirectory("${_adl_root}/libs/arinc615a" adl_arinc615a)
        add_subdirectory("${_adl_root}/libs/arinc615a_runner" adl_arinc615a_runner)
        # CMake does not know this compiler's language standards (Zephyr's toolchain
        # modules force it), so cxx_std_23 adds no flag: the libraries take the flags
        # Zephyr builds the application with, -std=c++23 among them from CONFIG_STD_CPP23.
        foreach(_lib IN ITEMS adl_arinc665 adl_arinc615a adl_arinc615a_runner)
            target_link_libraries(${_lib} PRIVATE zephyr_interface)
            if(NOT CONFIG_ADL_WARNINGS_AS_ERRORS)
                target_link_libraries(${_lib} PRIVATE adl_zephyr_tolerant)
            endif()
        endforeach()
        zephyr_append_cmake_library(adl_arinc665)
        zephyr_append_cmake_library(adl_arinc615a)
        zephyr_append_cmake_library(adl_arinc615a_runner)
    endif()
endif()
