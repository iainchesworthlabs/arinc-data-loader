# SPDX-License-Identifier: GPL-3.0-or-later
# Monocypher 4.0.2, unchanged (README.md). Only the Ed25519 functions are used. The code is not ours, so it is
# built as C with its own warnings, and adl::warnings (which is for the first-party C++) is not linked.
# The language is enabled by the CMakeLists.txt that adds this directory.

add_library(adl_monocypher STATIC monocypher.c monocypher-ed25519.c)
add_library(adl::monocypher ALIAS adl_monocypher)

target_include_directories(adl_monocypher SYSTEM PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
set_target_properties(adl_monocypher PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)
