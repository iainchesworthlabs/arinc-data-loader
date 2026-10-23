# SPDX-License-Identifier: GPL-3.0-or-later
"""Records what one build leg of the release workflow built with, for the SBOM.

    python collect_build_info.py --leg <windows|linux|macos> --build-dir <dir> --packages <dir>
                                 --qt-root <dir> --out <dir>

Writes <out>/build-info-<leg>.json and copies the Qt kit's own SBOM documents (the SPDX and CycloneDX
files its installer put in <qt-root>/sbom) to <out>/qt-sbom/<leg>-<name>. Everything is read from the
files the build left, never guessed:

  - the compiler, CMake and Ninja versions (from the build's own CMake files and the tools themselves);
  - the Qt version (from the kit's CMake package);
  - the vcpkg packages that were installed (from each port's vcpkg.spdx.json);
  - the files each package archive holds (the zip, the tar.gz, or on macOS the mounted disk image).

tools/release/make_sbom.py turns these, with the packages' hashes, into the release's SBOM. Standard
library only.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path


def run(command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def compiler(build_dir):
    for path in sorted(Path(build_dir).glob("CMakeFiles/*/CMakeCXXCompiler.cmake")):
        text = path.read_text(encoding="utf-8", errors="replace")
        identifier = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', text)
        version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', text)
        if identifier and version:
            return {"id": identifier.group(1), "version": version.group(1)}
    raise SystemExit(f"no CMakeCXXCompiler.cmake under {build_dir}/CMakeFiles")


def cache_value(build_dir, name):
    for line in (Path(build_dir) / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(name + ":"):
            return line.split("=", 1)[1]
    return None


def tool_version(command):
    match = re.search(r"(\d+\.\d+(?:\.\d+)*)", run(command))
    return match.group(1) if match else "unknown"


def qt_version(qt_root):
    candidates = sorted(Path(qt_root).glob("lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"))
    for path in candidates:
        match = re.search(r'set\(PACKAGE_VERSION "([^"]+)"\)', path.read_text(encoding="utf-8"))
        if match:
            return match.group(1)
    raise SystemExit(f"no Qt6ConfigVersionImpl.cmake under {qt_root}/lib/cmake/Qt6")


def vcpkg_packages(build_dir):
    packages = []
    for path in sorted(Path(build_dir).glob("vcpkg_installed/*/share/*/vcpkg.spdx.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        for package in document.get("packages", []):
            # A port's document also describes its binary package for the triplet ("catch2:x64-linux")
            # and its upstream source ("catchorg/Catch2"); the port itself is the one that is named
            # as its directory is.
            if package.get("name") != path.parent.name:
                continue
            packages.append(
                {
                    "name": package.get("name"),
                    "version": package.get("versionInfo"),
                    "download": package.get("downloadLocation"),
                    "license": package.get("licenseConcluded") or package.get("licenseDeclared"),
                    "homepage": package.get("homepage"),
                    "document": path.name,
                    "port": path.parent.name,
                }
            )
    return packages


def list_archive(path):
    name = path.name
    if name.endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            return sorted(n for n in archive.namelist() if not n.endswith("/"))
    if name.endswith(".tar.gz"):
        with tarfile.open(path) as archive:
            return sorted(m.name for m in archive.getmembers() if not m.isdir())
    if name.endswith(".dmg"):
        with tempfile.TemporaryDirectory() as mount:
            run(["hdiutil", "attach", "-readonly", "-nobrowse", "-noverify", "-mountpoint", mount, str(path)])
            try:
                return sorted(
                    str(p.relative_to(mount)) for p in Path(mount).rglob("*") if p.is_file() or p.is_symlink()
                )
            finally:
                subprocess.run(["hdiutil", "detach", mount, "-force"], check=False, capture_output=True)
    raise SystemExit(f"do not know how to list {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--leg", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--packages", required=True)
    parser.add_argument("--qt-root", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    qt_out = out / "qt-sbom"
    qt_out.mkdir(exist_ok=True)

    qt_documents = []
    sbom_dir = Path(args.qt_root) / "sbom"
    for path in sorted(sbom_dir.glob("*")) if sbom_dir.is_dir() else []:
        if path.name.endswith((".spdx.json", ".cdx.json")):
            target = qt_out / f"{args.leg}-{path.name}"
            shutil.copyfile(path, target)
            qt_documents.append(target.name)

    archives = {}
    for path in sorted(Path(args.packages).iterdir()):
        if path.name.endswith((".zip", ".tar.gz", ".dmg")):
            archives[path.name] = list_archive(path)

    info = {
        "leg": args.leg,
        "compiler": compiler(args.build_dir),
        "cmake": tool_version([cache_value(args.build_dir, "CMAKE_COMMAND"), "--version"]),
        "ninja": tool_version([cache_value(args.build_dir, "CMAKE_MAKE_PROGRAM"), "--version"]),
        "build_type": cache_value(args.build_dir, "CMAKE_BUILD_TYPE"),
        "qt_version": qt_version(args.qt_root),
        "qt_documents": qt_documents,
        "vcpkg": vcpkg_packages(args.build_dir),
        "archives": archives,
    }
    (out / f"build-info-{args.leg}.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"build-info-{args.leg}.json: {len(archives)} archive(s), {len(qt_documents)} Qt document(s), "
          f"{len(info['vcpkg'])} vcpkg package(s)")


if __name__ == "__main__":
    sys.exit(main())
