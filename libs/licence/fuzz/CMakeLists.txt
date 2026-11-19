# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for what this library parses (docs/fuzzing.md). Built only with ADL_ENABLE_FUZZING
# (cmake/Fuzzing.cmake); it has a seed corpus and a directory of past crashes. It links the test key, so
# that what it is given can reach the payload readers past the signature check.
adl_add_fuzz_target(licence adl::licence_test_keys)
