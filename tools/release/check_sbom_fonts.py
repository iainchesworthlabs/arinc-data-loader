#!/usr/bin/env python3
"""Checks that the SPDX document lists every typeface the programs embed.

The bill of materials names the typefaces by hand (make_sbom.py), so a face added to libs/ui/fonts without
a line there would ship without being listed. This compares the families of the font files with the
packages of the document: a family is the file name before its first hyphen (Archivo-Medium.ttf is
Archivo, NotoSansHebrew-Regular.ttf is NotoSansHebrew), and a package matches when its name, with the
spaces taken out, is that family, in any case.

    check_sbom_fonts.py <spdx.json> <font directory>

Exit 0: every family is listed. Exit 1: one is not (named on standard error). Exit 2: bad invocation.
Standard library only.
"""

import json
import sys
from pathlib import Path

FONT_SUFFIXES = {".ttf", ".otf"}


def families(directory: Path) -> set[str]:
    return {path.name.split("-")[0] for path in directory.iterdir() if path.suffix.lower() in FONT_SUFFIXES}


def listed(document: dict) -> set[str]:
    return {package.get("name", "").replace(" ", "").lower() for package in document.get("packages", [])}


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: check_sbom_fonts.py <spdx.json> <font directory>", file=sys.stderr)
        return 2
    document = json.loads(Path(argv[1]).read_text(encoding="utf-8"))
    directory = Path(argv[2])
    if not directory.is_dir():
        print(f"{directory} is not a directory", file=sys.stderr)
        return 2
    wanted = families(directory)
    if not wanted:
        print(f"no font files in {directory}", file=sys.stderr)
        return 2
    present = listed(document)
    missing = sorted(family for family in wanted if family.lower() not in present)
    for family in missing:
        print(f"{argv[1]}: the typeface {family} is embedded in the programs (libs/ui/fonts) and is not "
              f"listed in the bill of materials; add it to tools/release/make_sbom.py", file=sys.stderr)
    if missing:
        return 1
    print(f"{argv[1]}: lists every embedded typeface ({', '.join(sorted(wanted))})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
