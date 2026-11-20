#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail if a package holds the owner's licence tool, a signing library or a key file.

    python3 tools/release/check_package_contents.py <package.zip|package.tar.gz>...
    python3 tools/release/check_package_contents.py --directory <unpacked package>

What signs licences stays with the owner. The tool (adl_licence_owner), the libraries it signs with
(adl_licence_sign, adl_licence_test_keys) and a private key do not belong in anything the programs are shipped
in, and the release must fail if one is there (docs/licence-owner-tool.md, docs/releasing.md).
tools/release/Test-Package.ps1 runs this on every package it unpacks, and the release workflow runs that.

What it looks at, in every file of the package:

  names    The tool, the signing library and the test key library (adl_licence_owner, adl_licence_owner_core,
           adl_licence_sign, adl_licence_test_keys, in any extension, with or without a lib prefix, or as a
           folder of a macOS bundle), their headers (adl/licence/sign.hpp, adl/licence/test_keys.hpp), and the
           names of key files: *.adlownerkey and *.adlownerpub (what keygen writes), *.adlkey (a customer's own
           site key, which another tool writes), *.key, *.pem, *.p12, *.pfx, *.priv, and the names ssh-keygen uses.
  bytes    Whatever the name: the text adl_licence_owner (in a binary renamed to something else), the private
           half of the throwaway test key of RFC 8032 (raw, in hexadecimal, in base64 and in base64url), and a
           whole file that is 43 characters of base64url and a line end, which is what a key file made by keygen
           holds.

Exit status: 0 = clean, 1 = something found (each finding is printed with the file), 2 = bad invocation or a
package that could not be read.
"""

from __future__ import annotations

import argparse
import base64
import os
import re
import sys
import tarfile
import zipfile
from collections.abc import Iterator
from pathlib import Path, PurePosixPath
from typing import IO

# A file of a package: its name, its size (-1 for a folder) and a stream of its bytes (None for a folder).
Entry = tuple[str, int, IO[bytes] | None]

# RFC 8032, section 7.1, test 1: the private half of the key the library holds for tests. It protects nothing,
# and no program may carry it.
TEST_SEED = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")

SIGNING_NAMES = {
    "adl_licence_owner",
    "adl_licence_owner_core",
    "adl_licence_owner_tests",
    "adl_licence_sign",
    "adl_licence_test_keys",
}
SIGNING_HEADERS = {("adl", "licence", "sign.hpp"), ("adl", "licence", "test_keys.hpp")}
KEY_SUFFIXES = {".adlownerkey", ".adlownerpub", ".adlkey", ".key", ".pem", ".p12", ".pfx", ".priv"}
KEY_NAMES = {"id_ed25519", "id_rsa", "id_ecdsa", "id_dsa"}

# What a file of the package may not hold, whatever it is called.
MARKERS = {
    "the name of the owner's tool": [b"adl_licence_owner", "adl_licence_owner".encode("utf-16-le")],
    "the private half of the RFC 8032 test key": [
        TEST_SEED,
        TEST_SEED.hex().encode(),
        TEST_SEED.hex().upper().encode(),
        base64.b64encode(TEST_SEED),
        base64.urlsafe_b64encode(TEST_SEED).rstrip(b"="),
    ],
}
LONGEST_MARKER = max(len(m) for ms in MARKERS.values() for m in ms)
KEY_FILE_FORM = re.compile(rb"[A-Za-z0-9_-]{43}\n?")
CHUNK = 1 << 20


def name_problems(path: str) -> list[str]:
    """What the name of a file of the package says it is."""
    parts = PurePosixPath(path.replace("\\", "/")).parts
    problems = []
    for part in parts:
        stem = part.lower()
        # adl_licence_owner.exe, libadl_licence_sign.a, adl_licence_owner.app, adl_licence_owner.pdb ...
        stem = stem.split(".", 1)[0]
        if stem.startswith("lib"):
            stem = stem[3:]
        if stem in SIGNING_NAMES:
            problems.append(f"{part} is the owner's licence tool or a library it signs with")
    lowered = tuple(p.lower() for p in parts)
    if len(lowered) >= 3 and lowered[-3:] in SIGNING_HEADERS:
        problems.append(f"{'/'.join(parts[-3:])} is the header of a signing library")
    base = lowered[-1] if lowered else ""
    suffix = os.path.splitext(base)[1]
    if suffix in KEY_SUFFIXES or base in KEY_NAMES:
        problems.append(f"{parts[-1]} is named like a key file")
    return problems


def content_problems(stream: IO[bytes], size: int) -> list[str]:
    """What the bytes of a file say it is. Reads the stream once, in pieces."""
    found: list[str] = []
    seen: set[str] = set()
    first = b""
    carry = b""
    total = 0
    while True:
        piece = stream.read(CHUNK)
        if not piece:
            break
        if total == 0:
            first = piece[:64]
        total += len(piece)
        window = carry + piece
        for label, markers in MARKERS.items():
            if label not in seen and any(marker in window for marker in markers):
                seen.add(label)
                found.append(f"it holds {label}")
        carry = window[-(LONGEST_MARKER - 1):]
    if size and total <= 44 and KEY_FILE_FORM.fullmatch(first[:total]):
        found.append("it is 43 characters of base64url, which is what a key file made by keygen holds")
    return found


def zip_files(path: Path) -> Iterator[Entry]:
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            if info.is_dir():
                yield info.filename, -1, None
                continue
            with archive.open(info) as stream:
                yield info.filename, info.file_size, stream


def tar_files(path: Path) -> Iterator[Entry]:
    with tarfile.open(path, "r:*") as archive:
        for member in archive:
            if member.isdir():
                yield member.name, -1, None
            elif member.isfile():
                stream = archive.extractfile(member)
                if stream is not None:
                    with stream:
                        yield member.name, member.size, stream


def directory_files(root: Path) -> Iterator[Entry]:
    for folder, names, files in os.walk(root):
        for name in names:
            yield str((Path(folder) / name).relative_to(root)), -1, None
        for name in files:
            full = Path(folder) / name
            if full.is_symlink():
                continue
            with full.open("rb") as stream:
                yield str(full.relative_to(root)), full.stat().st_size, stream


def check_entries(entries: Iterator[Entry]) -> list[str]:
    problems = []
    for name, size, stream in entries:
        for problem in name_problems(name):
            problems.append(f"{name}: {problem}")
        if stream is None:  # a folder
            continue
        for problem in content_problems(stream, size):
            problems.append(f"{name}: {problem}")
    return problems


def check_package(path: Path) -> list[str]:
    lowered = path.name.lower()
    if lowered.endswith(".zip"):
        return check_entries(zip_files(path))
    if lowered.endswith((".tar.gz", ".tgz", ".tar")):
        return check_entries(tar_files(path))
    raise ValueError(f"{path.name} is not a .zip or .tar.gz: unpack it and give its folder with --directory")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("packages", nargs="*", type=Path, help=".zip or .tar.gz packages")
    parser.add_argument("--directory", type=Path, help="an unpacked package (for a .dmg, which is mounted and copied)")
    args = parser.parse_args(argv)
    if bool(args.packages) == bool(args.directory):
        parser.error("give packages, or --directory, and not both")

    targets = [(p.name, p) for p in args.packages] if args.packages else [(str(args.directory), args.directory)]
    failed = False
    for label, target in targets:
        try:
            if args.directory:
                if not target.is_dir():
                    raise ValueError(f"{target} is not a folder")
                problems = check_entries(directory_files(target))
            else:
                problems = check_package(target)
        except (OSError, ValueError, zipfile.BadZipFile, tarfile.TarError) as error:
            print(f"check_package_contents: cannot read {label}: {error}", file=sys.stderr)
            return 2
        if problems:
            failed = True
            for problem in problems:
                print(f"::error::{label}: {problem}")
        else:
            print(f"{label}: no owner tool, signing library or key file")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
