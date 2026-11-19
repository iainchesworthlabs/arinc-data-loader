#!/bin/bash -eu
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the harnesses of libs/*/fuzz and apps/*/fuzz for ClusterFuzzLite, inside the image of ./Dockerfile. `compile`
# runs this with CC, CXX, CFLAGS and CXXFLAGS already carrying the sanitizer and coverage flags. The
# libraries have no vcpkg dependencies, so the configure leaves the triplet out and needs no vcpkg.
cd "$SRC/arinc-data-loader"

cmake -S . -B "$WORK/build" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
    -DADL_ENABLE_FUZZING=ON \
    -DADL_FUZZING_SANITIZERS= \
    -DADL_BUILD_TESTS=OFF \
    -DADL_BUILD_APP=OFF
cmake --build "$WORK/build" --parallel "$(nproc)"

# Each harness, with its seeds and past crashes as the starting corpus. The harnesses are in libs/*/fuzz
# and apps/*/fuzz; a pattern that matches nothing (a harness is in one of the two) is left out, not given
# to zip as it is written.
shopt -s nullglob
for harness in "$WORK"/build/bin/adl_fuzz_*; do
    name="$(basename "$harness")"
    target="${name#adl_fuzz_}"
    cp "$harness" "$OUT/$name"
    zip -q -j -r "$OUT/${name}_seed_corpus.zip" \
        {libs,apps}/*/fuzz/corpus/"$target" {libs,apps}/*/fuzz/regressions/"$target"
done
