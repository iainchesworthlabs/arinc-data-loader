# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
# The libFuzzer harness for what the owner's tool reads that is not a licence file (docs/fuzzing.md): a public
# key file, a command line and a UTF-16 command line. A licence file, a request and a revocation list are read
# by the library, whose own harness (libs/licence/fuzz) holds them. Built only with ADL_ENABLE_FUZZING
# (cmake/Fuzzing.cmake); it has a seed corpus and a directory of past crashes.
adl_add_fuzz_target(owner adl_licence_owner_core)
