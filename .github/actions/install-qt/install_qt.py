# SPDX-License-Identifier: GPL-3.0-or-later
"""Installs the prebuilt Qt kit (6.x, desktop) from download.qt.io, checking every archive against
SHA-256 values pinned in this repository before unpacking it, or writes those pins.

    python install_qt.py install <version> <windows|linux|mac> <output directory> --pins <file>
    python install_qt.py lock    <version> <windows|linux|mac>... --pins <file>

Why it exists: aqtinstall 3.3.0 knows the layout of Qt's online repository up to Qt 6.10 and cannot
read Windows from Qt 6.11 on ("Failed to locate XML data"); and aqtinstall, like the Qt installer
itself, checks an archive only against a hash fetched from the same server, so whoever can replace the
archive can replace the hash. The only thing aqtinstall did for a Qt 6 desktop kit beyond unpacking it
(a qt.conf beside the tools) is done here, and the kit is unpacked into the layout it gave,
<output>/<version>/<msvc2022_64|gcc_64|macos>.

What it does: read the repository's Updates.xml for the kit's one package (qtbase, qtdeclarative,
qtsvg, qttools, ... and the platform's extras), download each of its archives, refuse any archive whose
SHA-256 is not the one in the pins file (and any pins file that does not cover exactly the archives
the repository now lists), and unpack the verified archive. Updates.xml and the .sha256 files that sit
beside the archives are never trusted for anything but locating files.

`lock` is the only way a pin is made: it downloads the archives, writes their SHA-256 values and
refuses to when they disagree with the .sha256 the repository publishes (a disagreement means the
download was damaged or the repository is not consistent; look before pinning). Run it when the Qt
version changes, read the diff of the pins file, and commit it with the version bump. docs/ci.md says
how.

Needs py7zr (.github/requirements/qt.txt, installed with --require-hashes).
"""

import argparse
import hashlib
import sys
import tempfile
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ElementTree
from pathlib import Path

BASE = "https://download.qt.io/online/qtsdkrepository"

# Where each host's kit lives in the repository, how its one package is named and where aqtinstall
# put the unpacked kit. "{short}" is the version without dots (6.12.0 -> 6120). Windows has had one
# directory per architecture since Qt 6.11; Linux and macOS keep a single one.
HOSTS = {
    "windows": {
        "repository": "windows_x86",
        "directory": "qt6_{short}/qt6_{short}_msvc2022_64",
        "package": "qt.qt6.{short}.win64_msvc2022_64",
        "kit": "msvc2022_64",
    },
    "linux": {
        "repository": "linux_x64",
        "directory": "qt6_{short}/qt6_{short}",
        "package": "qt.qt6.{short}.linux_gcc_64",
        "kit": "gcc_64",
    },
    "mac": {
        "repository": "mac_x64",
        "directory": "qt6_{short}/qt6_{short}",
        "package": "qt.qt6.{short}.clang_64",
        "kit": "macos",
    },
}

# ICU's archive holds the libraries without a lib/ in their paths, and the two Windows runtime files
# (Direct3D's shader compiler and Mesa's software OpenGL) without a bin/; the Qt installer puts them
# there and windeployqt looks for them there. Every other archive holds the kit's own tree.
UNPACK_INTO = {"icu-": "lib", "d3dcompiler_47": "bin", "opengl32sw": "bin"}


def fetch(url, attempts=4):
    last = None
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=180) as response:
                return response.read()
        except OSError as error:  # URLError and timeouts are OSErrors
            last = error
            time.sleep(2 * (attempt + 1))
    raise SystemExit(f"could not download {url}: {last}")


def describe(version, host):
    """The repository's settings for the host and the archives (name, url) it lists."""
    settings = HOSTS[host]
    short = version.replace(".", "")
    directory = f"{BASE}/{settings['repository']}/desktop/{settings['directory'].format(short=short)}/"
    package_name = settings["package"].format(short=short)
    updates = ElementTree.fromstring(fetch(directory + "Updates.xml"))
    package = next(
        (p for p in updates.iter("PackageUpdate") if p.findtext("Name") == package_name), None
    )
    if package is None:
        raise SystemExit(f"{package_name} is not in {directory}Updates.xml")
    package_version = package.findtext("Version")
    names = [a.strip() for a in package.findtext("DownloadableArchives").split(",") if a.strip()]
    archives = [
        (name, directory + urllib.parse.quote(f"{package_name}/{package_version}{name}"))
        for name in names
    ]
    return settings, archives


def pin_key(version, host, name):
    return f"{version}/{HOSTS[host]['repository']}/{name}"


def read_pins(path):
    pins = {}
    for number, line in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        digest, _, key = line.partition("  ")
        if len(digest) != 64 or not key:
            raise SystemExit(f"{path}:{number}: expected '<sha256>  <version>/<repository>/<archive>'")
        pins[key] = digest.lower()
    return pins


def install(version, host, output, pins_path):
    pins = read_pins(pins_path)
    settings, archives = describe(version, host)
    wanted = {pin_key(version, host, name) for name, _ in archives}
    pinned = {key for key in pins if key.startswith(f"{version}/{settings['repository']}/")}
    if wanted != pinned:
        raise SystemExit(
            f"the pins in {pins_path} do not cover the archives Qt {version} has for {host}: "
            f"unpinned {sorted(wanted - pinned)}, pinned but not listed {sorted(pinned - wanted)}. "
            "Run `install_qt.py lock` and review the change."
        )

    import py7zr  # imported late so that a missing wheel is told apart from a bad argument

    kit = Path(output) / version / settings["kit"]
    kit.mkdir(parents=True, exist_ok=True)
    scratch_root = Path(output)
    with tempfile.TemporaryDirectory(dir=scratch_root) as scratch:
        for name, url in archives:
            print(f"downloading {name}", flush=True)
            data = fetch(url)
            actual = hashlib.sha256(data).hexdigest()
            expected = pins[pin_key(version, host, name)]
            if actual != expected:
                raise SystemExit(
                    f"{name}: SHA-256 {actual} is not the pinned {expected}. Nothing was unpacked "
                    "from it. If Qt re-published the archive on purpose, run `install_qt.py lock`, "
                    "read the change and commit it."
                )
            path = Path(scratch) / name
            path.write_bytes(data)
            target = kit
            for prefix, directory in UNPACK_INTO.items():
                if name.startswith(prefix):
                    target = kit / directory
            target.mkdir(parents=True, exist_ok=True)
            with py7zr.SevenZipFile(path) as unpacked:
                unpacked.extractall(path=target)
            path.unlink()
    # What aqtinstall wrote for a Qt 6 desktop kit: the tools in bin find the kit from here.
    (kit / "bin").mkdir(exist_ok=True)
    (kit / "bin" / "qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
    print(f"Qt {version} ({host}) installed in {kit}")


def lock(version, hosts, pins_path):
    pins_file = Path(pins_path)
    pins = read_pins(pins_file) if pins_file.exists() else {}
    for host in hosts:
        settings, archives = describe(version, host)
        prefix = f"{version}/{settings['repository']}/"
        pins = {key: digest for key, digest in pins.items() if not key.startswith(prefix)}
        for name, url in archives:
            print(f"locking {host}: {name}", flush=True)
            data = fetch(url)
            digest = hashlib.sha256(data).hexdigest()
            published = fetch(url + ".sha256").decode().split()[0].lower()
            if digest != published:
                raise SystemExit(
                    f"{name}: downloaded SHA-256 {digest} is not the {published} the repository "
                    "publishes; not pinning it."
                )
            pins[pin_key(version, host, name)] = digest
    lines = [
        "# SHA-256 of every Qt archive the CI installs, checked by .github/actions/install-qt/install_qt.py",
        "# before the archive is unpacked. Made by `install_qt.py lock`; review a change like a dependency",
        "# update. <sha256>  <qt version>/<repository directory>/<archive>",
    ]
    lines += [f"{digest}  {key}" for key, digest in sorted(pins.items())]
    pins_file.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"{pins_file}: {len(pins)} pin(s)")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    install_parser = sub.add_parser("install")
    install_parser.add_argument("version")
    install_parser.add_argument("host", choices=sorted(HOSTS))
    install_parser.add_argument("output")
    install_parser.add_argument("--pins", required=True)
    lock_parser = sub.add_parser("lock")
    lock_parser.add_argument("version")
    lock_parser.add_argument("hosts", nargs="+", choices=sorted(HOSTS))
    lock_parser.add_argument("--pins", required=True)
    args = parser.parse_args()
    if args.command == "install":
        install(args.version, args.host, args.output, args.pins)
    else:
        lock(args.version, args.hosts, args.pins)


if __name__ == "__main__":
    sys.exit(main())
