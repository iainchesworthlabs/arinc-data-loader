#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the fuzzers' seed corpus from the golden data of the tests.

    python3 tools/fuzz/make_seeds.py

The golden headers (tests/golden/golden_payloads.hpp for ARINC 665, tests/golden/golden_615a.hpp for
ARINC 615A, tests/golden/golden_licence.hpp for licence files) hold the hexadecimal of valid files and
packets that were written independently of the library. Each becomes one binary file under libs/<lib>/fuzz/corpus/<target>/, so the fuzzers start from
inputs that get past the first checks. Run it again after the golden data changes; the files it writes
are committed. The command lines that seed libs/platform/fuzz are in this script.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

# golden constant name pattern -> fuzz target
SEEDS_665 = [
    (r"header_file_hex", "header_file"),
    (r"batch_file_hex", "batch_file"),
    (r"loads_list_member_\d+_hex", "loads_list"),
    (r"batches_list_member_\d+_hex", "batches_list"),
    (r"files_list_member_\d+_hex", "files_list"),
]
SEEDS_615A = [
    (r"(rrq|wrq|oack|ack|data|error)_\w*hex", "tftp_packet"),
    (r"find_\w+_hex", "find"),
    (r"initialization_\w+_hex", "initialization_file"),
    (r"configuration_list_hex", "configuration_list"),
    (r"configuration_status\w*_hex", "configuration_status"),
    (r"upload_request_hex", "upload_request"),
    (r"upload_status\w*_hex", "upload_status"),
    (r"download_request_hex", "download_request"),
    (r"download_status_hex", "download_status"),
    (r"download_list_hex", "download_list"),
    (r"download_answer_hex", "download_answer"),
]
# Licence files, a revocation list and the payloads behind the signature all go to the one harness.
SEEDS_LICENCE = [
    (r"(licence|revocations)_\w+_hex", "licence"),
]


# Command lines for libs/platform/fuzz (target command_line), which reads its input as UTF-16, low byte first:
# the quoting examples of Microsoft's documentation, a quoted program name, empty arguments, text outside the
# Western code pages, and half of a surrogate pair.
COMMAND_LINES = {
    "documented_1": 'prog "abc" d e',
    "documented_2": r'prog a\\b d"e f"g h',
    "documented_3": r'prog a\\\"b c d',
    "documented_4": r'prog a\\\\"b c" d e',
    "documented_5": 'prog a"b"" c d',
    "quoted_name": r'"C:\Program Files\adl\prog.exe" "C:\folder with spaces\\" x',
    "empty_arguments": 'prog "" "" x',
    "unicode": "prog caf\u00e9 \u65e5\u672c\u8a9e \U0001F600",
    "half_a_pair": "prog a\ud83d x \ude00",
}


def write_command_lines() -> int:
    out = ROOT / "libs" / "platform" / "fuzz" / "corpus" / "command_line"
    out.mkdir(parents=True, exist_ok=True)
    for name, line in COMMAND_LINES.items():
        (out / f"{name}.bin").write_bytes(line.encode("utf-16-le", "surrogatepass"))
    return len(COMMAND_LINES)


def constants(path: Path) -> dict[str, bytes]:
    text = path.read_text(encoding="utf-8")
    found = {}
    for m in re.finditer(r"inline constexpr std::string_view (\w+)\s*=\s*((?:\"[0-9a-f]*\"\s*)+);", text):
        digits = "".join(re.findall(r"\"([0-9a-f]*)\"", m.group(2)))
        found[m.group(1)] = bytes.fromhex(digits)
    return found


def write(golden: Path, library: str, table) -> int:
    count = 0
    for name, data in constants(golden).items():
        for pattern, target in table:
            if re.fullmatch(pattern, name):
                out = ROOT / "libs" / library / "fuzz" / "corpus" / target
                out.mkdir(parents=True, exist_ok=True)
                (out / (name.removesuffix("_hex") + ".bin")).write_bytes(data)
                count += 1
                break
        else:
            print(f"{golden.name}: {name} matches no fuzz target", file=sys.stderr)
    return count


# The owner's tool (tools/licence) reads a public key file, a command line and a UTF-16 command line, which have no
# golden data: their seeds are written here. The public key is the published test key's, from RFC 8032.
TEST_PUBLIC_KEY = "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"
SEEDS_OWNER = {
    "public_key_licence": f'{{"key_id":"lic-2026-a","purpose":"licence","public_key":"{TEST_PUBLIC_KEY}"}}\n'.encode(),
    "public_key_revocations": f'{{"key_id":"rev-2026-a","purpose":"revocations","public_key":"{TEST_PUBLIC_KEY}"}}\n'.encode(),
    "public_key_no_line_end": f'{{"key_id":"lic-2026-b","purpose":"licence","public_key":"{TEST_PUBLIC_KEY}"}}'.encode(),
    "command_line": b"--trust\nkey.adlpub\n--now\n2026-11-13T02:10:00Z\n--test\nlicence.adllic\n",
    "licence_number": b"ADL-2026-000123",
    "format_text": b"%1 needs %2 and %3",
    # U+00FC, U+1F511 (a surrogate pair in UTF-16), U+822A and U+7A7A, as little-endian UTF-16.
    "utf16_text": "".join(map(chr, (0xFC, 0x1F511, 0x822A, 0x7A7A))).encode("utf-16-le"),
}


def write_owner() -> int:
    out = ROOT / "tools" / "licence" / "fuzz" / "corpus" / "owner"
    out.mkdir(parents=True, exist_ok=True)
    for name, data in SEEDS_OWNER.items():
        (out / (name + ".bin")).write_bytes(data)
    return len(SEEDS_OWNER)


def main() -> int:
    total = write(ROOT / "tests/golden/golden_payloads.hpp", "arinc665", SEEDS_665)
    total += write(ROOT / "tests/golden/golden_615a.hpp", "arinc615a", SEEDS_615A)
    total += write(ROOT / "tests/golden/golden_licence.hpp", "licence", SEEDS_LICENCE)
    total += write_command_lines()
    total += write_owner()
    print(f"{total} seed file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
