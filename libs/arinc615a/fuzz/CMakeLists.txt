# SPDX-License-Identifier: GPL-3.0-or-later
# libFuzzer harnesses for what this library parses (docs/fuzzing.md). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); each has a seed corpus and a directory of past crashes.
foreach(name tftp_packet find initialization_file configuration_list configuration_status upload_request upload_status download_request download_status download_list download_answer)
    adl_add_fuzz_target(${name} adl::arinc615a)
endforeach()
