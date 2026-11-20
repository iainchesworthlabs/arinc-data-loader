#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the fuzzers' seed corpus from the golden data of the tests.

    python3 tools/fuzz/make_seeds.py

The golden headers (tests/golden/golden_payloads.hpp for ARINC 665, tests/golden/golden_615a.hpp for
ARINC 615A) hold the hexadecimal of valid files and packets that were written independently of the
library. Each becomes one binary file under libs/<lib>/fuzz/corpus/<target>/, so the fuzzers start from
inputs that get past the first checks. Run it again after the golden data changes; the files it writes
are committed. The harness of libs/loader_core reads text and not a file, and so does the one of
apps/load (the arguments of a command line, each ended by a NUL), so their seeds are written out below,
and the command lines that seed libs/platform/fuzz are in this script.
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
# Command lines for the harness of apps/load: the commands with each of their options, and what is
# refused. The arguments are joined by NUL bytes, as the harness cuts them.
TARGET = ["--target", "10.0.0.2", "--position", "HNPFMS_L"]
SEEDS_LOAD = {
    "find": ["find"],
    "find-all-options": ["find", "--seconds", "10", "--local-address", "192.168.1.5", "--json"],
    "information": ["information", *TARGET],
    "information-all-options": ["information", *TARGET, "--control-port", "30901", "--target-port", "30959", "--local-address", "127.0.0.1",
                                "--json", "--record", "out/record.json"],
    "upload-loads": ["upload", *TARGET, "--media-set", "set", "--load", "PN-1", "--load", "PN-2"],
    "upload-all-loads": ["upload", *TARGET, "--media-set", "set", "--all-loads", "--json"],
    "download-files": ["download", *TARGET, "--into", "out", "--file", "A.BIN", "--file", "B.BIN"],
    "download-pick": ["download", *TARGET, "--into", "out", "--pick", "A.BIN"],
    "download-pick-all": ["download", *TARGET, "--into", "out", "--pick-all", "--record", "r.json"],
    "help": ["--help"],
    "version": ["--version"],
    "command-help": ["upload", "--help"],
    "empty": [],
    "no-command": ["--json"],
    "unknown-command": ["fetch"],
    "unknown-option": ["find", "--bogus"],
    "equals-form": ["information", "--target=10.0.0.2"],
    "stray-word": ["information", "10.0.0.2"],
    "missing-value": ["information", "--target"],
    "value-is-option": ["information", "--target", "--position", "HNPFMS_L"],
    "empty-value": ["information", "--target", ""],
    "duplicate-option": ["information", *TARGET, "--target", "10.0.0.3"],
    "wrong-command": ["find", "--target", "10.0.0.2"],
    "bad-address": ["information", "--target", "10.0.0", "--position", "HNPFMS_L"],
    "bad-port": ["information", *TARGET, "--control-port", "65536"],
    "target-port": ["information", *TARGET, "--target-port", "30959"],
    "bad-target-port-zero": ["information", *TARGET, "--target-port", "0"],
    "bad-target-port-too-big": ["upload", *TARGET, "--media-set", "set", "--all-loads", "--target-port", "65536"],
    "target-port-for-find": ["find", "--target-port", "59"],
    "bad-seconds": ["find", "--seconds", "0"],
    "bad-position": ["information", "--target", "10.0.0.2", "--position", "A/B"],
    "position-not-for-a-folder": ["download", "--target", "10.0.0.2", "--position", "A:B", "--into", "o", "--pick-all"],
    "no-loads": ["upload", *TARGET, "--media-set", "set"],
    "loads-and-all": ["upload", *TARGET, "--media-set", "set", "--load", "PN", "--all-loads"],
    "duplicate-load": ["upload", *TARGET, "--media-set", "set", "--load", "PN", "--load", "PN"],
    "file-and-pick": ["download", *TARGET, "--into", "out", "--file", "A", "--pick", "B"],
    "no-files": ["download", *TARGET, "--into", "out"],
    "serve": ["serve"],
    "serve-all-options": ["serve", "--local-address", "127.0.0.1", "--control-port", "30901", "--max-operations", "64", "--json"],
    "bad-max-operations-zero": ["serve", "--max-operations", "0"],
    "bad-max-operations-too-big": ["serve", "--max-operations", "65"],
    "max-operations-for-find": ["find", "--max-operations", "8"],
    "serve-with-target": ["serve", "--target", "10.0.0.2"],
}

# The standard input of `adl_load serve` for the harness of apps/load (serve_request): lines of requests, with
# a tab between the fields, and what is not a request. Bytes where the text is not UTF-8.
HERE = "run\tfms-left.1\tupload\t--target\t192.168.10.21\t--position\tHNPFMS_L\t--media-set\t"
SEEDS_SERVE = {
    "run-information": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tHNPFMS_L\n",
    "run-upload": HERE + "E:\\sets\\FMS STD14\t--all-loads\t--record\tE:\\runs\\fms-left.1.json\n",
    "run-upload-loads": HERE + "sets/fms\t--load\tPN-1\t--load\tPN-2\t--target-port\t5059\n",
    "run-download-files": "run\td.2\tdownload\t--target\t10.0.0.3\t--position\tHNPFMS_R\t--into\tout\t--file\tA.BIN\t--file\tB.BIN\n",
    "run-download-pick-all": "run\td.3\tdownload\t--target\t10.0.0.4\t--position\tHNPFMS_C\t--into\tout\t--pick-all\n",
    "run-unicode-path": HERE + "wide-\u00e9-\u65e5\u672c\u8a9e-\U0001F600 and spaces\t--all-loads\n",
    "cancel": "cancel\tfms-left.1\n",
    "stop": "stop\n",
    "run-then-stop": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tHNPFMS_L\nstop\n",
    "two-runs-and-cancel": "run\ta\tinformation\t--target\t10.0.0.2\t--position\tP\nrun\tb\tinformation\t--target\t10.0.0.3\t--position\tP\ncancel\ta\n",
    "crlf-line-ends": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tHNPFMS_L\r\ncancel\ta1\r\nstop\r\n",
    "stop-with-a-field": "stop\tnow\n",
    "cancel-without-id": "cancel\n",
    "cancel-with-two-ids": "cancel\ta\tb\n",
    "run-without-id": "run\n",
    "run-without-command": "run\ta1\n",
    "run-empty-id": "run\t\tinformation\t--target\t10.0.0.2\t--position\tP\n",
    "run-id-with-a-space": "run\ta b\tinformation\t--target\t10.0.0.2\t--position\tP\n",
    "run-id-too-long": "run\t" + "x" * 65 + "\tinformation\t--target\t10.0.0.2\t--position\tP\n",
    "run-id-longest": "run\t" + "x" * 64 + "\tinformation\t--target\t10.0.0.2\t--position\tP\n",
    "run-find": "run\ta1\tfind\n",
    "run-serve": "run\ta1\tserve\n",
    "run-json": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--json\n",
    "run-local-address": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--local-address\t127.0.0.1\n",
    "run-control-port": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--control-port\t59\n",
    "run-help": "run\ta1\tinformation\t--help\n",
    "run-wrong-option": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--bogus\n",
    "run-empty-field": "run\ta1\tinformation\t\t10.0.0.2\n",
    "unknown-request": "pause\ta1\n",
    "empty-line": "\n",
    "empty-lines": "\n\n\n",
    "only-a-tab": "\t\n",
    "unfinished-last-line": "stop",
    "unfinished-after-a-line": "cancel\ta\nrun\tb\tinformation",
    "invalid-utf8": b"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\xff\xfe\n",
    "overlong-utf8": b"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xc0\x80\n",
    "surrogate-utf8": b"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xed\xa0\x80\n",
    "cut-utf8": b"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xe6\x97\n",
    "nul-in-a-line": b"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\x00Q\n",
    "line-too-long": "run\ta1\tinformation\t--target\t10.0.0.2\t--position\t" + "P" * 600 + "\nstop\n",
    "line-at-the-limit": "x" * 512 + "\r\n",
    "line-over-the-limit-by-a-return": "x" * 513 + "\r\n",
    "stop-after-too-long": "y" * 2000 + "\nstop\n",
}


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


def write_text(library: str, target: str, seeds: dict[str, str | bytes], root: str = "libs") -> int:
    out = ROOT / root / library / "fuzz" / "corpus" / target
    out.mkdir(parents=True, exist_ok=True)
    for name, text in seeds.items():
        (out / f"{name}.bin").write_bytes(text if isinstance(text, bytes) else text.encode("utf-8"))
    return len(seeds)


def main() -> int:
    total = write(ROOT / "tests/golden/golden_payloads.hpp", "arinc665", SEEDS_665)
    total += write(ROOT / "tests/golden/golden_615a.hpp", "arinc615a", SEEDS_615A)
    total += write_text("loader_core", "ipv4_address", SEEDS_IPV4)
    nul = chr(0)
    total += write_text("load", "load_options", {n: nul.join(a) for n, a in SEEDS_LOAD.items()}, root="apps")
    total += write_text("load", "serve_request", SEEDS_SERVE, root="apps")
    total += write_command_lines()
    print(f"{total} seed file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
