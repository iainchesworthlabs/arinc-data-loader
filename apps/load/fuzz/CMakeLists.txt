# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for the command line of adl_load (docs/fuzzing.md). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); it has a seed corpus and a directory of past crashes. The
# seeds are also replayed under the ordinary compilers, by tests/load/options_seeds_test.cpp.
foreach(name load_options serve_request)
    adl_add_fuzz_target(${name} adl_load_cli)
endforeach()
