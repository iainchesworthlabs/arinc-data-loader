#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the fuzzers' seed corpus from the golden data of the tests.

    python3 tools/fuzz/make_seeds.py

The golden headers (tests/golden/golden_payloads.hpp for ARINC 665, tests/golden/golden_615a.hpp for
ARINC 615A, tests/golden/golden_licence.hpp for licence files) hold the hexadecimal of valid files and
packets that were written independently of the library. Each becomes one binary file under
libs/<lib>/fuzz/corpus/<target>/ (tools/<tool>/fuzz/corpus/<target>/ for a tool), so the fuzzers start from
inputs that get past the first checks. Run it again after the golden data changes; the files it writes
are committed. The harness of libs/loader_core reads text and not a file, so its seeds are written
out below, and so are the scenario files that seed libs/loader_scenario/fuzz and the command lines that seed
libs/platform/fuzz.
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
# Text for the harness of libs/loader_core: addresses as an operator types them, good and not.
SEEDS_IPV4 = {
    "typical": "10.0.0.2",
    "all-ones": "255.255.255.255",
    "zeros": "0.0.0.0",
    "leading-zeros": "010.001.000.002",
    "too-few-parts": "10.0.0",
    "too-many-parts": "10.0.0.2.3",
    "empty-part": "10..0.2",
    "trailing-dot": "10.0.0.",
    "out-of-range": "256.0.0.1",
    "four-digits": "1000.0.0.1",
    "space-after": "10.0.0.2 ",
    "not-a-number": "a.b.c.d",
}

# Licence files, a revocation list, a clock file and the payloads behind the signature all go to the one harness.
SEEDS_LICENCE = [
    (r"(licence|revocations|clock)_\w+_hex", "licence"),
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


def write_text(library: str, target: str, seeds: dict[str, str]) -> int:
    out = ROOT / "libs" / library / "fuzz" / "corpus" / target
    out.mkdir(parents=True, exist_ok=True)
    for name, text in seeds.items():
        (out / f"{name}.bin").write_bytes(text.encode("ascii"))
    return len(seeds)


# Scenario files for the harness of libs/loader_scenario: the guide's example (read from the test that holds it), a
# step for each action, and files that are refused.
_HEAD = '{ "kind": "adl-loader-scenario", "format": 1, "seed": 7, "steps": [ '
_REQUEST = '{ "when": { "request": 1 }, '
SEEDS_LOADER_SCENARIO = {
    "no_steps": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [] }',
    "drop": _HEAD + _REQUEST + '"do": { "action": "drop" } } ] }',
    "duplicate": _HEAD + _REQUEST + '"do": { "action": "duplicate", "copies": 4 }, "count": "all" } ] }',
    "delay": _HEAD + _REQUEST + '"do": { "action": "delay", "ms": 3000 } } ] }',
    "delay_range": _HEAD + _REQUEST + '"do": { "action": "delay", "min_ms": 200, "max_ms": 900 }, "per_mille": 500 } ] }',
    "reorder": _HEAD + _REQUEST + '"do": { "action": "reorder", "within_ms": 300 } } ] }',
    "corrupt": _HEAD + _REQUEST + '"on": { "datagram": "data", "block": 1 }, "do": { "action": "corrupt", "offset": 9, "xor": 128 } } ] }',
    "check_value": _HEAD + _REQUEST + '"do": { "action": "check_value" } } ] }',
    "length": _HEAD + _REQUEST + '"do": { "action": "length", "delta": -3 } } ] }',
    "error": _HEAD + _REQUEST + '"do": { "action": "error", "code": 5, "text": "unknown transfer id" } } ] }',
    "wait": _HEAD + _REQUEST + '"do": { "action": "wait", "seconds": 12 }, "count": 3 } ] }',
    "abort_message": _HEAD + _REQUEST + '"on": { "file": ".LUS" }, "do": { "action": "abort_message", "status": "0x1004" } } ] }',
    "abort": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "status_file": 2, "operation": "upload" }, "do": { "action": "abort", "status": "0x1004" } } ] }',
    "silent": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "after_ms": 5000 }, "do": { "action": "silent" } } ] }',
    "every_times": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "request": 2, "every": 3, "times": 4 }, "do": { "action": "drop" } } ] }',
    "refused_unknown_key": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [], "chanse": 1 }',
    "refused_mock_kind": '{ "kind": "adl-mock-scenario", "format": 1, "steps": [] }',
    "refused_format": '{ "kind": "adl-loader-scenario", "format": 2, "steps": [] }',
    "refused_status": _HEAD + _REQUEST + '"do": { "action": "abort_message", "status": "1004" } } ] }',
    "refused_two_triggers": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "request": 1, "after_ms": 5 }, "do": { "action": "drop" } } ] }',
    "refused_no_seed": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "request": 1 }, "do": { "action": "drop" }, "per_mille": 5 } ] }',
    "refused_repeated_key": '{ "kind": "adl-loader-scenario", "kind": "adl-loader-scenario", "format": 1, "steps": [] }',
    "refused_comment": '{ // no' + chr(10) + ' "kind": "adl-loader-scenario", "format": 1, "steps": [] }',
    "refused_trailing_comma": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [], }',
    "refused_fraction": '{ "kind": "adl-loader-scenario", "format": 1, "seed": 1.5, "steps": [] }',
    "refused_too_deep": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [[[[[[[[[[]]]]]]]]]] }',
    "refused_abort_with_on": '{ "kind": "adl-loader-scenario", "format": 1, "steps": [ { "when": { "request": 1 }, "on": {}, "do": { "action": "abort" } } ] }',
}


def write_loader_scenario() -> int:
    out = ROOT / "libs" / "loader_scenario" / "fuzz" / "corpus" / "scenario"
    out.mkdir(parents=True, exist_ok=True)
    seeds: dict[str, bytes] = {name: text.encode("utf-8") for name, text in SEEDS_LOADER_SCENARIO.items()}
    test = (ROOT / "tests" / "loader_scenario" / "reader_test.cpp").read_text(encoding="utf-8")
    example = re.search(r'constexpr std::string_view example = R"\((.*?)\)";', test, re.DOTALL)
    if example is None:
        print("reader_test.cpp: the example of the guide was not found", file=sys.stderr)
        return 0
    seeds["example"] = example.group(1).encode("utf-8")
    seeds["refused_byte_order_mark"] = bytes.fromhex("efbbbf") + seeds["no_steps"]
    seeds["refused_not_utf8"] = b'{ "kind": "adl-loader-scenario", "format": 1, "name": "' + bytes.fromhex("ff") + b'", "steps": [] }'
    for name, data in seeds.items():
        (out / f"{name}.bin").write_bytes(data)
    return len(seeds)


# The owner's tool (tools/licence) reads a public key file, a command line and a UTF-16 command line, which have no
# golden data: their seeds are written here. The public key is the published test key's, from RFC 8032.
TEST_PUBLIC_KEY = "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"
SEEDS_OWNER = {
    "public_key_licence": f'{{"key_id":"lic-2026-a","purpose":"licence","public_key":"{TEST_PUBLIC_KEY}"}}\n'.encode(),
    "public_key_revocations": f'{{"key_id":"rev-2026-a","purpose":"revocations","public_key":"{TEST_PUBLIC_KEY}"}}\n'.encode(),
    "public_key_no_line_end": f'{{"key_id":"lic-2026-b","purpose":"licence","public_key":"{TEST_PUBLIC_KEY}"}}'.encode(),
    "command_line": b"--trust\nkey.adlownerpub\n--now\n2026-11-13T02:10:00Z\n--test\nlicence.adllic\n",
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
    total += write_text("loader_core", "ipv4_address", SEEDS_IPV4)
    total += write(ROOT / "tests/golden/golden_licence.hpp", "licence", SEEDS_LICENCE)
    total += write_loader_scenario()
    total += write_command_lines()
    total += write_owner()
    print(f"{total} seed file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
