#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""System tests of adl_licence: the built programs, run as processes.

    python3 -X utf8 -I tests/licence_cli/test_adl_licence.py --program <adl_licence> \
        --test-program <adl_licence_test_program> --data tests/licence/data --scratch <folder>

The program that ships trusts the four public keys of ADL and no test key, and the private halves are the owner's
alone, so what it can be shown is its usage, its words, an empty computer and the refusal of every test licence
(the test key is one it does not know, which it reads as a licence for a newer version); that is the first half.
The second half runs adl_licence_test_program, the same commands with the throwaway test key of RFC 8032 and a clock and a computer
name of the test's own, so that a licence is imported, shown, checked and removed in each state, and the manifest
of test licences (tests/licence/data/licences/manifest.txt) is judged through the command line. Every run has
ADL_SETTINGS_DIR set to a folder of its own, so no real licence folder is ever read or written. CTest runs it as
`licence.cli` (label system).
"""

from __future__ import annotations

import argparse
import base64
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

PROGRAM: Path
TEST_PROGRAM: Path
DATA: Path
SCRATCH: Path

# RFC 8032, section 7.1, test 1: the private half of the test key. It must never be printed.
TEST_SEED = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")
TEST_SEED_TEXT = base64.urlsafe_b64encode(TEST_SEED).decode("ascii").rstrip("=")

NEWER_VERSION = ("This licence needs a newer version of ARINC Data Loader. Install the latest version, "
                 "then import the licence again.")
IN_FORCE = {"valid", "expiring", "grace"}


class Result:
    def __init__(self, code: int, out: str, err: str):
        self.code = code
        self.out = out
        self.err = err

    def lines(self) -> dict[str, str]:
        """The `Name: value` lines of the output that are not indented; the first of each name wins."""
        found: dict[str, str] = {}
        for line in self.out.splitlines():
            name, separator, value = line.partition(": ")
            if separator and not line.startswith(" ") and name not in found:
                found[name] = value
        return found


def run(program: Path, settings: Path | None, *args: str | Path, now: str | None = None,
        computer: str | None = None) -> Result:
    """Runs a program with ADL_SETTINGS_DIR set to `settings`; a test program also gets --now and --computer."""
    environment = dict(os.environ)
    environment.pop("ADL_SETTINGS_DIR", None)
    if settings is not None:
        environment["ADL_SETTINGS_DIR"] = str(settings)
    command = [str(program)]
    if now is not None:
        command += ["--now", now]
    if computer is not None:
        command += ["--computer", computer]
    command += [str(arg) for arg in args]
    done = subprocess.run(command, capture_output=True, check=False, env=environment)
    # A Windows console program writes CR LF line ends; the tests read LF.
    result = Result(done.returncode, done.stdout.decode("utf-8").replace("\r\n", "\n"),
                    done.stderr.decode("utf-8").replace("\r\n", "\n"))
    assert TEST_SEED_TEXT not in result.out + result.err, "a program printed the private half of the test key"
    return result


def tree(folder: Path) -> dict[str, bytes | None]:
    """Every file and folder below `folder` with its bytes (None for a folder): what a command left behind."""
    found: dict[str, bytes | None] = {}
    if folder.exists():
        for path in sorted(folder.rglob("*")):
            found[str(path.relative_to(folder))] = path.read_bytes() if path.is_file() else None
    return found


class Scratch(unittest.TestCase):
    """A folder of its own for each test, removed afterwards: the settings folder and a place for loose files."""

    def setUp(self) -> None:
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.folder = Path(tempfile.mkdtemp(prefix="adl-licence-test-", dir=SCRATCH))
        self.addCleanup(shutil.rmtree, self.folder, True)
        self.settings = self.folder / "settings"
        self.inbox = self.folder / "inbox"
        self.inbox.mkdir()

    def licence(self, name: str) -> Path:
        """A test licence copied to the inbox, where a customer would have it."""
        target = self.inbox / name
        shutil.copyfile(DATA / "licences" / name, target)
        return target


class Shipped(Scratch):
    """The program as it ships: ADL's four public keys and no test key, so a test licence is not accepted."""

    def test_help_and_version(self) -> None:
        helped = run(PROGRAM, self.settings, "--help")
        self.assertEqual(helped.code, 0, helped.err)
        for word in ("usage: adl_licence show", "import <file>", "remove <name>", "verify <file>", "--version"):
            self.assertIn(word, helped.out)
        version = run(PROGRAM, self.settings, "--version")
        self.assertEqual(version.code, 0, version.err)
        self.assertTrue(version.out.startswith("adl_licence "), version.out)
        self.assertEqual(len(version.out.splitlines()), 1)
        self.assertEqual(tree(self.settings), {})

    def test_usage_errors_exit_2_and_say_what_is_wrong(self) -> None:
        for args in ([], ["frobnicate"], ["--json"], ["show", "x"], ["import"], ["import", "a", "b"], ["remove"],
                     ["verify"], ["--help", "show"]):
            with self.subTest(args=args):
                result = run(PROGRAM, self.settings, *args)
                self.assertEqual(result.code, 2, result.err)
                self.assertEqual(result.out, "")
                self.assertTrue(result.err.startswith("adl_licence: "), result.err)
                self.assertIn("usage: adl_licence show", result.err)
        self.assertEqual(tree(self.settings), {})

    def test_an_empty_computer_has_no_licence_and_looking_writes_nothing(self) -> None:
        result = run(PROGRAM, self.settings, "show")
        self.assertEqual(result.code, 0, result.err)
        self.assertEqual(result.lines().get("Edition"), "community")
        self.assertEqual(result.lines().get("State"), "none")
        self.assertEqual(result.lines().get("Files"), "none")
        self.assertEqual(result.err, "")
        self.assertFalse(self.settings.exists())

    def test_a_licence_made_with_the_test_key_is_refused_as_one_for_a_newer_version(self) -> None:
        # The test key is not one of the keys of the release, so the program does not know it. That is what a
        # licence made with a key of a later release looks like, and the sentence says so. It is also the check
        # that no test key reaches a program that ships.
        licence = self.licence("valid.adllic")
        imported = run(PROGRAM, self.settings, "import", licence)
        self.assertEqual(imported.code, 1)
        self.assertEqual(imported.err.strip(), "import: " + NEWER_VERSION)
        verified = run(PROGRAM, self.settings, "verify", licence)
        self.assertEqual(verified.code, 1)
        self.assertEqual(verified.err.strip(), "verify: " + NEWER_VERSION)
        self.assertFalse(self.settings.exists())
        self.assertTrue(licence.exists())

    def test_no_test_licence_in_a_licence_folder_is_in_force(self) -> None:
        # Every licence the tests made, put where a customer would put one: none is in force, each is listed as not
        # valid, and the edition stays Community. Nothing is written by looking.
        licences = self.settings / "licences"
        licences.mkdir(parents=True)
        names = sorted(path.name for path in (DATA / "licences").glob("*.adllic"))
        self.assertGreaterEqual(len(names), 15)
        for name in names:
            shutil.copyfile(DATA / "licences" / name, licences / name)
        before = tree(self.settings)
        shown = run(PROGRAM, self.settings, "show", now=None)
        self.assertEqual(shown.code, 0, shown.err)
        self.assertEqual(shown.lines().get("Edition"), "community")
        self.assertEqual(shown.lines().get("State"), "invalid")
        self.assertNotIn("Note:", shown.out)
        for name in names:
            self.assertIn(f"  {name} (user, invalid, ", shown.out)
        self.assertEqual(tree(self.settings), before, "show wrote something")

    def test_a_file_that_is_not_there_or_not_a_licence_is_said_so(self) -> None:
        missing = run(PROGRAM, self.settings, "import", self.inbox / "none.adllic")
        self.assertEqual(missing.code, 1)
        self.assertIn("cannot read", missing.err)
        self.assertIn("it does not exist", missing.err)
        text = self.inbox / "text.adllic"
        text.write_text("this is not a licence", encoding="utf-8")
        damaged = run(PROGRAM, self.settings, "import", text)
        self.assertEqual(damaged.code, 1)
        self.assertIn("This file is not a valid ARINC Data Loader licence.", damaged.err)

    def test_remove_of_a_file_that_is_not_there(self) -> None:
        result = run(PROGRAM, self.settings, "remove", "ADL-2026-000123.adllic")
        self.assertEqual(result.code, 1)
        self.assertIn("there is no file called ADL-2026-000123.adllic in your licence folder", result.err)
        refused = run(PROGRAM, self.settings, "remove", "../x.adllic")
        self.assertEqual(refused.code, 1)
        self.assertIn("is not the name of a licence file", refused.err)

    def test_names_outside_the_code_page_reach_the_program_whole(self) -> None:
        # A settings folder and a file with accented and Japanese letters in their names. If either name were
        # changed on the way, the licence would be "not found", and not refused for its key.
        settings = self.folder / "settings é 日本語"
        licence = self.inbox / "licence é 日本語.adllic"
        shutil.copyfile(DATA / "licences" / "valid.adllic", licence)
        shown = run(PROGRAM, settings, "show")
        self.assertEqual(shown.code, 0, shown.err)
        self.assertEqual(shown.lines().get("State"), "none")
        imported = run(PROGRAM, settings, "import", licence)
        self.assertEqual(imported.code, 1)
        self.assertEqual(imported.err.strip(), "import: " + NEWER_VERSION)
        missing = run(PROGRAM, settings, "import", self.inbox / "nope é 日本語.adllic")
        self.assertIn("nope é 日本語.adllic", missing.err)


class WithTestKey(Scratch):
    """The same commands with the test key, a clock and a computer name set by the test."""

    def import_ok(self, name: str, now: str = "2026-06-01T00:00:00Z") -> Result:
        result = run(TEST_PROGRAM, self.settings, "import", self.licence(name), now=now)
        self.assertEqual(result.code, 0, result.err)
        return result

    def test_help_version_and_the_options_of_the_test_program(self) -> None:
        self.assertEqual(run(TEST_PROGRAM, self.settings, "--help").code, 0)
        self.assertEqual(run(TEST_PROGRAM, self.settings, "--version").out, "adl_licence test\n")
        bad = run(TEST_PROGRAM, self.settings, "show", now="yesterday")
        self.assertEqual(bad.code, 2)
        self.assertIn("is not a time like", bad.err)

    def test_import_show_verify_and_remove_a_licence_from_end_to_end(self) -> None:
        licence = self.licence("valid.adllic")
        before = licence.read_bytes()

        verified = run(TEST_PROGRAM, self.settings, "verify", licence, now="2026-06-01T00:00:00Z")
        self.assertEqual(verified.code, 0, verified.err)
        self.assertEqual(verified.lines()["State"], "valid")
        self.assertIn("A program would accept this licence file.", verified.out)
        self.assertEqual(tree(self.settings), {})

        imported = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-06-01T00:00:00Z")
        self.assertEqual(imported.code, 0, imported.err)
        self.assertEqual(imported.out, "Imported licence ADL-2026-900001 as ADL-2026-900001.adllic.\n"
                                       "This is now the Pro edition.\n")
        copy = self.settings / "licences" / "ADL-2026-900001.adllic"
        self.assertEqual(copy.read_bytes(), before)
        self.assertEqual(licence.read_bytes(), before)

        after_import = tree(self.settings)
        shown = run(TEST_PROGRAM, self.settings, "show", now="2026-06-01T00:00:00Z")
        self.assertEqual(shown.code, 0, shown.err)
        lines = shown.lines()
        self.assertEqual(lines["Edition"], "pro")
        self.assertEqual(lines["State"], "valid")
        self.assertEqual(lines["Licence"], "ADL-2026-900001")
        self.assertEqual(lines["Licensed to"], "Example Lab Pty Ltd")
        self.assertEqual(lines["Term"], "2026-01-01 to 2027-01-01")
        self.assertEqual(lines["Days left"], "214")
        self.assertIn("  ADL-2026-900001.adllic (user, valid, in use)", shown.out)
        self.assertNotIn("example-lab.invalid", shown.out)
        self.assertEqual(tree(self.settings), after_import, "show wrote something")

        again = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-06-01T00:00:00Z")
        self.assertEqual(again.code, 0)
        self.assertIn("already in the licence folder", again.out)
        self.assertEqual(tree(self.settings), after_import)

        removed = run(TEST_PROGRAM, self.settings, "remove", "ADL-2026-900001.adllic")
        self.assertEqual(removed.code, 0, removed.err)
        self.assertFalse(copy.exists())
        self.assertTrue(licence.exists())
        self.assertEqual(run(TEST_PROGRAM, self.settings, "show", now="2026-06-01T00:00:00Z").lines()["State"], "none")

    def test_the_same_licence_in_each_part_of_its_term(self) -> None:
        self.import_ok("valid.adllic")
        for now, state, edition in (("2026-12-15T00:00:00Z", "expiring", "pro"),
                                    ("2027-01-10T00:00:00Z", "grace", "pro"),
                                    ("2027-03-01T00:00:00Z", "expired", "community"),
                                    ("2025-12-01T00:00:00Z", "clock_suspect", "community")):
            with self.subTest(now=now):
                lines = run(TEST_PROGRAM, self.settings, "show", now=now).lines()
                self.assertEqual(lines["State"], state)
                self.assertEqual(lines["Edition"], edition)

    def test_import_says_what_an_ended_licence_is(self) -> None:
        for name, now, sentence in (
                ("expired.adllic", "2026-06-01T00:00:00Z", "ended on 2026-03-01"),
                ("grace.adllic", "2026-03-10T00:00:00Z", "its grace period lasts until 2026-03-29"),
                ("not_yet_valid.adllic", "2026-11-20T00:00:00Z", "This licence starts on")):
            with self.subTest(licence=name):
                result = run(TEST_PROGRAM, self.settings, "import", self.licence(name), now=now)
                self.assertEqual(result.code, 0, result.err)
                self.assertIn(sentence, result.out)

    def test_what_a_program_refuses_is_refused_with_the_reason_and_nothing_is_written(self) -> None:
        for name, sentence in (("wrong_product.adllic", "This licence is for another product."),
                               ("bad_signature.adllic", "This file is not a valid ARINC Data Loader licence."),
                               ("malformed.adllic", "This file is not a valid ARINC Data Loader licence."),
                               ("newer_version.adllic", "This licence needs a newer version of ARINC Data Loader."),
                               ("unknown_key.adllic", "This licence needs a newer version of ARINC Data Loader.")):
            with self.subTest(licence=name):
                licence = self.licence(name)
                imported = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-06-01T00:00:00Z")
                self.assertEqual(imported.code, 1)
                self.assertIn("import: " + sentence, imported.err)
                verified = run(TEST_PROGRAM, self.settings, "verify", licence, now="2026-06-01T00:00:00Z")
                self.assertEqual(verified.code, 1)
                self.assertIn("verify: " + sentence, verified.err)
        self.assertEqual(tree(self.settings), {})

    def test_a_licence_for_some_computers(self) -> None:
        licence = self.licence("machines.adllic")
        refused = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-06-01T00:00:00Z", computer="OFFICE-PC")
        self.assertEqual(refused.code, 1)
        self.assertIn("This computer is OFFICE-PC.", refused.err)
        taken = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-06-01T00:00:00Z", computer="LAB-PC-01")
        self.assertEqual(taken.code, 0, taken.err)

    def test_a_list_of_withdrawn_licences(self) -> None:
        self.import_ok("revoked.adllic")
        listed = run(TEST_PROGRAM, self.settings, "import", self.licence("revocations_1.adlrev"),
                     now="2026-06-01T00:00:00Z")
        self.assertEqual(listed.code, 0, listed.err)
        self.assertIn("Imported the list of withdrawn licences as revocations-1.adlrev.", listed.out)
        shown = run(TEST_PROGRAM, self.settings, "show", now="2026-06-01T00:00:00Z")
        self.assertEqual(shown.lines()["State"], "invalid")
        self.assertEqual(shown.lines()["Reason"], "the licence or its key was withdrawn")
        self.assertEqual(shown.lines()["Revocation lists"], "1 accepted, 0 refused")
        verified = run(TEST_PROGRAM, self.settings, "verify", self.inbox / "revocations_1.adlrev",
                       now="2026-06-01T00:00:00Z")
        self.assertEqual(verified.code, 0, verified.err)
        self.assertEqual(verified.lines()["Licences withdrawn"], "1")

    def test_a_licence_in_the_folder_of_this_computer_is_shown_and_cannot_be_removed(self) -> None:
        machine = self.settings / "machine-licences"
        machine.mkdir(parents=True)
        shutil.copyfile(DATA / "licences" / "valid.adllic", machine / "ADL-2026-900001.adllic")
        shown = run(TEST_PROGRAM, self.settings, "show", now="2026-06-01T00:00:00Z")
        self.assertEqual(shown.lines()["State"], "valid")
        self.assertIn("  ADL-2026-900001.adllic (machine, valid, in use)", shown.out)
        removed = run(TEST_PROGRAM, self.settings, "remove", "ADL-2026-900001.adllic")
        self.assertEqual(removed.code, 1)
        self.assertIn("only an administrator can change", removed.err)
        self.assertTrue((machine / "ADL-2026-900001.adllic").exists())

    def test_a_clock_reset_is_applied_once(self) -> None:
        licence = self.licence("clock_reset.adllic")
        clock = self.settings / "licence-clock.json"
        self.settings.mkdir(parents=True)
        clock.write_text('{"format": 1, "latest_seen": "2027-06-01T00:00:00Z", "applied_resets": []}\n',
                         encoding="utf-8")
        result = run(TEST_PROGRAM, self.settings, "import", licence, now="2026-11-20T00:00:00Z")
        self.assertEqual(result.code, 0, result.err)
        self.assertIn("This is now the Pro edition.", result.out)
        self.assertIn('"applied_resets": ["', clock.read_text(encoding="utf-8"))
        shown = run(TEST_PROGRAM, self.settings, "show", now="2026-11-20T00:00:00Z")
        self.assertEqual(shown.lines()["State"], "valid")

    def test_the_manifest_of_test_licences_through_the_command_line(self) -> None:
        rows = []
        for line in (DATA / "licences" / "manifest.txt").read_text(encoding="utf-8").splitlines():
            columns = line.split()
            if columns and not columns[0].startswith("#"):
                rows.append(columns[:7])
        self.assertGreaterEqual(len(rows), 25)
        for file, now, latest_seen, computer, revocations, state, reason in rows:
            with self.subTest(file=file, now=now, computer=computer, revocations=revocations):
                settings = self.folder / f"row-{abs(hash((file, now, computer, revocations)))}"
                licences = settings / "licences"
                licences.mkdir(parents=True)
                if file != "-":
                    shutil.copyfile(DATA / "licences" / file, licences / file)
                if revocations != "-":
                    shutil.copyfile(DATA / "licences" / revocations, licences / revocations)
                if latest_seen != "-":
                    (settings / "licence-clock.json").write_text(
                        '{"format": 1, "latest_seen": "%s", "applied_resets": []}\n' % latest_seen, encoding="utf-8")
                where = computer if computer != "-" else "LAB-RIG-07"
                shown = run(TEST_PROGRAM, settings, "show", now=now, computer=where)
                self.assertEqual(shown.code, 0, shown.err)
                self.assertEqual(shown.lines()["State"], state)
                self.assertEqual(shown.lines()["Edition"] != "community", state in IN_FORCE)
                self.assertEqual("Reason" in shown.lines(), reason != "ok")
                if file != "-":
                    elsewhere = settings / "elsewhere" / file
                    elsewhere.parent.mkdir()
                    shutil.copyfile(DATA / "licences" / file, elsewhere)
                    verified = run(TEST_PROGRAM, settings, "verify", elsewhere, now=now, computer=where)
                    self.assertEqual(verified.code, 1 if state == "invalid" else 0, verified.err)


def main() -> int:
    global PROGRAM, TEST_PROGRAM, DATA, SCRATCH
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--test-program", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--scratch", type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    PROGRAM = arguments.program.resolve()
    TEST_PROGRAM = arguments.test_program.resolve()
    DATA = arguments.data.resolve()
    SCRATCH = arguments.scratch.resolve()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
