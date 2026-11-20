#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Tests of the package check (tools/release/check_package_contents.py).

    python3 -X utf8 -I tests/licence/test_package_check.py --tool <adl_licence_owner>

A package that holds the owner's tool, a signing library or a key file fails the check, and a clean one passes,
as a .zip, as a .tar.gz and as an unpacked folder. The built tool is used as the bytes of the file that must be
found even when it is renamed. Nothing here makes a key: the key-like files are text made up for the test.
CTest runs it as `licence.package_check` (label system).
"""

from __future__ import annotations

import base64
import io
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "release" / "check_package_contents.py"
TOOL: Path

# RFC 8032, section 7.1, test 1: the private half of the throwaway test key.
TEST_SEED = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")

# What a clean package holds: the licence files, a program, the headers of the development package and some
# binary data that is not anything of ours.
CLEAN = {
    "arinc-data-loader/LICENSE.txt": b"The GNU General Public License ... " * 40,
    "arinc-data-loader/NOTICES.txt": b"Qt is used under the LGPL ... " * 40,
    "arinc-data-loader/bin/adl_write_payloads.exe": bytes(range(256)) * 400,
    "arinc-data-loader/bin/adl_app.exe": bytes((i * 7 + 3) % 256 for i in range(100_000)),
    "arinc-data-loader/include/adl/arinc665/crc.hpp": b"#pragma once\n",
    "arinc-data-loader/include/adl/platform/host.hpp": b"#pragma once\n",
    "arinc-data-loader/lib/libadl_arinc665.a": b"!<arch>\n" + bytes(1000),
    "arinc-data-loader/lib/cmake/adl/adlConfig.cmake": b"# adl\n",
    "arinc-data-loader/share/doc/arinc-data-loader/README.txt": b"A key id looks like lic-2026-a.\n",
    # A short text that is not a key file: it has spaces in it.
    "arinc-data-loader/share/doc/arinc-data-loader/note.txt": b"this note has spaces so it is not a key file!\n",
}


def run_check(*args: str | Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, "-X", "utf8", "-I", str(CHECK), *map(str, args)], capture_output=True,
                          text=True, encoding="utf-8", check=False)


class PackageCheck(unittest.TestCase):
    def setUp(self) -> None:
        self.folder = Path(tempfile.mkdtemp(prefix="adl-package-check-"))
        self.addCleanup(self._remove)

    def _remove(self) -> None:
        shutil.rmtree(self.folder, ignore_errors=True)

    def make(self, files: dict[str, bytes], kind: str) -> Path:
        """A package of `files` as a .zip, a .tar.gz or an unpacked folder."""
        if kind == "zip":
            path = self.folder / "arinc-data-loader-0.1.0-windows-x86_64.zip"
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
                for name, data in files.items():
                    archive.writestr(name, data)
        elif kind == "tar":
            path = self.folder / "arinc-data-loader-0.1.0-linux-x86_64.tar.gz"
            with tarfile.open(path, "w:gz") as archive:
                for name, data in files.items():
                    info = tarfile.TarInfo(name)
                    info.size = len(data)
                    archive.addfile(info, io.BytesIO(data))
        else:
            path = self.folder / "unpacked"
            for name, data in files.items():
                target = path / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        return path

    def check(self, files: dict[str, bytes], kind: str) -> subprocess.CompletedProcess[str]:
        package = self.make(files, kind)
        return run_check("--directory", package) if kind == "dir" else run_check(package)

    def assert_clean(self, files: dict[str, bytes]) -> None:
        for kind in ("zip", "tar", "dir"):
            with self.subTest(kind=kind):
                result = self.check(files, kind)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("no owner tool, signing library or key file", result.stdout)
                shutil_clean(self.folder)

    def assert_found(self, extra: dict[str, bytes], member: str, words: str) -> None:
        for kind in ("zip", "tar", "dir"):
            with self.subTest(kind=kind, member=member):
                result = self.check({**CLEAN, **extra}, kind)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(member, result.stdout)
                self.assertIn(words, result.stdout)
                shutil_clean(self.folder)

    def test_a_clean_package_passes(self) -> None:
        self.assert_clean(CLEAN)

    def test_the_tool_by_name(self) -> None:
        for name in ("adl_licence_owner.exe", "adl_licence_owner", "adl_licence_owner.pdb", "ADL_LICENCE_OWNER.EXE",
                     "adl_licence_owner_core.lib", "libadl_licence_owner_core.a"):
            self.assert_found({f"arinc-data-loader/bin/{name}": b"x"}, name, "owner's licence tool")
        # A macOS bundle: the folder is the name, and what is inside it is found as well.
        self.assert_found({"tools/adl_licence_owner.app/Contents/MacOS/anything": b"x"}, "adl_licence_owner.app",
                          "owner's licence tool")

    def test_the_signing_libraries_by_name(self) -> None:
        for name in ("libadl_licence_sign.a", "adl_licence_sign.lib", "adl_licence_test_keys.lib",
                     "libadl_licence_test_keys.a"):
            self.assert_found({f"arinc-data-loader/lib/{name}": b"x"}, name, "a library it signs with")

    def test_the_headers_of_a_signing_library(self) -> None:
        for name in ("sign.hpp", "test_keys.hpp"):
            self.assert_found({f"arinc-data-loader/include/adl/licence/{name}": b"#pragma once\n"}, name, "header")
        # The header a program verifies with is not one of them.
        self.assert_clean({**CLEAN, "arinc-data-loader/include/adl/licence/licence.hpp": b"#pragma once\n"})

    def test_a_key_file_by_name(self) -> None:
        for name in ("lic-2026-a.adlkey", "signing.key", "private.PEM", "id_ed25519", "release.p12", "x.pfx", "a.priv"):
            self.assert_found({f"arinc-data-loader/{name}": b"some text\n"}, name, "key file")
        # The public half is not secret.
        self.assert_clean({**CLEAN, "arinc-data-loader/lic-2026-a.adlpub": b'{"key_id":"lic-2026-a"}\n'})

    def test_a_key_file_under_another_name(self) -> None:
        key = base64.urlsafe_b64encode(bytes(range(7, 39))).decode().rstrip("=")
        self.assertEqual(len(key), 43)
        for data in (key.encode() + b"\n", key.encode()):
            self.assert_found({"arinc-data-loader/share/notes.txt": data}, "notes.txt", "key file made by keygen")

    def test_the_tool_under_another_name(self) -> None:
        built = TOOL.read_bytes()
        self.assertGreater(len(built), 10_000)
        self.assert_found({"arinc-data-loader/bin/helper.bin": built}, "helper.bin", "owner's tool")
        # Both the name and the test key's private half are in the program the tool is built into.
        result = self.check({**CLEAN, "arinc-data-loader/bin/helper.bin": built}, "zip")
        self.assertIn("the name of the owner's tool", result.stdout)

    def test_the_test_key_in_any_form(self) -> None:
        forms = {
            "raw": TEST_SEED,
            "hex": TEST_SEED.hex().encode(),
            "HEX": TEST_SEED.hex().upper().encode(),
            "base64": base64.b64encode(TEST_SEED),
            "base64url": base64.urlsafe_b64encode(TEST_SEED).rstrip(b"="),
        }
        for form, data in forms.items():
            with self.subTest(form=form):
                # In the middle of a large file, and across the border of two of the pieces it is read in.
                filler = bytes((i * 13 + 5) % 251 for i in range((1 << 20) - 10))
                self.assert_found({"arinc-data-loader/bin/other.dll": filler + data + b"tail"}, "other.dll",
                                  "private half of the RFC 8032 test key")

    def test_a_package_that_cannot_be_read_is_not_a_pass(self) -> None:
        broken = self.folder / "broken.zip"
        broken.write_bytes(b"this is not a zip file")
        result = run_check(broken)
        self.assertEqual(result.returncode, 2)
        self.assertIn("cannot read", result.stderr)
        self.assertEqual(run_check(self.folder / "missing.zip").returncode, 2)
        self.assertEqual(run_check(self.folder / "package.dmg").returncode, 2)
        self.assertEqual(run_check("--directory", self.folder / "missing").returncode, 2)

    def test_several_packages_are_all_checked(self) -> None:
        good = self.make(CLEAN, "zip")
        bad_zip = self.folder / "bad.zip"
        with zipfile.ZipFile(bad_zip, "w") as archive:
            archive.writestr("bin/adl_licence_owner.exe", b"x")
        result = run_check(good, bad_zip)
        self.assertEqual(result.returncode, 1)
        self.assertIn("bad.zip", result.stdout)
        self.assertIn("no owner tool, signing library or key file", result.stdout)

    def test_a_folder_and_packages_together_is_a_usage_error(self) -> None:
        self.assertEqual(run_check("--directory", self.folder, self.folder / "x.zip").returncode, 2)
        self.assertEqual(run_check().returncode, 2)


def shutil_clean(folder: Path) -> None:
    """Empties `folder` between the three forms of one package."""
    for child in folder.iterdir():
        if child.is_dir():
            shutil.rmtree(child, ignore_errors=True)
        else:
            child.unlink()


def main() -> int:
    global TOOL
    rest = sys.argv[1:]
    if len(rest) < 2 or rest[0] != "--tool":
        print("usage: test_package_check.py --tool <adl_licence_owner> [unittest options]", file=sys.stderr)
        return 2
    TOOL = Path(rest[1]).resolve()
    program = unittest.main(argv=[sys.argv[0], *rest[2:]], exit=False)
    return 0 if program.result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
