# Changelog

*For people following what has shipped. The project overview is the
[README](README.md).*

All notable changes to this project are documented in this file, in the
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. This project
adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

This is a rebuild of an earlier Visual Studio project (a Win32 static library
for ARINC 665, an empty one for ARINC 615A, and a C++/CLI unit test project) as a
C++23 CMake monorepo. Nothing has been released.

### Added

- **A CMake build for Windows, macOS and Linux**, with presets for MSVC, GCC and
  AppleClang, warnings as errors, AddressSanitizer in Debug, an optional
  coverage build, and vcpkg (a submodule) for dependencies.
- **`adl::arinc665`, the whole of ARINC 665-3's file formats.** Loads (header,
  data and support files), batch files, and media sets with `LOADS.LUM`,
  `FILES.LUM` and `BATCHES.LUM`, each written and read with its lengths, pointers
  and CRCs checked; media labels; part numbers and their check characters. A load
  is built from file contents and verified end to end, and a media set is built
  from loads and read back by the rules of section 3.2.4. Departures from the
  standard's recommendations are reported as findings rather than refused. See
  [docs/arinc665-coverage.md](docs/arinc665-coverage.md).
- **All five check values**: the 8, 16 and 32-bit CRCs of section 4.3 as
  compile-time tables, and MD5 and SHA-1, which the earlier code declared and
  never wrote. They are tested against the standard's own values, the Appendix L
  reference files, and the RFC and FIPS vectors.
- **Tests against a second implementation.** `tests/golden/make_golden.py` writes
  each file format separately from the standard's tables, and the tests compare
  the library's output with it byte for byte; payloads for the ARINC 615 and
  Boeing 777 layouts of Appendix F, for the limits of the field widths, and for
  damaged files.
- **`adl::arinc615a`, the codec of ARINC 615A-3** (`src/arinc615a`,
  [docs/arinc615a-codec.md](docs/arinc615a-codec.md)): TFTP packets (RRQ, WRQ, DATA,
  ACK, ERROR, OACK) with the options of RFCs 2347 to 2349 and the `checksum_<n>` and
  `port` options of 615A, block numbers that wrap from 65535 to 1, and the WAIT and
  ABORT messages; the FIND request and answer; and the twelve protocol files of
  section 6.4 with the status code table. Encoders write into a buffer the caller
  supplies and decoders return views over the bytes they were given, with every
  length checked, so nothing allocates, throws or needs RTTI. The places where the
  standard is unclear, and what was chosen, are listed in the page above. Tested
  against the standard's own numbers, round trips, every truncation and bit change,
  random bytes, and byte for byte against an independent writer
  (`tests/golden/make_golden_615a.py`).
- **The application, first slice** (`src/app`, [docs/app.md](docs/app.md)): a Qt 6.12 (Qt Quick) window
  to choose a media set directory (read and shown: its part number, loads and any differences from the
  standard), run FIND and take a target from the list or type one, and run an Information operation with an
  abort button, the target's status files and the engine's notices in an activity log, and the hardware and
  parts it reports. The engine runs on a thread of its own, reporting through queued signals and aborted
  through an atomic flag the runner polls. Built with `-DADL_BUILD_APP=ON` (Qt is the prebuilt kit, found
  through `QT_ROOT_DIR`); CI installs it on the three desktop legs. Not run against a real target. Download is
  not in the window yet.
- **The application: Upload** ([docs/app.md](docs/app.md)): tick loads of the media set that was read and
  load them to the target with an `UploadOperation`, with a progress bar and a figure for the list, the
  percentage and status of each load from the upload status files, the files the target reads in the log, and
  the abort button. The files are read from the media set's directory tree (`MediaSetIndex` from `FILES.LUM`
  and a `FileSource` over it, which tells two files of one name apart by the CRC in the target's request).
  Not run against a real target.
- **The 615A engine and runner** (`adl::arinc615a`, `adl::arinc615a_runner`,
  [docs/arinc615a-engine.md](docs/arinc615a-engine.md),
  [docs/arinc615a-coverage.md](docs/arinc615a-coverage.md)): the data loader's side of ARINC
  615A-3 as state machines that are told the time and the bytes that arrived and answer with the
  bytes to send, with no socket, thread, clock or file system inside, no allocation, exceptions or RTTI.
  A TFTP transfer in both roles and both directions (options, block numbers wrapping 65535 to 1, the
  2 s time-out with one retry, the Sorcerer's Apprentice fix, WAIT, ABORT, `tsize` and checksum checks
  that never fail a transfer for a bad checksum); the Information, Upload (short and full load) and
  Download (media defined and operator defined) operations with the status counters, the 13 s data load
  time-out, the exception timer, the DLP retry, and the abort paths of 5.4.5.2; FIND. The runner opens
  the sockets, waits on them from one thread and drives the engine over `adl::platform`; a file source
  and sink over a directory of the platform's files come with it. Tested against a simulated target
  on the fake platform (a full load of a media set made with `build_media_set`, downloads back, and loss,
  duplication, reordering, WAIT, ABORT and silence from fixed seeds) whose TFTP shares no code with the
  engine. Not yet checked against another implementation, or over real sockets.
- **`adl::media_tree`**: writes the members of a media set to a directory tree
  (`member_001`, `member_002`, ...), mapping the standard's backslash pathnames to
  native paths, and reads one back into `MediaMember`s. It refuses names that
  cannot be files on every platform, writes nothing if anything is invalid, never
  writes over a member directory that has files in it, and does not follow links.
  It uses `std::filesystem` only, so `adl::arinc665` stays free of file I/O and
  nothing platform-specific is needed.
- **`adl::samples` and `adl_write_payloads`**: the sample loads and media sets
  (the golden scenario and the ARINC 615 and Boeing 777 layouts of Appendix F),
  and a command-line program that writes them to a directory, verified after
  they are written, so that other loaders can be tried on them. The tests build
  from the same samples.
- **`adl::platform`**: the platform section, selected in CMake and not in the
  source (`docs/platform-isolation.md`).
- **The platform abstraction for the ARINC 615A loader**
  ([docs/platform-abstraction.md](docs/platform-abstraction.md)): interfaces for
  files and storage, memory (an arena, a block pool and a provider, chosen over
  `std::pmr`), threads with an abstract priority scheme, mutexes and semaphores,
  a monotonic clock, IPv4 UDP sockets that wait on several at once, and bounded
  message queues. Windows, Linux and macOS implement them (Winsock and Win32;
  POSIX, with the differences of Linux and macOS in one small file each). Failure
  is `std::expected`, nothing throws, and a platform with no heap can serve the
  handles from fixed slots.
- **A fake platform** (`tests/fake`): an in-memory clock, network, file system,
  memory and threads on a clock that moves only when told to, so the engine can be
  tested without a socket or a real second. The network loses, duplicates,
  corrupts and reorders datagrams from a seed or by a rule, and a simulated target
  can answer at an address.
- **Conformance tests** that run the same checks against the real host of each
  platform and against the fake, and loopback tests over real UDP.
- **QNX, VxWorks, FreeRTOS and Zephyr as named targets.** `ADL_TARGET_PLATFORM`
  names the platform of a cross build. QNX has code that has never been compiled
  (refused unless `ADL_ACCEPT_UNBUILT_PLATFORM` is on); the other three are
  designs only, and a build for them says so.
- **Repository rules as code**: gitflow branch names, no attribution in commit
  messages or pull request text, formatting, platform isolation, CodeQL,
  dependency review, and the rulesets that require them.
- **`cmake/FindQt6.cmake` and `cmake/Tracy.cmake`**, ready for the Qt 6.12
  application and Tracy profiling on the roadmap.

### Changed

- The CRC-32 follows the standard: input and output are not reflected, and the
  check value is `0xB6B5EE95`. The earlier code described it as reflected with
  the check value `0xCBF43926` (a different CRC).
- The part flags follow the standard: the least significant bit is set for a
  *download* part. The earlier code named it as an upload bit.
- `CONFIG.LDR` and `EXCONFIG.LDR` are support files, as ARINC 665-3 section 2.2.3.3
  has them, not data files.
- Failures are returned (`std::expected`), not thrown; the singleton and the
  factory that registered itself during static initialisation are gone; a
  checker is chosen by `IntegrityChecker::make`.

### Removed

- The Visual Studio solution, project, test-settings and `.vsmdi` files, and the
  C++/CLI test project.
