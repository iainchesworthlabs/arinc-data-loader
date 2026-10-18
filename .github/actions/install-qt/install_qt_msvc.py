"""Installs the prebuilt Qt kit for Windows (MSVC 2022, 64-bit) from download.qt.io.

Why this exists: aqtinstall 3.3.0 knows the layout of Qt's online repository up to Qt 6.10, where
one directory (qt6_6103/qt6_6103/) holds every architecture. From Qt 6.11 on, Windows has one
directory per architecture (qt6_6120/qt6_6120_msvc2022_64/) and aqtinstall cannot find the
metadata ("Failed to locate XML data"). Linux and macOS keep the old layout and use aqtinstall.
Drop this script, and use aqtinstall for Windows too, once a release of it handles the new layout.

What it does is what aqtinstall does for one package: read Updates.xml, take the package
qt.qt6.<version>.win64_msvc2022_64 (qtbase, qtdeclarative, qtsvg, qttools, ...), download each of its
archives, check each against the SHA-1 that sits beside it, and unpack it under the output
directory as <version>/msvc2022_64, the layout aqtinstall gives (the archives hold the kit's own
files, with no version in their paths).

Usage: python install_qt_msvc.py <version, e.g. 6.12.0> <output directory>
Needs py7zr, which aqtinstall brings.
"""

import hashlib
import sys
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ElementTree
from pathlib import Path

import py7zr

BASE = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop"
ARCH = "win64_msvc2022_64"


def fetch(url, attempts=3):
    last = None
    for _ in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=120) as response:
                return response.read()
        except OSError as error:  # URLError and timeouts are OSErrors
            last = error
    raise SystemExit(f"could not download {url}: {last}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    version, output = sys.argv[1], Path(sys.argv[2])
    short = version.replace(".", "")  # 6.12.0 -> 6120
    repository = f"{BASE}/qt6_{short}/qt6_{short}_msvc2022_64/"
    package_name = f"qt.qt6.{short}.{ARCH}"

    updates = ElementTree.fromstring(fetch(repository + "Updates.xml"))
    package = next(
        (p for p in updates.iter("PackageUpdate") if p.findtext("Name") == package_name), None
    )
    if package is None:
        raise SystemExit(f"{package_name} is not in {repository}Updates.xml")
    package_version = package.findtext("Version")
    archives = [a.strip() for a in package.findtext("DownloadableArchives").split(",") if a.strip()]

    kit = output / version / "msvc2022_64"
    kit.mkdir(parents=True, exist_ok=True)
    scratch = output / "_download"
    scratch.mkdir(exist_ok=True)
    for archive in archives:
        url = repository + urllib.parse.quote(f"{package_name}/{package_version}{archive}")
        print(f"downloading {archive}", flush=True)
        data = fetch(url)
        expected = fetch(url + ".sha1").decode().split()[0].lower()
        actual = hashlib.sha1(data).hexdigest()
        if actual != expected:
            raise SystemExit(f"{archive}: SHA-1 {actual} is not the published {expected}")
        path = scratch / archive
        path.write_bytes(data)
        with py7zr.SevenZipFile(path) as unpacked:
            unpacked.extractall(path=kit)
        path.unlink()
    scratch.rmdir()
    print(f"Qt {version} ({ARCH}) installed in {kit}")


if __name__ == "__main__":
    main()
