#!/usr/bin/env bash
#
# Installs the Zephyr SDK's Arm toolchain and host tools (dtc, gperf, QEMU) for Linux x86-64 into a
# directory, by downloading the release files of tools/ci/zephyr_sdk.sha256 straight from
# github.com/zephyrproject-rtos/sdk-ng/releases/download and checking each against the SHA-256 pinned
# there. It does what `west sdk install --gnu-toolchains arm-zephyr-eabi` does, without asking the
# GitHub releases API which files exist: that API is rate limited per address when it is not
# authenticated, and a shared CI runner's address is used up (the installer fails with 403), while
# a release download is not an API call.
#
#   tools/ci/install_zephyr_sdk.sh <install directory>
#   ZEPHYR_SDK_INSTALL_DIR=<install directory> tools/ci/zephyr_qemu_suite.sh
#
# Environment:
#   ZEPHYR_BASE   optional: when set, its SDK_VERSION file must name the pinned version
#
# Exits 0 when the directory holds the SDK (it is left alone if it already does), 1 on a download or
# checksum failure, 2 on a bad invocation.

set -euo pipefail

here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
# Without carriage returns, which a checkout with CRLF line endings would add.
sums=$(mktemp)
tr -d '\r' < "$here/zephyr_sdk.sha256" > "$sums"

if [ $# -ne 1 ]; then
    echo "usage: install_zephyr_sdk.sh <install directory>" >&2
    exit 2
fi
dir=$1

minimal=$(awk '!/^#/ && $2 ~ /_minimal\.tar\.xz$/ { print $2 }' "$sums")
toolchain=$(awk '!/^#/ && $2 ~ /^toolchain_gnu_linux-x86_64_arm-zephyr-eabi\.tar\.xz$/ { print $2 }' "$sums")
hosttools=$(awk '!/^#/ && $2 ~ /^hosttools_linux-x86_64\.tar\.xz$/ { print $2 }' "$sums")
version=$(printf '%s' "$minimal" | sed -n 's/^zephyr-sdk-\(.*\)_linux-x86_64_minimal\.tar\.xz$/\1/p')
if [ -z "$minimal" ] || [ -z "$toolchain" ] || [ -z "$hosttools" ] || [ -z "$version" ]; then
    echo "install_zephyr_sdk: tools/ci/zephyr_sdk.sha256 does not name the three files." >&2
    exit 2
fi

if [ -n "${ZEPHYR_BASE:-}" ] && [ -f "$ZEPHYR_BASE/SDK_VERSION" ]; then
    wanted=$(tr -d '[:space:]' < "$ZEPHYR_BASE/SDK_VERSION")
    if [ "$wanted" != "$version" ]; then
        echo "install_zephyr_sdk: Zephyr asks for SDK $wanted and tools/ci/zephyr_sdk.sha256 pins $version." >&2
        exit 2
    fi
fi

if [ -f "$dir/sdk_version" ] && [ -d "$dir/gnu/arm-zephyr-eabi" ] && [ -d "$dir/hosttools/sysroots" ]; then
    echo "install_zephyr_sdk: SDK $(cat "$dir/sdk_version") is already in $dir"
    exit 0
fi

base="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v$version"
work=$(mktemp -d)
trap 'rm -rf "$work" "$sums"' EXIT

for file in "$minimal" "$toolchain" "$hosttools"; do
    echo "install_zephyr_sdk: downloading $file"
    curl --fail --silent --show-error --location --retry 5 --retry-delay 5 --retry-all-errors \
        --output "$work/$file" "$base/$file"
    expected=$(awk -v f="$file" '$2 == f { print $1 }' "$sums")
    actual=$(sha256sum "$work/$file" | awk '{ print $1 }')
    if [ "$expected" != "$actual" ]; then
        echo "install_zephyr_sdk: $file has SHA-256 $actual, not the pinned $expected." >&2
        exit 1
    fi
done

rm -rf "$dir"
mkdir -p "$dir/gnu"
# The minimal SDK is a directory (zephyr-sdk-<version>) of CMake package files, the version, the list
# of toolchains and a setup script; the toolchain is a directory of its own, placed in gnu/; the
# host tools are a self-extracting installer.
tar -xf "$work/$minimal" -C "$dir" --strip-components=1
tar -xf "$work/$toolchain" -C "$dir/gnu"
mkdir "$work/hosttools"
tar -xf "$work/$hosttools" -C "$work/hosttools"
sh "$work/hosttools"/zephyr-sdk-x86_64-hosttools-standalone-*.sh -y -d "$dir/hosttools" > "$work/hosttools.log" 2>&1 \
    || { cat "$work/hosttools.log" >&2; exit 1; }

echo "install_zephyr_sdk: SDK $(cat "$dir/sdk_version") installed in $dir"
