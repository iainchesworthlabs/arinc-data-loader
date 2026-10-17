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
