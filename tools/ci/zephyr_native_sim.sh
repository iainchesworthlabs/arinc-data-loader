#!/usr/bin/env bash
#
# Builds the Zephyr test program (tests/platform/zephyr) for the native_sim board and runs it:
# the platform layer's conformance suite and the 615A engine tests, on Zephyr's kernel, network
# stack and file system running as a host process. The CI job zephyr-native-sim runs exactly
# this; so can you, anywhere Zephyr's host build works (Linux, WSL). docs/building.md has the
# setup it expects.
#
#   ZEPHYR_BASE=<the zephyr directory of a west workspace> tools/ci/zephyr_native_sim.sh
#
# Environment:
#   ZEPHYR_BASE               required: the Zephyr checkout (west init / west update made it)
#   ZEPHYR_TOOLCHAIN_VARIANT  default "host": the machine's own gcc, which native_sim uses
#   ADL_BUILD_DIR             default <repo>/build/zephyr-native-sim
#   ADL_CATCH_ARGS            Catch2's arguments, e.g. "[conformance] --durations yes"
#
# Needs west, cmake, ninja, gcc/g++ 14 or newer (C++23), dtc and gperf on PATH. Exits with the
# test program's status: 0 only if every test passed.

set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH='' cd -- "$here/../.." && pwd)

if [ -z "${ZEPHYR_BASE:-}" ] || [ ! -d "$ZEPHYR_BASE" ]; then
    echo "zephyr_native_sim: set ZEPHYR_BASE to the zephyr directory of a west workspace." >&2
    exit 2
fi
export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-host}"
build="${ADL_BUILD_DIR:-$repo/build/zephyr-native-sim}"

# -p always: a build directory left by another configuration is never trusted.
west build -p always -b native_sim/native/64 -d "$build" "$repo/tests/platform/zephyr"

# The program keeps the simulated flash in a file of the working directory; a directory of its
# own makes a run start from an empty one.
run="$build/run"
rm -rf "$run"
mkdir -p "$run"
cd "$run"
timeout 900 "$build/zephyr/zephyr.exe"
