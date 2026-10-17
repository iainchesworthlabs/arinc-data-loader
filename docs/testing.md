# Testing

The tests are Catch2 unit tests for the libraries and the platform layer, plus
checks about the build itself and runs of the `adl_write_payloads` program. They run in CI on Windows (MSVC), Linux (GCC)
and macOS (AppleClang); locally they have been run on Windows only.

## Running them

After a build (see [Building](building.md)):

```sh
ctest --preset windows-msvc-debug              # or linux-gcc-debug, macos-clang-debug
ctest --preset windows-msvc-debug -R platform  # just the two build checks
ctest --preset windows-msvc-debug -R tools     # just the payload writer
```

Every Catch2 `TEST_CASE` is registered with CTest by name
(`catch_discover_tests`), so a failing case is named in the CTest output, and
`ctest --preset <preset>` shows the output of any that fail. To run the
unit-test program directly, with Catch2's own filters:

```sh
build/windows-msvc-debug/bin/adl_unit_tests --list-tests
build/windows-msvc-debug/bin/adl_unit_tests "[golden]"
```

The tags are `[crc]`, `[digest]`, `[integrity]`, `[check-value]`, `[part-number]`,
`[naming]`, `[header]`, `[load]`, `[batch]`, `[media-files]`, `[media-set]`,
`[label]`, `[media-tree]`, `[golden]`, `[compatibility]`, `[limits]`, `[robustness]` and
`[platform]`.

## What there is

| Test file | What it holds down |
| --- | --- |
| `crc_test.cpp` | The CRCs against the check values of ARINC 665-3 section 4.3 (also as `static_assert`), the Appendix L reference files whose content the standard describes, the usual `"123456789"` results, feeding data in pieces, and that a CRC appended to its data makes the CRC of the whole zero |
| `digest_test.cpp` | MD5 against RFC 1321's test suite and SHA-1 against FIPS 180's, a million `a`, every padding boundary, reading a digest mid-stream |
| `integrity_checker_test.cpp`, `check_value_type_test.cpp` | The five check values in the form section 5.1 stores them, Table 5.1's values, titles and lengths, types that cannot be calculated |
| `part_number_test.cpp` | Check characters (Appendix E's example), building a part number, the checks of sections 2.1.1 and 3.1 |
| `naming_test.cpp` | File names, extensions and reserved extensions (Table 3.2.2-1), pathnames, target hardware IDs |
| `header_file_test.cpp` | The layout of Figure C-1 (the fixed part, the pointers, padding, relative pointers), round trips, each way a header file can be damaged, reading a later format version through its pointers, the recommendations |
| `load_test.cpp` | Building a load, every check value type for the Load Check Value, verifying a load and each way it can fail |
| `batch_file_test.cpp` | The batch file layout, round trips, damage, the recommendations |
| `media_files_test.cpp` | The three lists: layout, round trips with and without user data, sequence numbers, damage, the recommendations |
| `media_set_test.cpp` | Building and reading media sets across members, the lists on each member, FILES.LUM's entries, partial reads, damage of every file, members of different sets, files of one name told apart by directory and CRC (section 3.2.4.2), findings |
| `media_tree_test.cpp` | Every sample media set through a directory tree and back (and the set read from the disk verifies like the one in memory), the backslash pathnames as native paths, empty files and deep directories, binary content, names that cannot be files on every platform (device names, reserved characters, trailing dots, non-ASCII), sequence numbers outside 1 to 255, duplicate names whatever their case, nothing written when anything is invalid, never writing over a non-empty member, a stray entry or a link in a tree being refused, and damage on the disk being found when the set is read. The temporary directories come from `tests/support/temp_dir.hpp`. The link case is skipped where the account cannot create symbolic links (an ordinary Windows account) |
| `media_label_test.cpp` | The label text, its date format, the required items |
| `golden_test.cpp` | The library's header file, batch file and list files against files written independently ([below](#golden-payloads)) |
| `compatibility_test.cpp` | The ARINC 615 and Boeing 777 layouts of Appendix F (built by `adl::samples`, which `adl_write_payloads` writes out) |
| `limits_test.cpp` | Each side of the 16-bit field limits, a load of a thousand files, a 3 MB file, a media set of many loads |
| `robustness_test.cpp` | Every decoder over every truncation, every single-bit change, damage with the CRC put right, and random bytes, under the sanitizers |
| `paths_test.cpp` | `adl::platform::executable_path` names the running test program |
| `tools.write_payloads*` (CTest) | `adl_write_payloads` writes every scenario to a fresh directory and verifies it (the setup test `tools.write_payloads.clean` empties the directory first, since the program never overwrites), `--list` works, and an unknown scenario or a missing output directory is refused |
| `tests/platform/check_fragments.cmake` | `platform.fragments`: each fragment satisfies the platform contract, on whichever host this runs |
| `tools/checks/check_platform_isolation.ps1` | `platform.isolation`: no preprocessor conditionals or defines, no OS headers outside the platform section, no OS tests in CMake outside the selector |

## Golden payloads

`tests/golden/make_golden.py` writes a header file, a batch file and the three
lists of a two-member media set. It is a second implementation of the formats,
written from the tables and figures of the standard and sharing no code with the
library, with the CRC from the sample program in Appendix K and the digests from
Python's `hashlib`. Its output is `tests/golden/golden_payloads.hpp`, which is
committed, so building the tests needs no Python. `golden_test.cpp` builds the
same scenario with the library (`tests/support/fixtures.hpp` forwards to the
scenario in `adl::samples`, which mirrors the script, input for input) and compares the bytes.

If the format changes on purpose, change both and regenerate:

```sh
python tests/golden/make_golden.py
```

This catches a mistake in the C++. It does not catch a misreading of the standard
that the script shares; [the coverage page](arinc665-coverage.md#places-the-standard-is-unclear)
lists where the standard is unclear.

## Does the suite fail when the code is wrong?

It was checked by breaking the library on purpose and running it: a wrong pointer
in the header, file resolution that ignores the CRC, check characters added where
they are exclusive-ored, a Load CRC over the wrong part of the header, a Load
Check Value that starts early, and text fields not padded to a word. Each was
caught, the last by 53 test cases.

## Adding a test

Put a `TEST_CASE` in the file for the thing it tests, or add a file under
`tests/unit/` and list it in `tests/CMakeLists.txt`. Name the case as a
sentence about behaviour, tag it, and when the standard says what the answer is,
use the standard's value and say where it comes from. A test of one platform's
code goes in `tests/platform/` (the platform section is the one place the
isolation check allows OS headers). Helpers for building byte strings, damaging a
file and putting its CRC right again are in `tests/support`.

A bug fix comes with the test that failed before it.

## Known gaps

- The golden payloads come from the same reading of the standard as the library.
  Nothing has yet been checked against a load or a media set written by another
  tool.
- Appendix L's random reference files and its Ethernet frame are not in the
  standard.
- No tool other than this one has read the trees `adl_write_payloads` writes.
- There are no tests of the Linux and macOS platform sources on their hosts
  until their CI legs have run.
