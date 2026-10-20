# Changelog

*For people following what has shipped. The project overview is the
[README](README.md).*

All notable changes to this project are documented in this file, in the
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. This project
adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

This is a rebuild of an earlier Visual Studio project (a Win32 static library
for ARINC 665, an empty one for ARINC 615A, and a C++/CLI unit test project) as a
C++23 CMake monorepo. Nothing has been released yet; this section is what the first
preliminary release (0.1.0) will contain.

### Known limitations

- **Nothing has been checked against a real target, a real loader or another tool's media
  sets.** The protocol and the file formats are tested against simulated targets, the mock target
  and independent writers of the same tables. See
  [the user's guide](docs/user-guide/files-and-limits.md#what-has-not-been-checked).
- There is no installer or package: the programs are built from source. Only Windows (MSVC) has
  been driven by hand; Linux and macOS build and test in CI.
- The loader is the only translated window, and only 148 of its 515 messages are translated in
  each of the seven non-English catalogues; the translations are a first, mechanical draft.
- On Windows the Debug presets run under AddressSanitizer, which stops the system file dialogs
  ("Choose...") from working; use a Release build for the windows.

### Added

- **Network and privilege gates.** The mock target has a **This computer only (127.0.0.1)** setting
  (`Config::bind_address`), and the loader binds loopback when its target is a loopback address
  (`RunOptions::local_address`): such sockets raise no firewall prompt, and the loopback integration
  test uses them, so it no longer stops an unattended run on a Windows prompt. A port that is refused
  with *permission denied* now says that a port below 1024 may need privilege.
  `tools/network-access/` has a Windows Firewall rule helper (`adl_firewall.ps1`: UDP only, Private
  profile, local subnet, per program path), an AppArmor profile and installer for Linux, and a macOS
  application-firewall helper; [Installing](docs/user-guide/installing.md#what-the-firewall-asks)
  explains the firewall, `CAP_NET_BIND_SERVICE` and SELinux.
- **A user's guide and pictures of every window** ([docs/user-guide](docs/user-guide/index.md)):
  what ARINC 615A and 665 are, installing and starting, a ten-minute demonstration with the
  three programs on one machine, and a page per task (make a media set, run the mock target, find
  a target, choose a media set, upload, download, the log and the receipt, preferences, languages
  and colour, keyboard, troubleshooting, where files go and the limits). The screenshots in
  `docs/images` are of the real windows and `tools/docs/capture_screenshots.ps1` regenerates them
  ([docs/screenshots.md](docs/screenshots.md)); a link checker (`tools/checks/check_doc_links.ps1`, the
  `docs.links` test) fails the build on a broken link or a missing picture.

- **Third-party licences in the applications.** About has a Licences button in all
  three applications that opens the notices this build carries: Qt under the LGPL
  (with the text of the licence and the offer of its source), the Archivo typeface
  under the SIL Open Font License, the runtime files Qt's deployment tool adds on
  Windows, and Tracy in a profiling build. `cmake/Notices.cmake` assembles
  `NOTICES.txt` at configure time from `notices/fragments` and the licence texts
  in `notices/licences`, embeds it in each executable and places it, with
  `LICENSE.txt`, beside them; `system.notices` checks the result.
  [docs/licensing.md](docs/licensing.md) says how to add a component.
- **The test suite is now three labelled tiers, with a UI suite and a coverage
  minimum.** Every CTest test is `unit`, `integration` or `system` (`ctest -L
  unit`, and the `check-unit` / `check-integration` / `check-system` targets),
  and the system tier includes smoke tests that start the three application
  programs as processes. `tests/ui` is a Qt Quick Test suite that loads the real
  `Main.qml` and drives its buttons, fields, check boxes and dialogs with Qt
  Quick Test's mouse and key events against the real model and the simulated
  target; it runs as `ui.tst_main_window` and `ui.tst_dialogs`. Coverage is
  gated at **90% of lines and 80% of branches of the portable code** by
  `cmake/CoverageReport.cmake` (the `coverage` target and the CI coverage job).
  [docs/testing.md](docs/testing.md) describes the tiers, the UI suite and the
  report.
- **A nightly static-analysis suite** ([docs/static-analysis.md](docs/static-analysis.md)):
  `static-analysis.yml` runs the pinned clang-tidy (with the Clang Static
  Analyzer, warnings as errors) over `src/`, MSVC Code
  Analysis (`/analyze /WX`) over the libraries, tests and the three Qt
  applications, a Clang ASan/UBSan build and test run, and an optional
  SonarQube scan; CodeQL moves to the same nightly schedule. The analyzers are
  not required checks and do not run on every commit. New presets
  `linux-gcc-tidy`, `windows-msvc-analyze`, `linux-clang-debug` and
  `linux-gcc-sonar` run the same work locally, and `ADL_ENABLE_CLANG_TIDY` /
  `ADL_ENABLE_MSVC_ANALYSIS` switch the analyzers in a build. `.clang-tidy` is
  now the gate CI holds, with each disabled check carrying its reason.
- **The two Green Hills INTEGRITY platforms, `ghs508` and `ghs178b`**
  (unbuilt): a port of the platform layer to INTEGRITY 5.0.8 on the shared
  POSIX sources, and a native port to INTEGRITY-178B (semaphores, the
  virtual-clock time-out pattern, ipflite UDP, a static memory provider), with
  `cmake/FindIntegrity.cmake`, `cmake/FindIntegrity178.cmake`, the GHS
  toolchain files and presets. The interfaces stay C++23; a GHS standard
  library that is short one of the C++20/23 headers the interfaces include
  gets a shim from `src/platforms/ghs/std/`, selected automatically per header
  by `cmake/GhsStdShims.cmake`.
  [docs/platform-ghs.md](docs/platform-ghs.md) is the review that shaped them,
  and names what is still open (the 178B application task and file-system
  model).
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
  through `QT_ROOT_DIR`); CI installs it on the three desktop legs. Not run against a real target.
- **The application: Upload** ([docs/app.md](docs/app.md)): tick loads of the media set that was read and
  load them to the target with an `UploadOperation`, with a progress bar and a figure for the list, the
  percentage and status of each load from the upload status files, the files the target reads in the log, and
  the abort button. The files are read from the media set's directory tree (`MediaSetIndex` from `FILES.LUM`
  and a `FileSource` over it, which tells two files of one name apart by the CRC in the target's request).
  Not run against a real target.
- **The application: the whole job, Download, and two more applications**
  ([docs/app.md](docs/app.md), [docs/media-gen.md](docs/media-gen.md),
  [docs/mock-target.md](docs/mock-target.md)): the loader gains preferences that persist, properties for the
  media set and the target, a validation gate (reading the set is checking it) and failure dialogs that say
  what to do, and the Download operation in both modes into a fresh `DNLD_DATA_<THW_ID_POS>_<number>`
  directory, with a dark scheme and a keyboard pass over all three windows. `adl_media_gen` generates a
  media set as the directory tree the loader opens and reads it back to verify it. `adl_mock_target` is a 615A
  target over real sockets (FIND, Information, Upload, both Downloads, and failure injections), on
  `adl::arinc615a_mini`, the second TFTP implementation extracted from the tests so that the two ends of a
  dialogue share no transfer code. `loopback.integration` runs the loader's runner against the mock target's
  engine over 127.0.0.1. Not run against a real target.
- **The window's minimum set** ([docs/app.md](docs/app.md)): one design-token file (`src/ui/qml/Theme.qml`)
  is the single source of truth for colour, type and geometry, and the window is built from a small
  component library over it (`AppButton`, `SegmentedControl`, `FocusRing` and the rest of `src/ui/qml`), as the sibling
  applications' windows are. A first launch opens on a first-run screen (a default target, the
  download directory, and what the window is for) that is stored once passed; the address and
  `<THW_ID_POS>` of the last operation are remembered and put back at start-up; and the strings are
  in a catalogue (`qt_add_translations`, an English `.ts`) with a `LanguageManager` that persists the
  language, follows the system locale and retranslates the running window. The QML is a module
  library of its own, so the window's offscreen test loads the same compiled QML the application
  does.
- **The window in eight languages** ([docs/app.md](docs/app.md)): catalogues for French, Spanish,
  German, Yiddish, Hebrew, Simplified Chinese and Japanese beside the English source, built by
  `qt_add_translations` into the window's resources and installed by `LanguageManager`; the
  right-to-left scripts set the application's layout direction (the layout of the window is not
  mirrored: no `LayoutMirroring` is set in the QML, a known defect) and the language preference is
  persisted. The text is a first, mechanical draft, to be reviewed by speakers; the window test
  installs each catalogue in turn and checks the retranslation and the layout direction.
- **The applications build by default on the desktops** ([docs/building.md](docs/building.md)):
  `ADL_BUILD_APP` defaults to on for Windows, Linux and macOS (the source side, and the only place
  Qt exists) when the Qt kit can be found, and off when `ADL_TARGET_PLATFORM` names a target or the
  kit is absent — so a desktop without Qt configures the libraries alone instead of failing, and
  `-DADL_BUILD_APP=ON` with no kit is still an error. The desktop presets therefore contain
  `adl_app`, `adl_media_gen`, `adl_mock_target` and their tests without a flag, and an IDE that
  does not run the labs' environment script finds the shared kit under `C:/Repos/common/qt` (the
  Windows fragment's search roots) rather than failing to generate. The configure output names the
  three programs, and the building page has the source-side/target-side matrix: the libraries, the
  host tools, the source-side applications and the (still planned) target-side application.
- **Tracy 0.14.1 profiling** ([docs/profiling.md](docs/profiling.md)): an opt-in build
  (`-DADL_ENABLE_TRACY=ON`, vcpkg's `profiling` feature) instruments ARINC 665 and the media tree,
  the 615A engine and runner, the application's engine jobs and media-set reads, the operator's
  actions, and the window's frame loop — frame marks, the frame interval and the render thread's
  time as plots, with the GUI, engine and render threads named. The `adl::profiling` names come
  from a header CMake generates, so no first-party file carries `#if` or `#define` and the same
  sources compile to nothing when the switch is off. The feature also installs Tracy's CLI tools,
  so a trace can be recorded and exported without the GUI profiler.
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
  engine. Not yet checked against another implementation; the loopback integration test runs it
  over real sockets against the mock target.
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
- **One design system for the three applications** (`src/ui`): the ICL Forge Modernist
  language applied to the loader, the generator and the mock target — the Signal palette,
  Archivo, zero radius, one control set, a fixed context rail and action bar, a first-run
  statement, a preflight and an operation receipt. The windows gained window-geometry
  persistence, a transactional Preferences dialog, a close-during-job question, target
  reconciliation on edit (`resultIsTransfer`/`resultWasDownload`), and the generator and
  mock target gained mode and check-value visibility. The Preferences page is a two-column modal
  with a window scrim, the About pages carry version, git describe, commit, branch, build target
  and a dirty-tree marker, and the QML resize and log-scroll costs were reduced. The analysis and
  before/after screenshots are in [docs/design-overhaul.md](docs/design-overhaul.md), the token
  reference in [docs/design-system.md](docs/design-system.md).

### Fixed

- A download directory chosen in the folder dialog was stored as a `file:///` URL and
  handed to the engine as a path; it is converted to a local path.
- The media set dialog's Loads and Files figures were fixed when the window was built;
  they follow the set that was read.
- "Stop and close" did nothing when the job ended while the question was open.
- The mock target could call into a link that its own session thread was destroying when
  Stop was pressed, and a download folder it could not list ended the program.
- Port numbers in the mock target's spin boxes no longer carry the locale's digit grouping
  ("1,001").
- Enter now presses the focused button as Space does; the segmented control keeps the
  keyboard focus on the segment it chooses and a click on the chosen segment does not clear
  it; error lines in the log use the accessible ink.
- The `coverage` target passes the `gcov` that matches the compiler, and the coverage gate
  fails when it measured too little to mean anything (gcovr reports 0 of 0 lines as 100%).
- The `check-*` targets fail when a tier has no tests; the branch-name check accepts the
  same one to three levels the ruleset does; Dependabot also covers the `install-qt` action.

### Changed

- CodeQL runs nightly, and not on every push or pull request: a full CodeQL
  build costs more than the rest of the per-commit matrix, and its alerts are
  triaged in Security -> Code scanning. The rest of the static analysis follows
  the same schedule ([docs/static-analysis.md](docs/static-analysis.md)).
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
