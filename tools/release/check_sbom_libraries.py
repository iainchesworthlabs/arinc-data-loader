#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that the bill of materials names exactly the libraries the development package ships.

The bill of materials names the first-party libraries by hand (LIBRARIES in make_sbom.py), and the
development package ships the ones whose CMakeLists.txt calls adl_install_library(<target> ...). A library
installed without a line in the list would ship without being listed, and a line for a library that
nothing installs would list one that is not in the package. This compares the two.

    check_sbom_libraries.py [<repository root>]

The root defaults to the one this script is in. Exit 0: the two agree. Exit 1: they do not (each
difference is named on standard error). Exit 2: bad invocation or a file that cannot be read. Standard
library only.
"""

import ast
import re
import sys
from pathlib import Path

INSTALL_CALL = re.compile(r"\badl_install_library\(\s*([A-Za-z0-9_]+)")


def listed_libraries(make_sbom: Path) -> set[str]:
    """The names in make_sbom.py's LIBRARIES, read without running the script."""
    tree = ast.parse(make_sbom.read_text(encoding="utf-8"), filename=str(make_sbom))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "LIBRARIES" for t in node.targets):
            return {entry[0] for entry in ast.literal_eval(node.value)}
    raise ValueError(f"{make_sbom} has no LIBRARIES")


def installed_libraries(libs: Path) -> set[str]:
    """The targets a CMakeLists.txt under libs installs (comment lines are not read)."""
    names: set[str] = set()
    for lists in sorted(libs.rglob("CMakeLists.txt")):
        for line in lists.read_text(encoding="utf-8").splitlines():
            if not line.lstrip().startswith("#"):
                names.update(INSTALL_CALL.findall(line))
    return names


def main(argv: list[str]) -> int:
    if len(argv) > 2:
        print("usage: check_sbom_libraries.py [<repository root>]", file=sys.stderr)
        return 2
    root = Path(argv[1]) if len(argv) == 2 else Path(__file__).resolve().parents[2]
    make_sbom = root / "tools" / "release" / "make_sbom.py"
    libs = root / "libs"
    if not make_sbom.is_file() or not libs.is_dir():
        print(f"{root} is not the repository root (no tools/release/make_sbom.py or libs)", file=sys.stderr)
        return 2
    try:
        listed = listed_libraries(make_sbom)
        installed = installed_libraries(libs)
    except (OSError, SyntaxError, ValueError) as error:
        print(f"{error}", file=sys.stderr)
        return 2
    if not installed:
        print(f"no adl_install_library call under {libs}", file=sys.stderr)
        return 2
    for name in sorted(installed - listed):
        print(f"{make_sbom}: the library {name} is installed by the development package and is not listed "
              f"in LIBRARIES; add it, with what it depends on", file=sys.stderr)
    for name in sorted(listed - installed):
        print(f"{make_sbom}: LIBRARIES lists {name}, which no CMakeLists.txt under libs installs", file=sys.stderr)
    if listed != installed:
        return 1
    print(f"the bill of materials lists every installed library ({', '.join(sorted(installed))})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
