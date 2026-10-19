#!/usr/bin/env bash
#
# Builds the Zephyr test program (tests/platform/zephyr) for the qemu_cortex_a9 board with the
# Zephyr SDK's own toolchain (GCC, picolibc, the SDK's libstdc++ with exceptions) and runs it in
# QEMU: the platform layer's conformance suite and the 615A engine tests, the same 222 test cases
# as on native_sim (tools/ci/zephyr_native_sim.sh), but on a 32-bit Arm CPU with a C library that
# is not the host's. The CI job zephyr-qemu runs exactly this.
#
#   ZEPHYR_BASE=<zephyr> ZEPHYR_SDK_INSTALL_DIR=<sdk> tools/ci/zephyr_qemu_suite.sh
#
# Environment:
#   ZEPHYR_BASE             required: the Zephyr checkout of the west workspace
#   ZEPHYR_SDK_INSTALL_DIR  required: the Zephyr SDK with the arm-zephyr-eabi toolchain and the
#                           host tools (west sdk install -t arm-zephyr-eabi). The board needs the
#                           SDK's qemu-system-xilinx-aarch64: distributions' QEMU has no machine
#                           for it.
#   ADL_BUILD_DIR           default <repo>/build/zephyr-qemu-suite
#   ADL_QEMU_TIMEOUT        seconds to wait for the report, default 1200 (a run takes a few minutes)
#
# Needs west, cmake and ninja on PATH. Exits 0 only if Catch2 reported that every test passed.

set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH='' cd -- "$here/../.." && pwd)

if [ -z "${ZEPHYR_BASE:-}" ] || [ ! -d "$ZEPHYR_BASE" ]; then
    echo "zephyr_qemu_suite: set ZEPHYR_BASE to the zephyr directory of a west workspace." >&2
    exit 2
fi
if [ -z "${ZEPHYR_SDK_INSTALL_DIR:-}" ] || [ ! -d "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    echo "zephyr_qemu_suite: set ZEPHYR_SDK_INSTALL_DIR to the Zephyr SDK (west sdk install)." >&2
    exit 2
fi
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
build="${ADL_BUILD_DIR:-$repo/build/zephyr-qemu-suite}"
limit="${ADL_QEMU_TIMEOUT:-1200}"

tools=$(find "$ZEPHYR_SDK_INSTALL_DIR/hosttools" -type d -path '*/usr/bin' | head -n 1)
if [ -n "$tools" ]; then
    export PATH="$PATH:$tools"
fi
qemu=$(command -v qemu-system-xilinx-aarch64 || true)
if [ -z "$qemu" ]; then
    echo "zephyr_qemu_suite: no qemu-system-xilinx-aarch64 in the SDK's host tools or on PATH." >&2
    exit 2
fi
dtb="$ZEPHYR_BASE/boards/qemu/cortex_a9/fdt-zynq7000s.dtb"

west build -p always -b qemu_cortex_a9 -d "$build" "$repo/tests/platform/zephyr"

# The board's console is the first UART. The program returns to an idle kernel when Catch2 is
# done and QEMU does not exit, so QEMU is stopped when the report is in (or after the time limit,
# which is a failure). Catch2 colours its output, since it cannot tell what it is writing to.
log="$build/qemu.log"
: > "$log"
start=$SECONDS
"$qemu" -machine arm-generic-fdt-7series -dtb "$dtb" -net none -nographic -serial mon:stdio \
    -serial null -device "loader,file=$build/zephyr/zephyr.elf,cpu-num=0" > "$log" 2>&1 &
qemu_pid=$!
trap 'kill "$qemu_pid" 2>/dev/null || true' EXIT
plain() { sed 's/\x1b\[[0-9;]*m//g' "$log"; }
while [ $((SECONDS - start)) -lt "$limit" ] && kill -0 "$qemu_pid" 2>/dev/null; do
    if plain | grep -Eq '^(All tests passed|test cases:)'; then
        break
    fi
    sleep 2
done
kill "$qemu_pid" 2>/dev/null || true
wait "$qemu_pid" 2>/dev/null || true

plain
echo "zephyr_qemu_suite: $((SECONDS - start)) s in QEMU"
if plain | grep -Eq '^All tests passed \(' && ! plain | grep -q 'FAILED'; then
    echo "zephyr_qemu_suite: passed"
else
    echo "zephyr_qemu_suite: FAILED (no report, or a test failed)" >&2
    exit 1
fi
