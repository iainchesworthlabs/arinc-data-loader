# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for what the platform layer parses (docs/fuzzing.md): the split of a Windows
# command line and the conversions between UTF-8 and UTF-16 (detail/command_line.hpp). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); it has a seed corpus and a directory of past crashes.
adl_add_fuzz_target(command_line adl::platform)
# The header it fuzzes is the platform layer's own (detail/), not part of its public interface.
target_include_directories(adl_fuzz_command_line PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
