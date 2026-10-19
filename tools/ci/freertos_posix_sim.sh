#!/usr/bin/env bash
#
# Builds the FreeRTOS test program (tests/platform/freertos) for the kernel's POSIX simulator
# port and runs it: the platform layer's conformance suite and the 615A engine tests, in a
# FreeRTOS task over lwIP's UDP sockets and littlefs. The CI job freertos-posix-sim runs exactly
# this; so can you, on any Linux with cmake, ninja and a C++23 compiler. FreeRTOS, lwIP,
# littlefs and Catch2 are fetched at the versions and hashes tests/platform/freertos/
# CMakeLists.txt pins.
#
#   tools/ci/freertos_posix_sim.sh
#
# Environment:
#   ADL_BUILD_DIR   default <repo>/build/freertos-posix-sim
#   CMAKE_BUILD_TYPE   default Debug
#
# Exits with ctest's status: 0 only if every test passed.

set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH='' cd -- "$here/../.." && pwd)
build="${ADL_BUILD_DIR:-$repo/build/freertos-posix-sim}"

cmake -S "$repo/tests/platform/freertos" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure
