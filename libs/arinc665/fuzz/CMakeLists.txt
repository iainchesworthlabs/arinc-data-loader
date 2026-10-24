# SPDX-License-Identifier: GPL-3.0-or-later
# libFuzzer harnesses for what this library parses (docs/fuzzing.md). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); each has a seed corpus and a directory of past crashes.
foreach(name header_file batch_file loads_list files_list batches_list)
    adl_add_fuzz_target(${name} adl::arinc665)
endforeach()
