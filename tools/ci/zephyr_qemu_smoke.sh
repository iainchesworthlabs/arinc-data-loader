#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Zephyr smoke test (tests/platform/zephyr_smoke) for a Cortex-M3 board with the Zephyr
# SDK's own toolchain (GCC, picolibc, no exceptions) and runs it in QEMU. The CI job
# zephyr-qemu-cortex-m3 runs exactly this. It checks the few things that show the platform layer
# builds and runs on a real architecture and C library; the conformance suite runs on native_sim
# (tools/ci/zephyr_native_sim.sh).
#
#   ZEPHYR_BASE=<zephyr> ZEPHYR_SDK_INSTALL_DIR=<sdk> tools/ci/zephyr_qemu_smoke.sh
#
# Environment:
#   ZEPHYR_BASE             required: the Zephyr checkout of the west workspace
#   ZEPHYR_SDK_INSTALL_DIR  required: the Zephyr SDK with the arm-zephyr-eabi toolchain and the
#                           host tools (tools/ci/install_zephyr_sdk.sh)
#   ADL_BUILD_DIR           default <repo>/build/zephyr-qemu-smoke
#
# Needs west, cmake and ninja on PATH. Exits 0 only if the program reported no failure.

set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH='' cd -- "$here/../.." && pwd)

if [ -z "${ZEPHYR_BASE:-}" ] || [ ! -d "$ZEPHYR_BASE" ]; then
    echo "zephyr_qemu_smoke: set ZEPHYR_BASE to the zephyr directory of a west workspace." >&2
    exit 2
fi
if [ -z "${ZEPHYR_SDK_INSTALL_DIR:-}" ] || [ ! -d "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    echo "zephyr_qemu_smoke: set ZEPHYR_SDK_INSTALL_DIR to the Zephyr SDK (tools/ci/install_zephyr_sdk.sh)." >&2
    exit 2
fi
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
build="${ADL_BUILD_DIR:-$repo/build/zephyr-qemu-smoke}"

# The SDK's host tools have the device tree compiler, gperf and QEMU; the machine's own are used
# first when it has them, since the SDK's QEMU is linked against libraries a minimal image may lack.
tools=$(find "$ZEPHYR_SDK_INSTALL_DIR/hosttools" -type d -path '*/usr/bin' | head -n 1)
if [ -n "$tools" ]; then
    export PATH="$PATH:$tools"
fi
qemu=$(command -v qemu-system-arm || true)
if [ -z "$qemu" ]; then
    echo "zephyr_qemu_smoke: no qemu-system-arm in the SDK's host tools or on PATH." >&2
    exit 2
fi

west build -p always -b mps2/an385 -d "$build" "$repo/tests/platform/zephyr_smoke"

# The program returns to an idle kernel and QEMU does not exit, so QEMU is stopped when the
# program has reported (or after two minutes, which is a failure).
log="$build/smoke.log"
"$qemu" -cpu cortex-m3 -machine mps2-an385 -nographic -vga none -net none \
    -kernel "$build/zephyr/zephyr.elf" > "$log" 2>&1 &
qemu_pid=$!
trap 'kill "$qemu_pid" 2>/dev/null || true' EXIT
for _ in $(seq 1 240); do
    if grep -q '^SMOKE: [0-9]* failure' "$log"; then
        break
    fi
    sleep 0.5
done
kill "$qemu_pid" 2>/dev/null || true
wait "$qemu_pid" 2>/dev/null || true

cat "$log"
if grep -q '^SMOKE: 0 failure' "$log" && ! grep -q '^SMOKE: FAIL' "$log"; then
    echo "zephyr_qemu_smoke: passed"
else
    echo "zephyr_qemu_smoke: FAILED (no report, or a check failed)" >&2
    exit 1
fi
