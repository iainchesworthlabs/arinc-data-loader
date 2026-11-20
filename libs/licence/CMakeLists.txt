# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
# The licence library: the signed licence file, the states a licence is in, the checks a program asks and
# the words for them. No Qt, no network and no platform code: it reads no file and no clock, so every state
# can be tested. It takes no ADL name except the target prefix and product.hpp, trusted_keys.cpp and
# revocations_release.cpp, so that it can move into a repository the two products share. Ed25519 is
# Monocypher (third_party/monocypher); no cryptography is written here.
#
#   adl_licence            verify and judge; what the programs link
#   adl_licence_sign       signing; the owner's tool and the tests only
#   adl_licence_test_keys  the throwaway RFC 8032 key; never in a product
#
# Not part of the installed adl package: nothing outside this repository builds on it.

add_library(adl_licence STATIC
    src/json.cpp
    src/licence.cpp
    src/revocations_release.cpp
    src/signed_file.cpp
    src/trusted_keys.cpp
    src/words.cpp)
add_library(adl::licence ALIAS adl_licence)

target_include_directories(adl_licence PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(adl_licence PUBLIC cxx_std_23)
target_link_libraries(adl_licence
    PRIVATE
        adl::monocypher
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

add_library(adl_licence_sign STATIC src/sign.cpp)
add_library(adl::licence_sign ALIAS adl_licence_sign)
target_link_libraries(adl_licence_sign
    PUBLIC
        adl::licence
    PRIVATE
        adl::monocypher
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

add_library(adl_licence_test_keys STATIC src/test_keys.cpp)
add_library(adl::licence_test_keys ALIAS adl_licence_test_keys)
target_link_libraries(adl_licence_test_keys
    PUBLIC
        adl::licence_sign
    PRIVATE
        $<BUILD_LOCAL_INTERFACE:adl::warnings>
        $<BUILD_LOCAL_INTERFACE:adl::sanitizers>
        $<BUILD_LOCAL_INTERFACE:adl::coverage>)

# The libFuzzer harness for what this library parses, when fuzzing is on (cmake/Fuzzing.cmake).
if(ADL_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
