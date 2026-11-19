# SPDX-License-Identifier: GPL-3.0-or-later
# libFuzzer harnesses for what this library parses (docs/fuzzing.md). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); each has a seed corpus and a directory of past crashes.
foreach(name ipv4_address)
    adl_add_fuzz_target(${name} adl::loader_core)
endforeach()
