# SPDX-License-Identifier: GPL-3.0-or-later
# The libFuzzer harness for the commands of adl_licence (docs/fuzzing.md): a command line and the file it names,
# through `run`, over files in memory. The files a licence is read from are read by the library, whose own harness
# (libs/licence/fuzz) holds them; this one holds what is added here: the words of a command line, a name that is
# given to the store, and what the commands print and write for any file. It links the test key, so that what it
# is given can get past the signature check. Built only with ADL_ENABLE_FUZZING (cmake/Fuzzing.cmake); it has a
# seed corpus and a directory of past crashes.
adl_add_fuzz_target(licence_cli adl::licence_cli)
target_include_directories(adl_fuzz_licence_cli PRIVATE "${PROJECT_SOURCE_DIR}/tests/licence")
target_link_libraries(adl_fuzz_licence_cli PRIVATE adl::licence_test_keys)
