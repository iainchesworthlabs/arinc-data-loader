#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the fuzzers' seed corpus from the golden data of the tests.

    python3 tools/fuzz/make_seeds.py

The golden headers (tests/golden/golden_payloads.hpp for ARINC 665, tests/golden/golden_615a.hpp for
ARINC 615A) hold the hexadecimal of valid files and packets that were written independently of the
library. Each becomes one binary file under libs/<lib>/fuzz/corpus/<target>/, so the fuzzers start from
inputs that get past the first checks. Run it again after the golden data changes; the files it writes
are committed.
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


def constants(path: Path) -> dict[str, bytes]:
    text = path.read_text(encoding="utf-8")
    found = {}
    for m in re.finditer(r"inline constexpr std::string_view (\w+)\s*=\s*((?:\s*\"[0-9a-f]*\"\s*)+);", text):
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


def main() -> int:
    total = write(ROOT / "tests/golden/golden_payloads.hpp", "arinc665", SEEDS_665)
    total += write(ROOT / "tests/golden/golden_615a.hpp", "arinc615a", SEEDS_615A)
    print(f"{total} seed file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
