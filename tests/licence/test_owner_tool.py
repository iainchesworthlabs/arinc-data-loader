#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""System tests of the owner's licence tool: the built program, run as a process.

    python3 -X utf8 -I tests/licence/test_owner_tool.py --tool <adl_licence_owner> --data tests/licence/data

Every command, every refusal, a key made and used end to end, and the manifest of test licences judged by `verify`.
Keys are made only in scratch folders that this test creates and removes; no real key exists, and the one private
key the tests may meet, the throwaway key of RFC 8032, is checked never to appear in what the tool prints.
CTest runs it as `licence.owner_tool` (label system).
"""

from __future__ import annotations

import base64
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL: Path
DATA: Path

# RFC 8032, section 7.1, test 1: the private half of the test key, which the library holds. It must never be
# printed, by the tool or by this test's own checks of the tool.
TEST_SEED = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")
TEST_SEED_TEXT = base64.urlsafe_b64encode(TEST_SEED).decode("ascii").rstrip("=")
TEST_NOTE = "NOTE: --test uses the throwaway key test-rfc8032-1"

ACTIVE_STATES = {"valid", "expiring", "grace"}


class Result:
    def __init__(self, code: int, out: str, err: str):
        self.code = code
        self.out = out
        self.err = err

    def lines(self) -> dict[str, str]:
        """The `Name: value` lines of the output; the first of each name wins."""
        found: dict[str, str] = {}
        for line in self.out.splitlines():
            name, separator, value = line.partition(": ")
            if separator and name not in found:
                found[name] = value
        return found


def run(*args: str | Path) -> Result:
    done = subprocess.run([str(TOOL), *map(str, args)], capture_output=True, check=False)
    # A Windows console program writes CR LF line ends; the tests read LF.
    result = Result(done.returncode, done.stdout.decode("utf-8").replace("\r\n", "\n"),
                    done.stderr.decode("utf-8").replace("\r\n", "\n"))
    assert TEST_SEED_TEXT not in result.out + result.err, "the tool printed the private half of the test key"
    return result


def request_text(licence_id: str = "ADL-2026-000123", product: str = "adl", extra: str = "") -> str:
    return (
        "{\n"
        f'  "product": "{product}",\n'
        f'  "licence_id": "{licence_id}",\n'
        '  "kind": "annual",\n'
        '  "edition": "pro",\n'
        '  "licensee": { "name": "Example Lab", "email": "tests@example-lab.invalid" },\n'
        '  "issued_at": "2026-11-13T02:10:00Z",\n'
        '  "starts_at": "2026-11-13T00:00:00Z",\n'
        '  "expires_at": "2027-11-13T00:00:00Z",\n'
        '  "grace_days": 28,\n'
        '  "seats": 1,\n'
        '  "features": ["support"],\n'
        '  "machines": [],\n'
        f"{extra}"
        '  "critical": []\n'
        "}\n"
    )


def in_git_work_tree(folder: Path) -> bool:
    return any((parent / ".git").exists() for parent in (folder, *folder.parents))


def scratch_root() -> Path:
    """A folder to make scratch folders in, outside any git work tree.

    The tool refuses to make a key inside one, and CTest points TEMP at the build folder, which is inside the
    checkout; so the system's usual temporary folders are tried in turn.
    """
    for candidate in (tempfile.gettempdir(), "/tmp", str(Path.home() / "AppData" / "Local" / "Temp"), str(Path.home())):
        folder = Path(candidate).resolve()
        if folder.is_dir() and not in_git_work_tree(folder):
            return folder
    raise RuntimeError("there is no temporary folder outside a git work tree to make keys in")


class Scratch(unittest.TestCase):
    """A folder of its own for each test, outside any git work tree, removed afterwards."""

    def setUp(self) -> None:
        self.folder = Path(tempfile.mkdtemp(prefix="adl-owner-test-", dir=scratch_root()))
        self.addCleanup(shutil.rmtree, self.folder, True)

    def write(self, name: str, text: str) -> Path:
        path = self.folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("utf-8"))
        return path


class CommandLine(Scratch):
    def test_help_lists_the_commands(self) -> None:
        for flag in ("--help", "-h", "help"):
            result = run(flag)
            self.assertEqual(result.code, 0)
            for word in ("inspect", "verify", "issue", "revocations", "keygen", "--test"):
                self.assertIn(word, result.out)

    def test_usage_errors_exit_2_and_say_where_to_look(self) -> None:
        for args in ([], ["frobnicate"], ["inspect"], ["inspect", "a", "b"], ["verify", "--bogus", "x"],
                     ["issue"], ["issue", "--request"], ["issue", "--request", "a", "--out", "b"],
                     ["issue", "--test", "--key-file", "k", "--request", "a", "--out", "b"],
                     ["keygen", "--key-id", "lic-1"], ["verify", "--now", "yesterday", "x"]):
            with self.subTest(args=args):
                result = run(*args)
                self.assertEqual(result.code, 2, result.err)
                self.assertTrue(result.err.startswith("adl_licence_owner: "), result.err)
                self.assertIn("Run adl_licence_owner --help", result.err)

    def test_an_option_given_twice_is_refused(self) -> None:
        result = run("verify", "--now", "2026-01-01T00:00:00Z", "--now", "2026-01-02T00:00:00Z", "x")
        self.assertEqual(result.code, 2)
        self.assertIn("--now was given twice", result.err)

    def test_the_test_note_is_the_first_line_of_every_test_output(self) -> None:
        request = self.write("r.json", request_text())
        valid = DATA / "licences" / "valid.adllic"
        commands = [
            ["issue", "--test", "--request", request, "--out", self.folder / "o.adllic"],
            ["issue", "--test", "--dry-run", "--request", request],
            ["inspect", "--test", valid],
            ["verify", "--test", valid, "--now", "2026-06-01T00:00:00Z"],
        ]
        for args in commands:
            with self.subTest(command=args[0]):
                result = run(*args)
                self.assertEqual(result.code, 0, result.err)
                self.assertTrue(result.out.startswith(TEST_NOTE), result.out)
        # Without --test there is no note, and a file signed with a test key still says so.
        plain = run("inspect", valid)
        self.assertNotIn(TEST_NOTE, plain.out)
        self.assertEqual(plain.lines()["Test key"], "yes")
        self.assertTrue(plain.lines()["Signature"].startswith("not checked"))


class TestLicences(Scratch):
    """The committed test licences are judged as the manifest says."""

    def rows(self) -> list[list[str]]:
        rows = []
        for line in (DATA / "licences" / "manifest.txt").read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.startswith("#"):
                rows.append(line.split())
        return rows

    def test_every_row_of_the_manifest(self) -> None:
        rows = self.rows()
        self.assertGreater(len(rows), 20)
        seen_states = set()
        for name, now, latest_seen, computer, revocations, state, reason, *_ in rows:
            seen_states.add(state)
            if name == "-":
                self.assertEqual((state, reason), ("none", "no_licence"))
                continue
            with self.subTest(file=name, now=now, computer=computer, revocations=revocations):
                args: list[str | Path] = ["verify", "--test", DATA / "licences" / name, "--now", now]
                if latest_seen != "-":
                    args += ["--latest-seen", latest_seen]
                if computer != "-":
                    args += ["--computer", computer]
                if revocations != "-":
                    args += ["--revocations", DATA / "licences" / revocations]
                result = run(*args)
                found = result.lines()
                self.assertEqual((found["State"], found["Reason"]), (state, reason), result.out + result.err)
                self.assertEqual(result.code, 0 if state in ACTIVE_STATES else 1)
        # A test licence for each state the library knows.
        self.assertEqual(seen_states,
                         {"none", "valid", "expiring", "grace", "expired", "not_yet_valid", "clock_suspect", "invalid"})

    def test_the_revocation_lists_verify(self) -> None:
        for name, sequence in (("revocations_1.adlrev", "1"), ("revocations_2.adlrev", "2")):
            with self.subTest(file=name):
                result = run("verify", "--test", DATA / "licences" / name)
                self.assertEqual(result.code, 0, result.err)
                found = result.lines()
                self.assertEqual(found["Purpose"], "revocations")
                self.assertEqual(found["Signature"], "verifies")
                self.assertEqual(found["Sequence"], sequence)
        second = run("inspect", "--test", DATA / "licences" / "revocations_2.adlrev").out
        self.assertIn("Withdrawn licence: ADL-2026-900012", second)
        self.assertIn("Withdrawn key: lic-2026-x from 2026-11-01T00:00:00Z", second)
        self.assertIn("Withdrawn key: lic-2026-y\n", second)

    def test_without_the_test_key_nothing_verifies(self) -> None:
        # No real key is in the trust list yet, so every licence is refused: the programs stay Community.
        result = run("verify", DATA / "licences" / "valid.adllic", "--now", "2026-06-01T00:00:00Z")
        self.assertEqual(result.code, 1)
        self.assertEqual(result.lines()["State"], "invalid")
        self.assertEqual(result.lines()["Reason"], "newer_version")

    def test_inspect_shows_the_fields_and_text_outside_ascii(self) -> None:
        result = run("inspect", "--test", DATA / "licences" / "enterprise.adllic")
        self.assertEqual(result.code, 0, result.err)
        found = result.lines()
        self.assertEqual(found["Signature"], "verifies")
        self.assertEqual(found["Key trusted by this build"], "no")
        self.assertEqual(found["Licence"], "ADL-2026-900008")
        self.assertEqual(found["Product"], "adl")
        self.assertEqual(found["Edition"], "enterprise")
        self.assertEqual(found["Licensee"], "M\U000000f6ller Avionik GmbH \U0000822a\U00007a7a")
        self.assertEqual(found["Contact"], "Pr\U000000fcfstand 3")
        self.assertEqual(found["Seats"], "0 (site licence)")
        self.assertEqual(found["Features"], "support, data_packs")
        self.assertEqual(found["Updates until"], "2029-11-13T23:59:59Z")

    def test_inspect_says_how_a_bad_file_fails(self) -> None:
        bad = run("inspect", "--test", DATA / "licences" / "bad_signature.adllic")
        self.assertEqual(bad.code, 0)
        self.assertTrue(bad.lines()["Signature"].startswith("does not verify"), bad.out)
        unknown = run("inspect", "--test", DATA / "licences" / "unknown_key.adllic")
        self.assertTrue(unknown.lines()["Signature"].startswith("not checked"), unknown.out)
        damaged = run("inspect", "--test", DATA / "licences" / "malformed.adllic")
        self.assertEqual(damaged.code, 1)
        self.assertIn("cannot read", damaged.err)
        missing = run("inspect", self.folder / "nothing.adllic")
        self.assertEqual(missing.code, 1)
        self.assertIn("it does not exist", missing.err)


class Issue(Scratch):
    def test_issue_writes_a_file_that_verifies(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-900100"))
        out = self.folder / "out.adllic"
        result = run("issue", "--test", "--request", request, "--out", out)
        self.assertEqual(result.code, 0, result.err)
        self.assertEqual(result.lines()["Signature"], "verifies")
        self.assertEqual(result.lines()["Licence"], "ADL-2026-900100")
        again = run("verify", "--test", out, "--now", "2026-12-01T00:00:00Z")
        self.assertEqual(again.code, 0, again.out)
        self.assertEqual(again.lines()["State"], "valid")

    def test_issue_is_deterministic(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-900101"))
        first, second = self.folder / "a.adllic", self.folder / "b.adllic"
        self.assertEqual(run("issue", "--test", "--request", request, "--out", first).code, 0)
        self.assertEqual(run("issue", "--test", "--request", request, "--out", second).code, 0)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertTrue(first.read_bytes().endswith(b"}\n"))
        self.assertNotIn(b"\r", first.read_bytes())

    def test_dry_run_needs_no_key_and_writes_nothing(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-000123"))
        result = run("issue", "--request", request, "--dry-run")
        self.assertEqual(result.code, 0, result.err)
        self.assertIn("Dry run: nothing was signed and nothing was written.", result.out)
        self.assertEqual(result.lines()["Licence"], "ADL-2026-000123")
        self.assertEqual(list(self.folder.iterdir()), [request])

    def test_issue_never_writes_over_a_file(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-900102"))
        out = self.write("out.adllic", "keep me")
        result = run("issue", "--test", "--request", request, "--out", out)
        self.assertEqual(result.code, 1)
        self.assertIn("it exists already", result.err)
        self.assertEqual(out.read_text(), "keep me")

    def test_refused_requests_leave_nothing_behind(self) -> None:
        cases = {
            "id_shape": (request_text("ADL-26-1"), "is not of the form ADL-2026-000123"),
            "id_prefix": (request_text("VSS-2026-000123"), "is not of the form ADL-2026-000123"),
            "damaged": ("{", "does not read it as a request"),
            "bad_grace": (request_text().replace('"grace_days": 28', '"grace_days": 4000'), "does not read it"),
            "unknown_critical": (request_text(extra='  "future": 1,\n').replace('"critical": []', '"critical": ["future"]'),
                                 "does not read it"),
        }
        for name, (text, why) in cases.items():
            with self.subTest(case=name):
                request = self.write(f"{name}.json", text)
                out = self.folder / f"{name}.adllic"
                result = run("issue", "--test", "--request", request, "--out", out)
                self.assertEqual(result.code, 1, result.out)
                self.assertIn(why, result.err)
                self.assertFalse(out.exists())

    def test_the_test_range_is_for_test_licences(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-900001"))
        result = run("issue", "--request", request, "--dry-run")
        self.assertEqual(result.code, 1)
        self.assertIn("900000 to 999999", result.err)
        self.assertEqual(run("issue", "--test", "--request", request, "--dry-run").code, 0)

    def test_a_licence_for_another_product_needs_test(self) -> None:
        request = self.write("request.json", request_text("ADL-2026-900104", product="vss"))
        self.assertEqual(run("issue", "--request", request, "--dry-run").code, 1)
        self.assertEqual(run("issue", "--test", "--request", request, "--out", self.folder / "o.adllic").code, 0)

    def test_a_missing_request_or_key_file_is_a_failure_not_a_usage_error(self) -> None:
        request = self.write("request.json", request_text())
        missing = run("issue", "--request", self.folder / "none.json", "--dry-run")
        self.assertEqual(missing.code, 1)
        no_key = run("issue", "--request", request, "--out", self.folder / "o.adllic",
                     "--key-id", "lic-2026-a", "--key-file", self.folder / "no.adlownerkey")
        self.assertEqual(no_key.code, 1)
        self.assertIn("the key file", no_key.err)
        self.assertFalse((self.folder / "o.adllic").exists())
        junk = self.write("junk.adlownerkey", "this is not a key\n")
        not_a_key = run("issue", "--request", request, "--out", self.folder / "o.adllic",
                        "--key-id", "lic-2026-a", "--key-file", junk)
        self.assertEqual(not_a_key.code, 1)
        self.assertIn("it does not hold a key as keygen writes it", not_a_key.err)
        self.assertNotIn("this is not a key", not_a_key.out + not_a_key.err)

    def test_a_key_id_that_begins_with_test_is_for_test_only(self) -> None:
        request = self.write("request.json", request_text())
        key = self.write("k.adlownerkey", base64.urlsafe_b64encode(bytes(range(32))).decode().rstrip("=") + "\n")
        result = run("issue", "--request", request, "--out", self.folder / "o.adllic", "--key-id", "test-x", "--key-file", key)
        self.assertEqual(result.code, 2)
        self.assertIn("begins with test-", result.err)

    def test_revocations_are_signed_and_read_back(self) -> None:
        request = self.write("list.json", (DATA / "requests" / "revocations_2.json").read_text(encoding="utf-8"))
        out = self.folder / "list.adlrev"
        result = run("revocations", "--test", "--request", request, "--out", out)
        self.assertEqual(result.code, 0, result.err)
        self.assertEqual(out.read_bytes(), (DATA / "licences" / "revocations_2.adlrev").read_bytes())
        self.assertEqual(result.lines()["Sequence"], "2")
        empty = self.write("empty.json", request_text())
        refused = run("revocations", "--test", "--request", empty, "--out", self.folder / "x.adlrev")
        self.assertEqual(refused.code, 1)
        zero = self.write("zero.json", (DATA / "requests" / "revocations_1.json").read_text().replace('"sequence": 1', '"sequence": 0'))
        self.assertEqual(run("revocations", "--test", "--request", zero, "--dry-run").code, 1)


class Keys(Scratch):
    def make_key(self, key_id: str = "lic-2026-a", purpose: str = "licence") -> tuple[Path, Path, Result]:
        folder = self.folder / "keys"
        folder.mkdir(exist_ok=True)
        result = run("keygen", "--key-id", key_id, "--purpose", purpose, "--out", folder)
        return folder / f"{key_id}.adlownerkey", folder / f"{key_id}.adlownerpub", result

    def test_keygen_writes_the_pair_and_never_prints_the_private_half(self) -> None:
        private, public, result = self.make_key()
        self.assertEqual(result.code, 0, result.err)
        secret = private.read_text(encoding="ascii")
        self.assertRegex(secret, r"^[A-Za-z0-9_-]{43}\n$")
        everything = result.out + result.err
        self.assertNotIn(secret.strip(), everything)
        self.assertNotIn(secret.strip()[:12], everything)
        self.assertNotIn(base64.urlsafe_b64decode(secret.strip() + "=").hex(), everything)
        text = public.read_text(encoding="ascii")
        self.assertRegex(text, r'^\{"key_id":"lic-2026-a","purpose":"licence","public_key":"[A-Za-z0-9_-]{43}"\}\n$')
        self.assertIn(text.split('"public_key":"')[1][:43], result.out)
        self.assertIn("not shown here and is never logged", result.out)
        self.assertIn("trusted_keys.cpp", result.out)
        self.assertEqual(sorted(p.name for p in private.parent.iterdir()), ["lic-2026-a.adlownerkey", "lic-2026-a.adlownerpub"])

    def test_the_private_file_is_for_its_owner_only_where_the_system_says_so(self) -> None:
        private, _, result = self.make_key()
        self.assertEqual(result.code, 0, result.err)
        if os.name != "posix":
            return
        # A file system that does not keep permissions (a Windows drive seen from WSL) shows 0o777 whatever is
        # asked of it; there is nothing to test there.
        probe = self.folder / "probe"
        probe.write_bytes(b"x")
        probe.chmod(0o600)
        if stat.S_IMODE(probe.stat().st_mode) != 0o600:
            self.skipTest("the file system does not keep permissions")
        self.assertEqual(stat.S_IMODE(private.stat().st_mode), 0o600)

    def test_two_keys_differ(self) -> None:
        folder = self.folder / "keys"
        folder.mkdir()
        for name in ("lic-2026-a", "lic-2026-b"):
            self.assertEqual(run("keygen", "--key-id", name, "--purpose", "licence", "--out", folder).code, 0)
        self.assertNotEqual((folder / "lic-2026-a.adlownerkey").read_bytes(), (folder / "lic-2026-b.adlownerkey").read_bytes())

    def test_keygen_says_where_the_key_came_from(self) -> None:
        _, _, result = self.make_key()
        self.assertEqual(result.lines()["Key source"], "the system's random numbers (std::random_device)")

    def test_a_seed_file_is_the_key(self) -> None:
        seed = bytes(range(1, 33))
        seed_file = self.folder / "seed.bin"
        seed_file.write_bytes(seed)
        folder = self.folder / "keys"
        folder.mkdir()
        result = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder, "--seed-file", seed_file)
        self.assertEqual(result.code, 0, result.err)
        text = base64.urlsafe_b64encode(seed).decode().rstrip("=")
        self.assertEqual((folder / "lic-2026-a.adlownerkey").read_text(encoding="ascii"), text + "\n")
        # The tool leaves the seed file as it was, says where the key came from and that the file is the key,
        # and does not print the key.
        self.assertEqual(seed_file.read_bytes(), seed)
        self.assertTrue(result.lines()["Key source"].startswith("the seed file"), result.out)
        self.assertIn("The seed file is the same key as the private key file", result.out)
        everything = result.out + result.err
        for form in (text, seed.hex(), base64.b64encode(seed).decode()):
            self.assertNotIn(form, everything)
        # The same seed is the same key, and it signs.
        again = self.folder / "again"
        again.mkdir()
        self.assertEqual(run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", again,
                             "--seed-file", seed_file).code, 0)
        self.assertEqual((again / "lic-2026-a.adlownerpub").read_bytes(), (folder / "lic-2026-a.adlownerpub").read_bytes())
        request = self.write("request.json", request_text("ADL-2026-000123"))
        out = self.folder / "o.adllic"
        self.assertEqual(run("issue", "--request", request, "--out", out, "--key-id", "lic-2026-a",
                             "--key-file", folder / "lic-2026-a.adlownerkey").code, 0)
        judged = run("verify", out, "--trust", folder / "lic-2026-a.adlownerpub", "--now", "2026-12-01T00:00:00Z")
        self.assertEqual(judged.code, 0, judged.out)

    def test_a_seed_file_that_is_not_32_bytes_of_a_key_is_refused(self) -> None:
        folder = self.folder / "keys"
        folder.mkdir()
        as_text = bytes(range(1, 33)).hex().encode()
        cases = {
            "empty": (b"", "not exactly 32 bytes"),
            "short": (bytes(range(1, 32)), "not exactly 32 bytes"),
            "long": (bytes(range(1, 34)), "not exactly 32 bytes"),
            "huge": (bytes(5000), "larger than"),
            "text": (as_text, "not exactly 32 bytes"),
            "zeros": (bytes(32), "all zero or are the test key"),
            "test_key": (TEST_SEED, "all zero or are the test key"),
        }
        for name, (data, why) in cases.items():
            with self.subTest(case=name):
                seed_file = self.folder / f"{name}.bin"
                seed_file.write_bytes(data)
                result = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder,
                             "--seed-file", seed_file)
                self.assertEqual(result.code, 1, result.out)
                self.assertIn("the seed file", result.err)
                self.assertIn(why, result.err)
                self.assertNotIn(as_text.decode(), result.out + result.err)
                self.assertEqual(list(folder.iterdir()), [])
        missing = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder,
                      "--seed-file", self.folder / "nothing.bin")
        self.assertEqual(missing.code, 1)
        self.assertIn("it does not exist", missing.err)

    def test_keygen_never_writes_over_a_key(self) -> None:
        private, public, first = self.make_key()
        before = private.read_bytes()
        second = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", private.parent)
        self.assertEqual(second.code, 1)
        self.assertIn("it exists already", second.err)
        self.assertEqual(private.read_bytes(), before)

    def test_keygen_refuses_a_folder_inside_a_git_repository(self) -> None:
        (self.folder / "work" / ".git").mkdir(parents=True)
        inside = self.folder / "work" / "sub"
        inside.mkdir()
        result = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", inside)
        self.assertEqual(result.code, 1)
        self.assertIn("inside a git repository", result.err)
        self.assertEqual(list(inside.iterdir()), [])

    def test_keygen_refuses_what_it_cannot_use(self) -> None:
        folder = self.folder / "keys"
        folder.mkdir()
        self.assertEqual(run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", self.folder / "no").code, 1)
        self.assertEqual(run("keygen", "--key-id", "Lic_2026", "--purpose", "licence", "--out", folder).code, 2)
        self.assertEqual(run("keygen", "--key-id", "test-a", "--purpose", "licence", "--out", folder).code, 2)
        self.assertEqual(run("keygen", "--key-id", "lic-2026-a", "--purpose", "plugin", "--out", folder).code, 2)
        self.assertEqual(run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder, "--test").code, 2)
        self.assertEqual(list(folder.iterdir()), [])

    def test_a_key_made_here_signs_a_licence_that_verifies_with_its_public_file(self) -> None:
        private, public, made = self.make_key("lic-2026-a", "licence")
        self.assertEqual(made.code, 0, made.err)
        request = self.write("request.json", request_text("ADL-2026-000123"))
        out = self.folder / "ADL-2026-000123.adllic"
        issued = run("issue", "--request", request, "--out", out, "--key-id", "lic-2026-a", "--key-file", private)
        self.assertEqual(issued.code, 0, issued.err)
        self.assertNotIn(TEST_NOTE, issued.out)
        self.assertEqual(issued.lines()["Signature"], "verifies")
        self.assertEqual(issued.lines()["Key trusted by this build"], "no")
        self.assertNotIn("Test key", issued.lines())

        # Without the public file the key is not known to the tool; with it, the file verifies.
        unknown = run("inspect", out)
        self.assertTrue(unknown.lines()["Signature"].startswith("not checked"), unknown.out)
        known = run("inspect", out, "--trust", public)
        self.assertEqual(known.lines()["Signature"], "verifies")
        self.assertEqual(known.lines()["Key trusted by this build"], "no")
        judged = run("verify", out, "--trust", public, "--now", "2026-12-01T00:00:00Z")
        self.assertEqual(judged.code, 0, judged.out)
        self.assertEqual((judged.lines()["State"], judged.lines()["Reason"]), ("valid", "ok"))
        self.assertEqual(judged.lines()["Key trusted by this build"], "no")

        # A key made for revocation lists does not verify a licence, and the other way round.
        rev_private, rev_public, rev_made = self.make_key("rev-2026-a", "revocations")
        self.assertEqual(rev_made.code, 0)
        wrong = run("verify", out, "--trust", rev_public, "--now", "2026-12-01T00:00:00Z")
        self.assertEqual(wrong.code, 1)
        listed = self.folder / "list.adlrev"
        list_request = self.write("list.json", (DATA / "requests" / "revocations_1.json").read_text(encoding="utf-8"))
        signed = run("revocations", "--request", list_request, "--out", listed, "--key-id", "rev-2026-a", "--key-file", rev_private)
        self.assertEqual(signed.code, 0, signed.err)
        self.assertEqual(run("verify", listed, "--trust", rev_public).code, 0)
        self.assertEqual(run("verify", listed, "--trust", public).code, 1)

    def test_public_key_files_that_are_not_what_keygen_writes_are_refused(self) -> None:
        _, public, _ = self.make_key()
        good = public.read_text(encoding="ascii")
        valid = DATA / "licences" / "valid.adllic"
        for name, text in {
            "empty": "",
            "spaces": good.replace('","purpose', '", "purpose'),
            "purpose": good.replace('"licence"', '"plugin"'),
            "short_key": good.replace('"}', 'AA"}'),
            "id": good.replace("lic-2026-a", "Lic_2026"),
            "two_lines": good + good,
        }.items():
            with self.subTest(case=name):
                result = run("inspect", valid, "--trust", self.write(f"{name}.adlownerpub", text))
                self.assertEqual(result.code, 1)
                self.assertIn("is refused", result.err)


class Utf8(Scratch):
    """Names outside the code page: the tool reads and writes them as UTF-8, on every system."""

    NAMES = ("\U000000dcberpr\U000000fcfung", "\U000030e9\U000030dc", "\U0000043a\U0000043b\U0000044e\U00000447", "\U0001f511")

    def test_every_file_name_the_tool_is_given_may_hold_any_character(self) -> None:
        for word in self.NAMES:
            with self.subTest(name=word):
                folder = self.folder / f"keys-{word}"
                folder.mkdir()
                key_result = run("keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder)
                self.assertEqual(key_result.code, 0, key_result.err)
                self.assertIn(word, key_result.out)
                request = self.write(f"request-{word}.json", request_text("ADL-2026-000123"))
                out = folder / f"licence-{word}.adllic"
                issued = run("issue", "--request", request, "--out", out,
                             "--key-id", "lic-2026-a", "--key-file", folder / "lic-2026-a.adlownerkey")
                self.assertEqual(issued.code, 0, issued.err)
                self.assertTrue(out.is_file())
                self.assertIn(word, issued.out)
                judged = run("verify", out, "--trust", folder / "lic-2026-a.adlownerpub", "--now", "2026-12-01T00:00:00Z")
                self.assertEqual(judged.code, 0, judged.out)

    def test_a_licensee_name_outside_ascii_is_signed_and_shown(self) -> None:
        text = request_text("ADL-2026-900105").replace("Example Lab", "Pr\U000000fcfstand \U000030e9\U000030dc \U0001f511")
        request = self.write("request.json", text)
        out = self.folder / "o.adllic"
        self.assertEqual(run("issue", "--test", "--request", request, "--out", out).code, 0)
        self.assertEqual(run("inspect", "--test", out).lines()["Licensee"], "Pr\U000000fcfstand \U000030e9\U000030dc \U0001f511")


class Regeneration(Scratch):
    def test_the_script_makes_the_committed_files_again(self) -> None:
        script = Path(__file__).resolve().parent / "make_test_licences.py"
        done = subprocess.run([sys.executable, "-X", "utf8", "-I", str(script), "--tool", str(TOOL), "--check"],
                              capture_output=True, text=True, encoding="utf-8", check=False)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)


def main() -> int:
    global TOOL, DATA
    rest = sys.argv[1:]
    values = {}
    while rest and rest[0] in ("--tool", "--data"):
        values[rest[0]] = rest[1]
        rest = rest[2:]
    if "--tool" not in values or "--data" not in values:
        print("usage: test_owner_tool.py --tool <adl_licence_owner> --data <tests/licence/data> [unittest options]",
              file=sys.stderr)
        return 2
    TOOL = Path(values["--tool"]).resolve()
    DATA = Path(values["--data"]).resolve()
    program = unittest.main(argv=[sys.argv[0], *rest], exit=False)
    return 0 if program.result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
