#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""Make the test licences again, byte for byte.

    python3 tests/licence/make_test_licences.py --tool <adl_licence_owner> [--out <folder>] [--check]

The requests are in tests/licence/data/requests: each is the payload of a licence file (or, named
revocations_N.json, of a revocation list). The tool signs each one with the throwaway test key (`--test`), which
no program accepts, and writes tests/licence/data/licences/<name>.adllic (or .adlrev). Ed25519 signing is
deterministic, so the same request always gives the same bytes: a licence file is never "edited" to be made
again. Four files that are meant to be damaged are made from valid.adllic by a fixed change:

    bad_signature.adllic   one character of the signature is changed
    malformed.adllic       the file is cut short
    newer_version.adllic   the envelope says it has format 2
    unknown_key.adllic     the key id is one no program knows

With --check nothing in the repository is written: the files are made in a scratch folder and compared with the
committed ones, and the exit status is 1 if any differs, is missing or is not expected. tests/licence/CMakeLists.txt
runs that as the test `licence.test_licences_current`, and the target `licence_test_licences` runs this script
to write the files. The manifest (data/licences/manifest.txt) is written by hand: it says what state each
file is in, judged when.

Nothing here reads or writes a real key: --test needs none.
"""

from __future__ import annotations

import argparse
import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
REQUESTS = DATA / "requests"
LICENCES = DATA / "licences"

TEST_KEY_ID = '"key_id":"test-rfc8032-1"'


def sign(tool: Path, request: Path, out: Path) -> None:
    command = "revocations" if request.stem.startswith("revocations_") else "issue"
    result = subprocess.run(
        [str(tool), command, "--test", "--request", str(request), "--out", str(out)],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        raise SystemExit(f"{request.name}: the tool failed ({result.returncode}):\n{result.stdout}{result.stderr}")


def damaged(valid: str) -> dict[str, str]:
    """The files that are valid.adllic with one fixed fault each. The text is the file's, as the tool wrote it."""
    marker = '"signature":"'
    at = valid.index(marker) + len(marker) + 10
    swap = "A" if valid[at] != "A" else "B"
    return {
        "bad_signature.adllic": valid[:at] + swap + valid[at + 1 :],
        "malformed.adllic": valid[:100],
        "newer_version.adllic": valid.replace('"format":1', '"format":2', 1),
        "unknown_key.adllic": valid.replace(TEST_KEY_ID, '"key_id":"lic-unknown-a"', 1),
    }


def make(tool: Path, folder: Path) -> list[str]:
    """Writes every test licence into `folder`. Returns the names."""
    folder.mkdir(parents=True, exist_ok=True)
    names = []
    for request in sorted(REQUESTS.glob("*.json")):
        suffix = ".adlrev" if request.stem.startswith("revocations_") else ".adllic"
        out = folder / (request.stem + suffix)
        sign(tool, request, out)
        names.append(out.name)
    valid = (folder / "valid.adllic").read_bytes().decode("utf-8")
    for name, text in damaged(valid).items():
        # newline="" so that the text is written as it is: the files carry LF whatever system makes them.
        with (folder / name).open("w", encoding="utf-8", newline="") as file:
            file.write(text)
        names.append(name)
    return sorted(names)


def check(tool: Path) -> int:
    problems = []
    with tempfile.TemporaryDirectory(prefix="adl-test-licences-") as scratch:
        made = make(tool, Path(scratch))
        committed = sorted(p.name for p in LICENCES.iterdir() if p.suffix in (".adllic", ".adlrev"))
        for name in sorted(set(made) | set(committed)):
            if name not in committed:
                problems.append(f"{name} is made by the script and is not in {LICENCES}")
            elif name not in made:
                problems.append(f"{name} is in {LICENCES} and the script does not make it")
            elif not filecmp.cmp(Path(scratch) / name, LICENCES / name, shallow=False):
                problems.append(f"{name} differs from what the script makes")
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print("Make them again with: cmake --build <build folder> --target licence_test_licences", file=sys.stderr)
        return 1
    print(f"{len(made)} test licences are what the tool makes from the requests")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tool", required=True, type=Path, help="the built adl_licence_owner")
    parser.add_argument("--out", type=Path, default=LICENCES, help="where to write them (default: the repository's)")
    parser.add_argument("--check", action="store_true", help="compare with the committed files and write nothing")
    args = parser.parse_args()
    if not args.tool.is_file():
        print(f"no such program: {args.tool}", file=sys.stderr)
        return 2
    if args.check:
        return check(args.tool)
    names = make(args.tool, args.out)
    print(f"wrote {len(names)} test licences to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
