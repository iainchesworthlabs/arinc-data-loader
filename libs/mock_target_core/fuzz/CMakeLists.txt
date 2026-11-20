# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for the command line of adl_mock (docs/fuzzing.md). Built only with
# ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake), and added by apps/mock/CMakeLists.txt, since the parser it
# fuzzes is the program's. It is kept here because the fuzzing workflows look for the harnesses and
# their seeds in libs/*/fuzz. It has a seed corpus and a directory of past crashes, like the others, and
# apps/mock's tests (tests/mock_cli) replay the seeds under the ordinary compilers too.
adl_add_fuzz_target(parse_options adl::mock_cli)
