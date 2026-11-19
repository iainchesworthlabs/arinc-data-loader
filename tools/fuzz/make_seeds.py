#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the fuzzers' seed corpus from the golden data of the tests.

    python3 tools/fuzz/make_seeds.py

The golden headers (tests/golden/golden_payloads.hpp for ARINC 665, tests/golden/golden_615a.hpp for
ARINC 615A, tests/golden/golden_licence.hpp for licence files) hold the hexadecimal of valid files and
packets that were written independently of the library. Each becomes one binary file under
libs/<lib>/fuzz/corpus/<target>/ (tools/<tool>/fuzz/corpus/<target>/ for a tool), so the fuzzers start from
inputs that get past the first checks. Run it again after the golden data changes; the files it writes
are committed. The harness of libs/loader_core reads text and not a file, and so does the one of
apps/load (the arguments of a command line, each ended by a NUL), so their seeds are written
out below, and so are the command lines that seed libs/platform/fuzz. The description files that seed
libs/set_maker/fuzz are written out below too, and so is the golden media set as an archive of its files (the golden
set is made by tests/golden/make_golden.py, which this loads).
"""

from __future__ import annotations

import importlib.util
import re
import struct
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


def write_text(library: str, target: str, seeds: dict[str, str], root: str = "libs") -> int:
    out = ROOT / root / library / "fuzz" / "corpus" / target
    out.mkdir(parents=True, exist_ok=True)
    for name, text in seeds.items():
        (out / f"{name}.bin").write_bytes(text.encode("ascii"))
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


# The description file of adl_make_set (libs/set_maker/fuzz, target description): a good one, the smallest one, and a
# refusal of each kind, so that the fuzzer starts on both sides of every check.
_DESCRIPTION_HEAD = '{"format":1,"kind":"adl-media-set-description",'
_ONE_LOAD = '"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":"a/app.bin","name":"APP.LUP"}]}]'
SEEDS_DESCRIPTION = {
    "example": """{
  "format": 1,
  "kind": "adl-media-set-description",
  "media_set": { "part_number": "ACM-SET-0001", "members": 2, "files_check": "sha1", "files_list_check": "crc32" },
  "loads": [
    {
      "part_number": "ACM47-1234-5678",
      "header_file": "ACM47.LUH",
      "type": "OPS",
      "target_hardware_ids": ["ACME", "ACMEFMS"],
      "check": "sha1",
      "files": [
        { "source": "payloads/app.bin", "name": "APPCODE.LUP", "kind": "data", "check": "crc32" },
        { "source": "payloads/nav.bin", "name": "NAVDATA.LUP", "kind": "data", "check": "crc32" },
        { "source": "payloads/CONFIG.LDR", "kind": "support" }
      ]
    },
    {
      "part_number": "ACM4C-0100-0200",
      "type": { "description": "Test software", "id": 4 },
      "target_hardware_ids": ["ACME"],
      "files": [ { "source": "payloads/selftest.bin", "name": "SELFTEST.LUP" } ]
    }
  ],
  "batch": { "part_number": "ACM4D-0100-0300", "comment": "Both loads for the unit" }
}
""",
    "minimal": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001"},' + _ONE_LOAD + "}",
    "batch-targets": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001","members":3},'
    '"loads":[{"part_number":"ACM47-1234-5678","target_hardware_ids":["ACME_L"],"files":[{"source":"a/app.bin"}]}],'
    '"batch":{"part_number":"ACM4C-0100-0200","file_name":"BATCH.LUB","targets":'
    '[{"id":"ACME_L","loads":["ACM47-1234-5678"]}]}}',
    "type-object": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001"},'
    '"loads":[{"part_number":"ACM47-1234-5678","type":{"description":"EEC Operational Software","id":65535},'
    '"files":[{"source":"a/app.bin","name":"APP.LUP","check":"md5"}]}]}',
    "unknown-key": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001","member":2},' + _ONE_LOAD + "}",
    "format-2": '{"format":2,"kind":"adl-media-set-description"}',
    "duplicate-key": _DESCRIPTION_HEAD + '"media_set":{"part_number":"A","part_number":"B"},' + _ONE_LOAD + "}",
    "absolute-source": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001"},'
    '"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":"/etc/app.bin","name":"APP.LUP"}]}]}',
    "name-the-window-refuses": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001"},'
    '"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":"a/app.bin","name":"CON.LUP"}]}]}',
    "part-number-wrong": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM SET"},'
    '"loads":[{"part_number":"nope","files":[{"source":"a/app.bin","name":"APP.LUP"}]}]}',
    "trailing-comma": _DESCRIPTION_HEAD + '"media_set":{"part_number":"ACM-SET-0001"},' + _ONE_LOAD + ",}",
    "not-an-object": "[]",
    "empty": "",
}


# The command line of adl_make_set, one argument to a line: each way it can be right and each way it can be wrong.
SEEDS_OPTIONS = {
    "make": "make\nset.json\n--output\nout\n",
    "make-all": "make\nset.json\n--output\nout\n--source-root\nsrc\n--json\n",
    "verify": "verify\nfolder\n",
    "inspect-json": "inspect\nfolder\n--json\n",
    "options-first": "--json\nmake\n--output\nout\nset.json\n",
    "help": "--help\n",
    "version": "make\n--version\n",
    "no-command": "",
    "unknown-command": "compare\na\n",
    "needs-output": "make\nset.json\n",
    "output-for-verify": "verify\nfolder\n--output\nout\n",
    "output-needs-a-value": "make\nset.json\n--output\n--json\n",
    "given-twice": "make\nset.json\n--output\nx\n--output\ny\n",
    "two-subjects": "inspect\na\nb\n",
    "empty-subject": "inspect\n\n",
    "unknown-option": "verify\nfolder\n--quiet\n",
}


def _golden_module():
    """tests/golden/make_golden.py, the independent writer of the golden media set, loaded as a module."""
    spec = importlib.util.spec_from_file_location("make_golden", ROOT / "tests" / "golden" / "make_golden.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _set_archive(files) -> bytes:
    """The input of libs/set_maker/fuzz (target inspect_set): for each file, the member number as 16 bits, the
    length and the bytes of its pathname, of its name, and the 32-bit length and the bytes of its content."""
    out = b""
    for member, pathname, name, content in files:
        out += struct.pack(">H", member) + bytes([len(pathname)]) + pathname.encode("ascii")
        out += bytes([len(name)]) + name.encode("ascii") + struct.pack(">I", len(content)) + content
    return out


def write_set_maker() -> int:
    """The description seeds, and the golden media set (two members, a batch, a stray file) as an archive of its
    files, whole and in the ways it can be wrong."""
    base = ROOT / "libs" / "set_maker" / "fuzz" / "corpus"
    description = base / "description"
    description.mkdir(parents=True, exist_ok=True)
    for name, text in SEEDS_DESCRIPTION.items():
        (description / f"{name}.bin").write_bytes(text.encode("utf-8"))

    golden = _golden_module()
    built = golden.media_set()
    root = "\\" + golden.LOAD_PN.replace("-", "") + "\\"
    whole = [
        (1, "\\", "LOADS.LUM", built["loads"][0]),
        (1, "\\", "BATCHES.LUM", built["batches"][0]),
        (1, "\\", "FILES.LUM", built["files"][0]),
        (1, root, golden.HEADER_NAME, built["header"]),
        (1, root, "APP.LUP", golden.DATA1),
        (1, root, "CONFIG.LDR", golden.CONFIG),
        (1, "\\", "NON_LOAD.CRC", golden.NON_LOAD),
        (2, "\\", "LOADS.LUM", built["loads"][1]),
        (2, "\\", "BATCHES.LUM", built["batches"][1]),
        (2, "\\", "FILES.LUM", built["files"][1]),
        (2, "\\", "ACM47.LUB", built["batch"]),
        (2, root + "disk001\\", "DATA.LUP", golden.DATA2),
    ]
    changed = [(m, p, n, c if n != "APP.LUP" else bytes([c[0] ^ 1]) + c[1:]) for m, p, n, c in whole]
    seeds = {
        "two-member-set": _set_archive(whole),
        "member-2-missing": _set_archive([f for f in whole if f[0] == 1]),
        "lists-only": _set_archive([f for f in whole if f[2].endswith(".LUM")]),
        "one-byte-changed": _set_archive(changed),
        "no-files": b"",
    }
    options = base / "options"
    options.mkdir(parents=True, exist_ok=True)
    for name, text in SEEDS_OPTIONS.items():
        (options / f"{name}.bin").write_bytes(text.encode("utf-8"))
    out = base / "inspect_set"
    out.mkdir(parents=True, exist_ok=True)
    for name, data in seeds.items():
        (out / f"{name}.bin").write_bytes(data)
    return len(SEEDS_DESCRIPTION) + len(SEEDS_OPTIONS) + len(seeds)


# Scenario files for libs/scenario_base/fuzz (target envelope), which reads its input as the file: a good one for each
# trigger, the shapes of the two kinds, and the ways a file is refused (the strict JSON reader's and the reader's own).
SEEDS_SCENARIO = {
    "mock_full": """{
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "Slow start, lost status files, then an abort",
  "description": "The first request waits 2 s. Half the status files from the 2nd on are lost, 3 times at most.",
  "seed": 20261120,
  "steps": [
    { "when": { "request": 1 }, "do": { "action": "wait", "seconds": 2 } },
    { "comment": "only the second upload request", "when": { "request": 2, "operation": "upload" },
      "do": { "action": "delay", "min_ms": 200, "max_ms": 900 } },
    { "when": { "status_file": 2, "every": 1, "times": 3 }, "per_mille": 500, "do": { "action": "drop_status_file" } },
    { "when": { "after_ms": 20000, "every": 10 }, "do": { "action": "abort" } }
  ]
}
""",
    "empty_steps": '{"kind":"adl-mock-scenario","format":1,"steps":[]}',
    "loader_style": """{"kind":"adl-loader-scenario","format":1,"seed":7,"steps":[
  {"when":{"request":3},"on":{"datagram":"data"},"count":2,"do":{"action":"drop"}}]}""",
    "non_ascii_name": r'{"kind":"adl-mock-scenario","format":1,"name":"Abbruch nach fünf Sekunden – 😀","steps":[]}',
    # The same as JSON escapes (a surrogate pair among them) and not as the characters themselves.
    "escaped_name": '{"kind":"adl-mock-scenario","format":1,"name":"a' + chr(92) + 'u00fc ' + chr(92) + 'ud83d' + chr(92) + 'ude00","steps":[]}',
    "wrong_kind": '{"kind":"adl-loader-scenario","format":1,"steps":[]}',
    "unknown_kind": '{"kind":"adl-other","format":1,"steps":[]}',
    "newer_format": '{"kind":"adl-mock-scenario","format":2,"steps":[],"new_key":true}',
    "unknown_key": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"chanse":5,"do":{"action":"wait"}}]}',
    "two_triggers": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1,"after_ms":5},"do":{"action":"silent"}}]}',
    "no_trigger": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{},"do":{"action":"silent"}}]}',
    "per_mille_without_seed": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"status_file":1},"per_mille":100,"do":{"action":"abort"}}]}',
    "fraction": '{"kind":"adl-mock-scenario","format":1,"seed":1.5,"steps":[]}',
    "repeated_key": '{"kind":"adl-mock-scenario","kind":"adl-mock-scenario","format":1,"steps":[]}',
    "too_deep": '{"kind":"adl-mock-scenario","format":1,"steps":[[[[[[[[[[1]]]]]]]]]]}',
    "comment_in_file": '{"kind":"adl-mock-scenario",/* no */"format":1,"steps":[]}',
    "limits": '{"kind":"adl-mock-scenario","format":1,"seed":9007199254740991,"steps":[{"when":{"after_ms":86400000,"every":86400000,"times":1000000},"per_mille":1000,"do":{"action":"silent"}}]}',
}


def write_scenarios() -> int:
    out = ROOT / "libs" / "scenario_base" / "fuzz" / "corpus" / "envelope"
    out.mkdir(parents=True, exist_ok=True)
    for name, text in SEEDS_SCENARIO.items():
        (out / (name + ".json")).write_bytes(text.encode("utf-8"))
    return len(SEEDS_SCENARIO)


# The mock target's scenario files for libs/mock_scenario/fuzz (target scenario): the example of the guide, every action
# with a trigger it goes with, the limits, and the ways a file is refused (the reader's own sentences).
SEEDS_MOCK_SCENARIO = {
    "guide_example": """{
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "Slow start, lost status files, then an abort",
  "description": "The first request waits 2 s. Uploads answer a little late. Half the status files from the 2nd on are lost, 3 times at most. 20 s into an operation the target gives up.",
  "seed": 20261120,
  "steps": [
    {
      "when": { "request": 1 },
      "do": { "action": "wait", "seconds": 2 }
    },
    {
      "comment": "only the second upload request, held back between 200 and 900 ms",
      "when": { "request": 2, "operation": "upload" },
      "do": { "action": "delay", "min_ms": 200, "max_ms": 900 }
    },
    {
      "when": { "status_file": 2, "every": 1, "times": 3 },
      "per_mille": 500,
      "do": { "action": "drop_status_file" }
    },
    {
      "when": { "after_ms": 20000 },
      "do": { "action": "abort" }
    }
  ]
}
""",
    "every_action": """{"kind":"adl-mock-scenario","format":1,"seed":3,"steps":[
  {"when":{"request":1},"do":{"action":"wait","seconds":5}},
  {"when":{"request":2},"do":{"action":"refuse","code":"0x1002"}},
  {"when":{"request":3},"do":{"action":"invalid_init_file"}},
  {"when":{"request":4},"do":{"action":"corrupt_served"}},
  {"when":{"request":5},"do":{"action":"delay","ms":250}},
  {"when":{"status_file":3},"do":{"action":"drop_status_file"}},
  {"when":{"status_file":4},"do":{"action":"abort"}},
  {"when":{"status_file":5},"do":{"action":"silent"}},
  {"when":{"status_file":6},"do":{"action":"wrong_checksum"}},
  {"when":{"after_ms":1500,"every":500,"times":2,"operation":"download_media_defined"},"per_mille":300,
   "do":{"action":"delay","min_ms":10,"max_ms":90}}]}""",
    "minimum": '{"kind":"adl-mock-scenario","format":1,"steps":[]}',
    "limits": '{"kind":"adl-mock-scenario","format":1,"seed":9007199254740991,"steps":[{"when":{"after_ms":86400000,"every":86400000,"times":1000000},"per_mille":1000,"do":{"action":"delay","min_ms":0,"max_ms":60000}}]}',
    "wrong_trigger_for_wait": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"status_file":2},"do":{"action":"wait","seconds":1}}]}',
    "wrong_trigger_for_abort": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":2},"do":{"action":"abort"}}]}',
    "unknown_action": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"do":{"action":"explode"}}]}',
    "unknown_key_in_do": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"status_file":1},"do":{"action":"silent","for_ms":5}}]}',
    "delay_without_seed": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"do":{"action":"delay","min_ms":1,"max_ms":9}}]}',
    "delay_both_forms": '{"kind":"adl-mock-scenario","format":1,"seed":1,"steps":[{"when":{"request":1},"do":{"action":"delay","ms":5,"max_ms":9}}]}',
    "refuse_code": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"do":{"action":"refuse","code":"0x1001"}}]}',
    "operation_word": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1,"operation":"download"},"do":{"action":"invalid_init_file"}}]}',
    "wait_zero": '{"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"do":{"action":"wait","seconds":0}}]}',
    "loader_kind": '{"kind":"adl-loader-scenario","format":1,"steps":[]}',
    "newer_format": '{"kind":"adl-mock-scenario","format":2,"steps":[],"new_key":true}',
    "not_json": '{"kind":"adl-mock-scenario","format":1,"steps":[],}',
}


def write_mock_scenarios() -> int:
    out = ROOT / "libs" / "mock_scenario" / "fuzz" / "corpus" / "scenario"
    out.mkdir(parents=True, exist_ok=True)
    for name, text in SEEDS_MOCK_SCENARIO.items():
        (out / (name + ".json")).write_bytes(text.encode("utf-8"))
    return len(SEEDS_MOCK_SCENARIO)


def main() -> int:
    total = write(ROOT / "tests/golden/golden_payloads.hpp", "arinc665", SEEDS_665)
    total += write(ROOT / "tests/golden/golden_615a.hpp", "arinc615a", SEEDS_615A)
    total += write_text("loader_core", "ipv4_address", SEEDS_IPV4)
    total += write(ROOT / "tests/golden/golden_licence.hpp", "licence", SEEDS_LICENCE)
    total += write_command_lines()
    total += write_owner()
    total += write_set_maker()
    total += write_scenarios()
    total += write_mock_scenarios()
    nul = chr(0)
    total += write_text("load", "load_options", {n: nul.join(a) for n, a in SEEDS_LOAD.items()}, root="apps")
    print(f"{total} seed file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
