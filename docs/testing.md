# Testing

The tests are Catch2 unit tests for the libraries and the platform layer, plus
checks about the build itself and runs of the `adl_write_payloads` program. They
run in CI on Windows (MSVC), Linux (GCC) and macOS (AppleClang); locally they have
been run on Windows, and on Linux in a container.

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
build/windows-msvc-debug/bin/adl_platform_tests "[conformance]"
build/windows-msvc-debug/bin/adl_engine_tests "[operation]"
```

There are three test programs: `adl_unit_tests` (the ARINC 665 library, the 615A codec, the media
tree and the samples), `adl_platform_tests` (the platform layer: its portable
types, the fake platform and the conformance suite) and `adl_engine_tests` (the 615A engine,
runner and the whole operations against a simulated target on the fake platform).

The tags are `[crc]`, `[digest]`, `[integrity]`, `[check-value]`, `[part-number]`,
`[naming]`, `[header]`, `[load]`, `[batch]`, `[media-files]`, `[media-set]`,
`[label]`, `[media-tree]`, `[golden]`, `[compatibility]`, `[limits]`, `[robustness]` and
`[platform]`. The 615A codec adds `[tftp]` (with `[options]`, `[wait]` and `[abort]`),
`[find]`, `[protocol-files]` (with `[status]`, `[text]` and `[names]`), `[golden615a]` and
`[robustness615a]`. The platform layer adds `[memory]`, `[error]`, `[time]`, `[network]`,
`[fake]`, `[conformance]` (with `[clock]`, `[files]`, `[threads]`, `[ipc]`,
`[network]`), `[integration]` (real UDP over the loopback interface) and `[real]` (runs
on the real host only). The 615A engine adds `[engine]`, with `[tftp-transfer]` (and
`[wrap]`, `[sas]`, `[faults]`, `[timeout]`, `[wait]`, `[abort]`, `[options]`, `[tsize]`, `[checksum]`,
`[receive]`, `[send]`, `[io]`), `[loader]` (the data loader driven by hand, with `[status]`,
`[version]`), `[operation]` (the whole operations, with `[information]`, `[upload]`, `[download]`,
`[operator]`, `[init]`, `[port]`, `[status]`, `[timeout]`, `[abort]`, `[version]`, `[faults]`),
`[runner]`, `[find]` and `[files]`.

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
| `tftp_test.cpp` | The TFTP packets of ARINC 615A-3 section 5.3.2: the layout of each, octet mode in any case and nothing else, block numbers that wrap 65535 to 1, a WRQ acknowledged with block 0, the largest block (65464), error and option-acknowledgement packets, an encoder that never writes past a buffer too small for it |
| `tftp_options_test.cpp` | `blksize`, `tsize`, `timeout`, `port` and `checksum_<n>` with each side of their limits and every malformed value, names in any case, unknown options ignored, repeats refused, the option builder, and the WAIT and ABORT messages: the standard's own `WAIT:50`, all 65536 wait times and all 65536 status codes round-tripped |
| `find_test.cpp` | The FIND request and answer of Attachment 3 (the example's layout, 0x10 and NUL terminators, each field at its limit and one over, empty fields, the manufacturer code of three) |
| `protocol_files_test.cpp` | The twelve protocol files of section 6.4: exact layouts, round trips, the file length and version, the status codes each field may hold, the text length rule and its padding, the ratios, counts, user defined data, nested lists, and a buffer too small for every file |
| `arinc615a_golden_test.cpp` | The codec's packets and files against the ones written independently ([below](#golden-payloads)) |
| `arinc615a_robustness_test.cpp` | Every 615A decoder over every truncation, bit and byte change, several changes at once, inserted and removed bytes, random bytes and random bytes behind a valid beginning, under the sanitizers; whatever is accepted must encode back to bytes that decode to the same thing |
| `tests/engine/tftp_transfer_test.cpp` | `TftpTransfer` on its own: two of them over a link in memory (a file read and written at 12 sizes and several block sizes, the largest block, block numbers across the 65535 to 1 wrap in both directions with 65 541 blocks, every datagram duplicated and the sender's block counted (the Sorcerer's Apprentice fix), 120 runs with loss, duplication and reordering from fixed seeds that either deliver the file exactly or time out, reordering by up to 900 ms, the two second time-out and one retry to the datagram, lost ACKs made up for by the retry and three losses not), and one transfer at a time with every datagram written out (WAIT as an answer to a request and as an ordinary error in the middle, ABORT from the peer with each code and our own, error packets, the options of our request and of the OACK we send and accept, an OACK that breaks the rules refused with error 8, a size that is not what arrives, the checksum verdict for each algorithm and a wrong one that does not fail the transfer, repeated and stray and garbage datagrams, a block too long, the dally, repeated requests, a full disk, a file that cannot be read, a request that does not fit its buffer) |
| `tests/engine/operations_test.cpp` | `DataLoader` with every datagram written out: the request for each operation's initialization file and its options, requests that cannot be made, the time-outs to the nanosecond (2 s and one retry, the DLP retry, 13 s from the end of the initialization file and from the end of each status file, an exception timer longer and shorter than the time-out), requests refused with the right error (before the operation is under way, another operation's file, no slot, from another host), a repeated request, a newer request replacing an old one, status counters (repeats, out of order, gaps, the wrap), an invalid status file, the abort and its confirmation, a protocol version that is not A4 |
| `tests/engine/data_loader_test.cpp` | Whole operations through the runner on the fake platform against the simulated target: an Information with and without status files, an Upload of a media set built with `build_media_set` (also 150 000 bytes in 64 byte blocks, a short load, two files of one name told apart by the checksum option, a file not yet available, a file refused), media defined and operator defined Downloads (a wrong checksum, no checksum, a file nobody asked for, a missing file, a sink that fails, the choice made later or not at all), a denied or unsupported operation, WAIT, a silent target, the port option, status files lost and repeated, 13 s of silence, the exception timer, the operator's abort in an upload and a download, a target's abort, a protocol version that is not A4, 24 seeds each of an upload and a download over a network that loses, repeats and reorders, a network that loses 45% (it must end, not hang), and that one seed is one history |
| `tests/engine/find_client_test.cpp` | `FindClient` (the request bytes, the three second window to the nanosecond, what is ignored, repeats, a concentrator's second identity, 64 targets) and `Runner::find` on the fake network (several targets, one that answers twice, one too late, addresses given for AFDX, a lossy network, a port that is taken) |
| `tests/engine/directory_files_test.cpp` | `DirectoryFileSource` and `DirectoryFileSink` on the fake file system: names that are safe, reading in blocks, names refused (paths, `..`, separators), the limit on open files, a file that appears only when complete, nothing left for a failed transfer, never overwriting, a full disk |
| `tests/engine/mini_tftp.cpp`, `sim_target.cpp`, `rig.hpp`, `transfer_pair.hpp` | Not tests: the simulated target, with its own TFTP (`mini_tftp`) that shares no code with the engine, and the helpers the tests above use ([below](#the-simulated-target)) |
| `paths_test.cpp` | `adl::platform::executable_path` names the running test program |
| `memory_test.cpp` | `Arena`, `BlockPool` and `StaticStorage`: alignment, exhaustion as a null result and never a write past the end, marks and rewinding, blocks that do not overlap and come back in any order, a block that is not the pool's being refused |
| `platform_types_test.cpp` | Error codes and their names, `Instant` arithmetic that saturates instead of wrapping, the protocol's largest figures fitting a `Duration`, IPv4 addresses read and written (and the bad ones refused) |
| `fake_platform_test.cpp` | The fake itself: the clock moves only when told, events run in time order, a latency, a loss profile that is the same for the same seed, a filter that drops, duplicates and corrupts chosen datagrams, jitter that reorders and loses none, an agent standing in for a target, two hosts on one network, broadcast, a receive buffer that overflows, the in-memory file system, a memory budget, threads that run when joined |
| `tests/conformance/*` | The platform contract, each check run against the fake **and** the real host this build selected. Clock: never backwards, sleeps at least what was asked and not wildly more, a wall clock that is plausible or `unsupported`. Memory: zeroed, aligned, writable, independent, refused with the right reason, `resident` granted or refused usefully. Files: a file written in pieces is read back at any offset (a short count only at the end), a file across the largest TFTP block size (65464), exclusive creation, directories, listing (and changing the directory while listing), rename over a file, a file that outlives its removal, paths the interface refuses (empty, backslash, NUL), non-ASCII names, a full disk, a closed file. Threads: join, destroying a thread waits for it, every priority level starts a thread and `normal` is always applied, a mutex, a semaphore's count, ceiling and time-out. Queues: whole messages in order, a full queue, a ring that wraps, bad sizes, caller-supplied storage. Network: a datagram of each TFTP-relevant size arrives whole with its source, order, time-out, a second bind refused, a port shared on request, an address that is not the host's, waiting on several sockets, truncation, the maximum datagram, a send to a closed port not upsetting the sender, broadcast needing permission. Real host only: threads contending on a mutex, a queue between threads under backpressure, a thousand datagrams echoed over real UDP, one thread serving five sockets with `wait_readable`, a datagram arriving from another thread while a receive waits |
| `tools.write_payloads*` (CTest) | `adl_write_payloads` writes every scenario to a fresh directory and verifies it (the setup test `tools.write_payloads.clean` empties the directory first, since the program never overwrites), `--list` works, and an unknown scenario or a missing output directory is refused |
| `tests/platform/check_fragments.cmake` | `platform.fragments`: each fragment satisfies the platform contract, on whichever host this runs |
| `tests/platform/check_selector.cmake` | `platform.selector`: configures the project with `ADL_TARGET_PLATFORM` set, and checks that a platform with no code is refused with its reason, that QNX is refused until `ADL_ACCEPT_UNBUILT_PLATFORM` is on and then configures, and that a name that is not a platform lists the ones that are |
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

`tests/golden/make_golden_615a.py` does the same for ARINC 615A-3: the TFTP requests,
option acknowledgement, ACK, DATA and error packets (including `WAIT:50` and
`ABORT:1004`), the FIND request and answer, and every protocol file, written from the
tables of section 6.4, Attachment 3 and the layouts of RFCs 1350 and 2347, sharing no code
with the library. Its output is `tests/golden/golden_615a.hpp`, committed, and
`arinc615a_golden_test.cpp` builds the same packets and files with the library (through
`tests/support/sample_615a.hpp`, which mirrors the script input for input) and compares
bytes, and reads the independent bytes back. If the format changes on purpose, change both
and run `python tests/golden/make_golden_615a.py`. It shares the choices
[the codec page](arinc615a-codec.md#places-the-standard-is-unclear) lists, so it cannot show
that they match another implementation. Appendix G's packet examples are figures with no
bytes in them, so there is nothing from the standard to check the TFTP layouts against beyond
the `WAIT` and FIND examples and the RFCs.

## Does the suite fail when the code is wrong?

It was checked by breaking the library on purpose and running it: a wrong pointer
in the header, file resolution that ignores the CRC, check characters added where
they are exclusive-ored, a Load CRC over the wrong part of the header, a Load
Check Value that starts early, and text fields not padded to a word. Each was
caught, the last by 53 test cases.

The 615A codec was checked the same way, with 25 deliberate breaks, one at a time: a block
number that wraps to 0, another transfer mode, a WAIT text without its colon, lower-case
ABORT digits, block size limits one off, a FIND terminator, the FIND field limits, a
different version, a text length without its terminator, a left-adjusted ratio, a file
length that leaves out its own four bytes, the fields of `.LUS`, `.LCS` and `.LNR` swapped
or off by one, a status code allowed in the wrong field, a decoder that ignores the file
length or the version, an empty required text, a checksum of any length, case-sensitive
option names, an oversized DATA block and an empty OACK. Each was caught. (One was not at
first: the FIND hardware identifier limit, because the test took its figure from the same
constant the library did. The test now writes the limits out.)

## Adding a test

Put a `TEST_CASE` in the file for the thing it tests, or add a file under
`tests/unit/` and list it in `tests/CMakeLists.txt`. Name the case as a
sentence about behaviour, tag it, and when the standard says what the answer is,
use the standard's value and say where it comes from. A test of one platform's
code goes in `tests/platform/` (the platform section is the one place the
isolation check allows OS headers). Helpers for building byte strings, damaging a
file and putting its CRC right again are in `tests/support`.

A bug fix comes with the test that failed before it.

## The fake and the conformance suite

The platform layer is tested by comparing two implementations of one contract. The
fake (`tests/fake`, [described here](platform-abstraction.md#the-fake-platform))
runs in memory on a clock that moves only when told to, so the engine's tests
need no socket and no real second; the conformance suite then checks that the fake
and the real host of each platform behave the same way, so that those tests mean
something about the real one. A conformance check is written with
`GENERATE(HostKind::fake, HostKind::real)` and `HostUnderTest`
(`tests/conformance/host_under_test.hpp`), which hands it a host and a scratch
directory; a check that needs real concurrency is written for the real host only
and tagged `[real]`. A port runs the suite on its target as its first test.

## The simulated target

The engine's operations are tested against a simulated 615A target (`tests/engine/sim_target.hpp`), an
`adl::fake::Agent` attached to the fake network at an address of its own, with the loader on another. It
behaves as sections 5.4 and 6.3 say a target does: it serves the initialization file, writes status files
as a heartbeat (counters and all), reads the load's files from the loader (an upload), writes the files and
the lists of a download, writes `.LCL`, and answers FIND. A `SimPlan` makes it slow, silent, denying, aborting,
waiting, refusing options, speaking another protocol version, losing or repeating a status file, sending a wrong
checksum, announcing an exception timer, or asking for a file that was not offered. Its TFTP is
`MiniTransfer` (`tests/engine/mini_tftp.cpp`): lock-step blocks, a two second time-out and one retry, written
plainly and sharing no code with the engine, so that the engine is run against a second reading of RFC 1350
and section 5.3.2 and not against itself. It does use the codec for the packets, which the golden tests
check against a writer of their own.

Everything runs on the fake clock, which moves only when told to, so a transfer of thirteen seconds of
silence takes a few milliseconds, and the same seed is the same history on every platform (the generator is
SplitMix64, not a standard library's distribution). The network's `LinkProfile` (latency, jitter, loss,
duplication, corruption) is how loss, repeats and reordering are made; the target's own faults are in the plan.
After the runner returns the rig advances the clock a second more, so that the target receives the loader's
last datagram, and `Rig::ended` is the time the runner stopped.

The media set the Upload loads is built with `adl::arinc665::build_media_set` from the sample load
(`adl::samples`), and served by a `FileSource` over its members that tells two files of one name apart by
the CRC in the request, as a header file's entries would. Because the target is told the files to read and
what to expect of each, it checks every byte it reads.

## Known gaps

- Nothing in the 615A codec or engine has been checked against another implementation of the
  protocol, a target or a loader; [the choices the codec made](arinc615a-codec.md#places-the-standard-is-unclear)
  and [the engine's](arinc615a-coverage.md#places-the-standard-is-unclear) are the first things to confirm.
  The simulated target has a TFTP of its own, but it is the same reading of the standard.
- No test runs a 615A operation over real sockets: the runner is tested on the fake platform, and the
  platforms' sockets by the conformance suite. macOS has not run any of the engine's tests except in CI.
- The codec and the engine are meant for a build without exceptions and RTTI, and no CI job builds them that
  way. (They compile that way under g++ 15 with `-fno-exceptions -fno-rtti`, checked once by hand.)
- The golden payloads come from the same reading of the standard as the library.
  Nothing has yet been checked against a load or a media set written by another
  tool.
- Appendix L's random reference files and its Ethernet frame are not in the
  standard.
- No tool other than this one has read the trees `adl_write_payloads` writes.
- The macOS platform sources have not yet been run on a Mac: the first CI run of
  the platform layer is their first test. (The Linux ones have, in a container
  with GCC 15 under the sanitizers.)
- QNX's sources have never been compiled, and VxWorks, FreeRTOS and Zephyr have
  none; the conformance suite is how a port will find out where it differs.
- The conformance suite cannot show a data race: the fake runs a thread when it is
  joined, and the real-host checks are a handful of contended runs, not a proof.
- A real allocator is not asked for an impossible amount of memory under the
  sanitizers, which abort the process instead of returning null; `resource_exhausted`
  is tested on the fake.
