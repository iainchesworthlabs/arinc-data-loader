# SPDX-License-Identifier: GPL-3.0-or-later
# libFuzzer harnesses for the media set generator (docs/fuzzing.md). Built only with ADL_ENABLE_FUZZING
# (cmake/Fuzzing.cmake), from apps/make_set; each has a seed corpus and a directory of past crashes.
#
#   description  the bytes of a description file (the reader of apps/make_set)
#   inspect_set  the members of a media set (what `adl_make_set inspect` reads) and the document it writes
#   options      the command line of adl_make_set
foreach(name description inspect_set options)
    adl_add_fuzz_target(${name} adl::make_set_cli)
endforeach()
