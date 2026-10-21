# Changelog

*For people following what has shipped. The project overview is the
[README](README.md).*

All notable changes to this project are documented in this file, in the
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. This project
adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

Changes that are waiting for the next release are kept as small files in
[`changelog.d`](changelog.d/README.md), one per change, and are assembled into this file
when a release is cut ([docs/releasing.md](docs/releasing.md)).

## [0.1.0] - 2026-10-21

The first preliminary release. It rebuilds an earlier Visual Studio project (a Win32 static library
for ARINC 665, an empty one for ARINC 615A, and a C++/CLI unit test project) as a C++23 CMake
project: an ARINC 665 library, an ARINC 615A library, a Qt 6.12 data loader, a media set generator
and a mock target, for Windows, Linux and macOS. It has not been run against a real target. The
packages are not code-signed for any operating system's store; see
[Verifying a release](docs/verifying-releases.md) for the checks that are available.

### Known limitations

- **Nothing has been checked against a real target, a real loader or another tool's media
  sets.** The protocol and the file formats are tested against simulated targets, the mock target
  and independent writers of the same tables. See
  [the user's guide](docs/user-guide/files-and-limits.md#what-has-not-been-checked).
- Only Windows (MSVC) has been used by hand. Linux and macOS are built and tested in CI, and their
  packages are started without a display by the release workflow.
- The three programs are available in eight languages (English, French, Spanish, German, Yiddish,
  Hebrew, Simplified Chinese and Japanese). The seven translations are machine-drafted first
  versions that no speaker has read. The ARINC 665 findings, the protocol log lines and what a
  target itself sends stay in English ([Translations](docs/translations.md)).
- The Zephyr and FreeRTOS ports are experimental. The platform and engine tests pass on simulators
  and under QEMU, nothing has run on a board, and neither is in any package
  ([what was and was not verified](docs/platform-abstraction.md#the-freertos-and-zephyr-ports)).
- On Windows the Debug presets run under AddressSanitizer, which stops the system file dialogs
  ("Choose...") from working. Use a Release build, or the package, to use the windows.

### Added

**The programs** ([user's guide](docs/user-guide/index.md))

- **The data loader, `adl_app`** ([docs/app.md](docs/app.md)): a Qt 6.12 (Qt Quick) window. It reads and
  checks a media set folder (its part number, its loads, and any departure from the standard), finds
  targets with FIND or takes an address you type, and runs Information, Upload and Download. In an
  Upload you tick the loads to send and see a progress bar and the percentage and status of each
  load. Download works in both modes and writes into a new `DNLD_DATA_<THW_ID_POS>_<number>` folder.
  Each operation can be aborted and has an activity log and a receipt. If a media set is changed
  after it was checked, the loader refuses to upload it. Preferences are saved, and a first-run
  screen sets the defaults. Not run against a real target.
- **The media set generator, `adl_media_gen`** ([docs/media-gen.md](docs/media-gen.md)): makes an
  ARINC 665 media set from files, as the folder tree the loader opens, and reads it back to check it.
  It writes next to the final folder and renames it into place only after the read-back, so a full
  disk or a cancel leaves nothing half-written.
- **The mock target, `adl_mock_target`** ([docs/mock-target.md](docs/mock-target.md)): an ARINC 615A
  target that runs over UDP sockets (FIND, Information, Upload, both Downloads, and failures you can
  switch on), for trying the loader. The setting **This computer only (127.0.0.1)** keeps it on the
  loopback interface, which does not raise a firewall prompt.
- **`adl_write_payloads`**: writes sample media sets (the golden scenario and the ARINC 615 and
  Boeing 777 layouts of Appendix F) to a folder and checks them after writing, so other loaders can be
  tried on them.
- **A shared look and feel** ([docs/design-system.md](docs/design-system.md)): the three programs use
  the same controls, words and layout, in a light and a dark scheme. Status is shown by a symbol as well
  as by colour. The windows fit a 1366 by 768 display, and all three can be used from the keyboard
  alone, with a visible focus outline and accessible names.
- **Eight languages** ([docs/translations.md](docs/translations.md)): chosen in Preferences (the
  loader) or in the header (the other two), applied to the open window at once and saved. Counts use
  each language's plural forms. Hebrew and Yiddish mirror the whole window and use the bundled Noto
  Sans Hebrew typeface. The seven translations are machine-drafted and unreviewed; please report
  wording that is wrong.
- **Settings in another place**: set `ADL_SETTINGS_DIR` to a folder and the programs read and write an
  ini file there instead of the registry, the property list or the configuration file of their
  platform. `--smoke-test` draws a window once and exits with code 0.
- **Third-party licences**: About > Licences in each program, and `NOTICES.txt` and `LICENSE.txt` in
  every package. They cover Qt (LGPL, with the offer of its source), the Archivo and Noto Sans Hebrew
  typefaces, and the runtime files Qt's deployment tool adds ([docs/licensing.md](docs/licensing.md)).
- **Firewall helpers** in `tools/network-access/`: a Windows Firewall rule script (UDP only, Private
  profile, local subnet, per program path), an AppArmor profile and installer for Linux, and a macOS
  application firewall helper ([Installing](docs/user-guide/installing.md#what-the-firewall-asks)).

**The libraries**

- **`adl::arinc665`** ([docs/arinc665-coverage.md](docs/arinc665-coverage.md)): the file formats of
  ARINC 665-3. Loads (header, data and support files), batch files, and media sets with `LOADS.LUM`,
  `FILES.LUM` and `BATCHES.LUM`, written and read with their lengths, pointers and CRCs checked; media
  labels; part numbers and their check characters; and the five check values (CRC-8, CRC-16 and
  CRC-32 as compile-time tables, MD5 and SHA-1). Departures from the standard's recommendations are
  reported as findings and are not refused.
- **`adl::media_tree`**: writes a media set to a folder tree (`member_001`, ...) and reads one back,
  mapping the standard's backslash pathnames to native paths. It refuses names that cannot be files on
  every platform, writes nothing if anything is invalid, does not write into a member folder that
  already has files in it, and does not follow links.
- **`adl::arinc615a`, the codec** ([docs/arinc615a-codec.md](docs/arinc615a-codec.md)): the TFTP
  packets with the options of RFCs 2347 to 2349 and 615A's `checksum_<n>` and `port`, block numbers
  that wrap from 65535 to 1, WAIT and ABORT, FIND, and the twelve protocol files of section 6.4. It does
  not allocate memory, throw exceptions or use RTTI. The places where the standard is unclear, and what
  was chosen, are listed in the page.
- **`adl::arinc615a`, the engine, and `adl::arinc615a_runner`**
  ([docs/arinc615a-engine.md](docs/arinc615a-engine.md),
  [docs/arinc615a-coverage.md](docs/arinc615a-coverage.md)): the data loader's side of ARINC 615A-3 as
  state machines that are given the time and the bytes that arrived and return the bytes to send. They
  contain no socket, thread, clock or file system. A TFTP transfer in both roles and both directions;
  the Information, Upload (short and full load) and Download (media defined and operator defined)
  operations with their status counters and time-outs; and FIND. The runner opens the sockets and
  drives the engine through `adl::platform`. Not checked against another implementation.
- **`adl::platform`** ([docs/platform-abstraction.md](docs/platform-abstraction.md)): interfaces for
  files, memory, threads with an abstract priority scheme, mutexes and semaphores, a monotonic clock,
  IPv4 UDP sockets that wait on several at once, and bounded message queues. The platform is chosen in
  CMake, not in the source ([docs/platform-isolation.md](docs/platform-isolation.md)). Windows, Linux
  and macOS implement the interfaces. Failure is returned as `std::expected`; nothing throws.
- **Other platforms**: Zephyr and FreeRTOS (experimental, see the limitations), QNX (code that has
  never been compiled), the Green Hills INTEGRITY platforms `ghs508` and `ghs178b` (not built,
  [docs/platform-ghs.md](docs/platform-ghs.md)), and VxWorks (a design only). `ADL_TARGET_PLATFORM`
  names the platform of a cross build. The top-level project refuses an unbuilt platform unless
  `ADL_ACCEPT_UNBUILT_PLATFORM` is set, and refuses Zephyr and FreeRTOS, whose images are built by
  projects of their own, and says where.
- **Tracy 0.14.1 profiling** ([docs/profiling.md](docs/profiling.md)), as an opt-in build
  (`-DADL_ENABLE_TRACY=ON`). It is not in any release.

**Packages and releases** ([docs/releasing.md](docs/releasing.md))

- **One package per desktop**: a zip for Windows (x86-64), a tar.gz for Linux (x86-64, with desktop
  entries and icons) and a disk image for macOS 14.4 or later (Apple silicon, signed ad hoc). Each holds
  the four programs and the Qt files they need to start on a machine without Qt. A **development
  package** holds the libraries above, their headers and a CMake package (`find_package(adl)`).
- **Checksums, signatures, attestations and a bill of materials**: every file is listed in `SHA256SUMS`,
  signed with a PGP release key, and given a GitHub build-provenance attestation. The software bill
  of materials is in SPDX 2.3 and CycloneDX 1.6, with the Qt kit's own SPDX documents. To check a
  download, see [Verifying a release](docs/verifying-releases.md). The key is in
  `docs/release-signing-key.asc` and `SECURITY.md`.

**Build and tests** ([docs/building.md](docs/building.md), [docs/testing.md](docs/testing.md))

- **A CMake build** for Windows, macOS and Linux, with presets for MSVC, GCC and AppleClang, warnings
  as errors, AddressSanitizer in Debug, an optional coverage build, and vcpkg as a submodule. The
  applications are built by default on the desktops when the Qt kit is found. Several clones and
  worktrees can share one vcpkg checkout by setting `ICL_VCPKG_STORE`.
- **Tests in three labelled tiers** (`unit`, `integration`, `system`); a Qt Quick Test suite that
  drives the window; and a coverage requirement of 90% of lines and 80% of branches of the portable
  code. The tests compare the libraries' output with independent writers of the same formats
  (`tests/golden`), run against a fake platform that can lose, duplicate, corrupt and reorder
  datagrams from a seed, and run the same conformance checks against each real platform. A loopback
  test runs the loader's runner against the mock target over 127.0.0.1.
- **Checks on every change**: formatting, platform isolation, commit and pull request text, and
  dependency review. **Nightly**: clang-tidy, MSVC Code Analysis, a Clang ASan/UBSan run and CodeQL
  ([docs/static-analysis.md](docs/static-analysis.md), [docs/ci.md](docs/ci.md)).

**Documentation**

- **A user's guide** ([docs/user-guide](docs/user-guide/index.md)) with 64 pictures of the windows, in
  light and dark. It covers what ARINC 615A and 665 are, installing and starting the programs, a
  ten-minute demonstration with the three programs on one machine, and a page for each task. A link
  checker fails the build on a broken link or a missing picture.

### Changed

These correct the earlier Visual Studio project's code. Anyone holding data made by it should know them.

- The CRC-32 follows the standard: input and output are not reflected, and the check value is
  `0xB6B5EE95`. The earlier code described it as reflected, with the check value `0xCBF43926`, which is
  a different CRC.
- The part flags follow the standard: the least significant bit is set for a *download* part. The
  earlier code named it as an upload bit.
- `CONFIG.LDR` and `EXCONFIG.LDR` are support files, as ARINC 665-3 section 2.2.3.3 has them, not
  data files.
- Failures are returned (`std::expected`) and not thrown. The singleton and the factory that registered
  itself during static initialisation are gone.

### Removed

- The Visual Studio solution, project, test-settings and `.vsmdi` files, and the C++/CLI test project.

### Security

- File names that a target chooses in a download, and that a media set or the mock target would store,
  are checked by one rule on every system: no colon (an NTFS stream), no device name such as `NUL`,
  `CON` or `COM1`, no trailing dot or space, and no separator ([the loader](docs/app.md)).
- The CI builds do not rely on Qt's download server for the integrity of the Qt kit. Every archive is
  checked against a SHA-256 pinned in the repository before it is unpacked, and the Python package that
  unpacks it is installed from a hash-locked requirements file.

[Unreleased]: https://github.com/iainchesworthlabs/arinc-data-loader/compare/v0.1.0...develop
[0.1.0]: https://github.com/iainchesworthlabs/arinc-data-loader/releases/tag/v0.1.0
