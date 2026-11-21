# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for the file this library reads (docs/fuzzing.md). Built only with ADL_ENABLE_FUZZING
# (cmake/Fuzzing.cmake); it has a seed corpus (tools/fuzz/make_seeds.py) and a directory of past crashes.
adl_add_fuzz_target(loader_scenario adl::loader_scenario)
