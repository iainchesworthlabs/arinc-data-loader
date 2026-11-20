# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for the mock target's scenario file (docs/fuzzing.md). Built only with ADL_ENABLE_FUZZING
# (cmake/Fuzzing.cmake); it has a seed corpus and a directory of past crashes, and the tests (tests/mock_scenario)
# replay the seeds under the ordinary compilers too.
adl_add_fuzz_target(scenario adl::mock_scenario)
