# Roadmap

What is built, and what is planned, in the order it is intended to be done.

## 1. The foundation (done)

- A CMake build for Windows, macOS and Linux, with presets, vcpkg, Catch2,
  sanitizers, coverage and warnings as errors.
- The platform section: the operating system is chosen by CMake, with no `#ifdef`
  anywhere ([platform isolation](platform-isolation.md)).
- CI and the repository rules: gitflow, no attribution in commits or PRs,
  formatting, platform isolation ([CI](ci.md)).

Not yet checked: the Linux and macOS legs (they are written, and have not run),
and the repository rulesets (they cannot be applied until the repository is
public, or on a paid plan).

## 2. ARINC 665 (written, not yet checked against another tool)

The data format of `docs/specs/665-3.pdf`, all of it: loads, batch files, media
sets and their lists, labels, part numbers, CRCs and check values. It is written,
read and verified, and compared byte for byte with a second implementation
written from the standard's tables. See [ARINC 665 coverage](arinc665-coverage.md).

Media sets can be put on a disk: `adl::media_tree` (`src/media_tree`) writes the
members of a set to a directory tree, one directory per member, and reads one
back, mapping the standard's backslash pathnames to native paths. It uses
`std::filesystem` only, so it is not platform code, and `adl::arinc665` stays free
of file I/O. `adl_write_payloads` (`src/write_payloads`) writes the sample media
sets of `adl::samples` to a directory (the golden scenario and the ARINC 615 and
Boeing 777 layouts of Appendix F), for trying other loaders and for the 615A work
to load.

What is left:

- **Check it against another tool.** The second implementation shares my reading
  of the standard, so the places the standard is unclear
  ([listed here](arinc665-coverage.md#places-the-standard-is-unclear)) need a load
  and a media set from an established tool, read here, and ours read there. The
  order the Load CRC takes the files in (header, data, support) is settled as a
  decision, and is still the first to confirm against a real tool.
- The Appendix L reference files that are not described exactly (T05 to T10).

## 3. ARINC 615A

`src/arinc615a`, from `docs/specs/615A-3.pdf`: the protocol engine over TFTP and
UDP, with the Find, Information, Upload and Download operations. It loads what
`adl::arinc665` makes. Built in three pull requests, all done:

1. **The platform abstraction (done).** Interfaces for files, memory, threads
   and priorities, a clock, UDP and message queues, implemented for Windows,
   Linux and macOS, with a fake platform and a conformance suite, and QNX,
   VxWorks, FreeRTOS and Zephyr designed for ([platform
   abstraction](platform-abstraction.md)). The transport is an interface, so the
   engine does not include a socket header and can be tested against a simulated
   target on the fake network.
2. **The TFTP codec and the 615A messages (done).** `adl::arinc615a`: the packets
   of RFC 1350 with the options of RFC 2347 to 2349 and the 615A ones (the checksum
   and port options, the WAIT and ABORT error texts), the FIND packets, and the
   protocol files (`.LCI`, `.LCL`, `.LCS`, `.LUI`, `.LUR`, `.LUS`, `.LND`, `.LNR`,
   `.LNS`, `.LNO`, `.LNL`, `.LNA`), written into caller-supplied buffers and read
   back as checked views, with no allocation, exceptions or RTTI, and tested the same
   way as the 665 formats, against an independent writer ([the
   codec](arinc615a-codec.md) lists the places the standard is unclear and what was
   chosen). Not checked against another implementation yet.
3. **The engine, the runner and the coverage page (done).** The sans-I/O core in `adl::arinc615a`
   (a TFTP client and server with the options, WAIT and ABORT, the Sorcerer's Apprentice fix and the
   time-out and retry of Attachment 4; the Information, Upload and Download operations with status
   files, the 13 second time-out, the exception timer, the abort paths and a protocol version check;
   FIND as a client), `adl::arinc615a_runner` over `adl::platform`, and an end-to-end test against a
   simulated target on the fake platform: a full load of a media set built with `build_media_set`
   and a download back, with loss, duplication, reordering, WAIT, ABORT and silence, from fixed seeds
   ([the engine](arinc615a-engine.md), [the coverage page](arinc615a-coverage.md)). Not checked
   against another implementation, a loader or a target, and not run over real sockets.

What is left of the 615A work:

- **Check it against a real target or loader.** Both ends of every test are my reading of the standard.
  The choices that are most likely to differ are listed in
  [the coverage page](arinc615a-coverage.md#places-the-standard-is-unclear) and
  [the codec page](arinc615a-codec.md#places-the-standard-is-unclear), the text length of a protocol file
  first.
- **An integration test over loopback with real sockets** (a target on the real host in a second thread),
  so that the runner is run through the real platforms, on all three desktops, in CI.
- **A CI job that builds the library with `-fno-exceptions -fno-rtti`**, which it is written for.
- **A target side** (answering `.LCI`, writing status files), if a target is to be built and not only
  loaded.
- Choosing, on the application side, the `DNLD_DATA_<THW_ID_POS>_<number>` directory (5.4.4.3) and
  when to run FIND (Attachment 3).

What is left from stage 1: the macOS leg has not yet run; the embedded ports are
unbuilt or designs.

## 4. The application

A Qt 6.12 application in `src/app` (Qt Quick), a thin layer over the libraries. Qt is not
from vcpkg: it is the prebuilt kit found through `QT_ROOT_DIR` (the shared
toolchain at `C:\Repos\common\qt\6.12.0`), by `cmake/FindQt6.cmake`; the platform
fragments declare where each system keeps Qt and the two hooks the application needs
(`adl_platform_configure_app`, `adl_platform_deploy_qt_runtime`). CI installs the kit with a pinned
aqtinstall on the three desktop legs, and builds the application with `-DADL_BUILD_APP=ON`. Built in
slices; [the application](app.md) says what each does.

1. **The skeleton and the engine bridge (done).** The engine on a thread of its own with a queued
   signal route to the window and an atomic abort, tested headless against the simulated target; a
   window that reads a media set from a directory (shown, not yet used), runs FIND, takes a target from
   the list or by hand, and runs an Information operation with an abort button, the target's status
   files and notices in an activity log, and its hardware and parts. The window is loaded offscreen in
   the tests. Not run against a real target, and the Linux and macOS legs had not run it when this was
   written.
2. **Upload (done).** The loads of the media set are ticked in the window and loaded to the target: a
   `FileSource` over the media set's directory tree (an index made from `FILES.LUM`, files told apart by
   the CRC in the request, read through the platform's file system), an `UploadOperation` from the ticked
   loads, progress for the list and per load from the upload status files, the abort button, and the
   outcome (a load the target failed makes the operation a failure). Tested through `LoaderModel` and the
   window against the simulated target. Not run against a real target.
3. **The operator's whole job (done).** The window around the two operations: preferences that persist
   (the last media set restored at start-up, a default target, the loader's control port, whether FIND
   runs before an operation with no chosen target, whether a chosen set is checked at once, the log
   capacity), properties for the media set (identity, loads, every file of `FILES.LUM`, the findings) and
   for the chosen target (its FIND answer), a validation gate — reading the set is checking it, an
   unreadable set cannot be uploaded, findings are warnings, files changed on disk are caught by a
   re-check — and error handling that says what failed and, where it is known, what to do about it.
   Tested through `LoaderModel` and the window ([the application](app.md)).
4. **The media set generator (done).** `src/media_gen`, a second Qt application: pick files from disk,
   describe the load and the media set, and generate the directory tree the loader opens — built with
   `adl::arinc665` and `adl::media_tree`, verified by reading it back, and read in the tests through the
   loader's own summary ([the generator](media-gen.md)).
5. **The mock target (done).** `src/mock_target`, a third Qt application: a 615A target over real
   sockets, so the dialogues can be demonstrated end to end without an embedded box — FIND, Information,
   Upload (with checksums checked and the files stored to disk), both Download modes, aborts, and the
   failure injections that show the loader coping ([the mock target](mock-target.md)). Its engine runs in
   the tests against the loader's real engine on the fake platform; the loader's TFTP and the mock
   target's (`adl::arinc615a_mini`, extracted from the tests) share no code.
6. **Download in the window, the loopback test, and the dark/keyboard pass (done).** The Download
   operation in both modes (media defined, and operator defined through the engine's deferred choice,
   "Choose none" cancelling the way the standard says), the files written to a fresh
   `DNLD_DATA_<THW_ID_POS>_<number>` directory ([the application](app.md)); an integration test that runs
   the loader's runner over real sockets against the mock target's engine on 127.0.0.1 — Information,
   Upload byte for byte, both Download modes and an abort, as `loopback.integration`
   ([the mock target](mock-target.md)); and a dark-scheme, keyboard and accessibility pass over all three
   windows (menus and shortcuts, initial focus, the keyboard highlight of the generator's file list, both
   schemes rendered in the offscreen tests).

What is left of the application:

- Remembering the last target and download directory; translations.
- FIND over real sockets in the loopback test: the standard fixes FIND to port 1001 on both ends, and two
  sockets sharing a port on one machine is not reliable on every platform, so the loopback test covers the
  operations and names the target by hand; a FIND case may join it if the reuse behaviour proves portable.
- Packaging (section 7 below), which is where `macdeployqt` comes in.

## 5. Profiling

Tracy, as an opt-in build: `cmake/Tracy.cmake` and the `profiling` vcpkg feature
are in place, behind `ADL_ENABLE_TRACY`. What is missing is the zones, which
come with the first code worth profiling. Nothing that ships opens Tracy's
listener.

## 6. An embedded target

A build of the loader library for an embedded RTOS. The first concrete attempt
is the **Green Hills INTEGRITY support** ([the GHS review](platform-ghs.md)):
`ghs508` (INTEGRITY 5.0.8, on the shared POSIX sources) and `ghs178b`
(INTEGRITY-178B, a native port), both unbuilt, with `cmake/FindIntegrity.cmake`,
`cmake/FindIntegrity178.cmake`, the GHS toolchain files and presets. VxWorks
178C remains the design target it was. The platform layer is shaped for all of
them ([platform abstraction](platform-abstraction.md)): a design for VxWorks,
FreeRTOS and Zephyr, code for QNX and INTEGRITY that has never been compiled,
and a selector that takes `-DADL_TARGET_PLATFORM`. The structure is meant to
make each a new platform and not a new codebase:

- an implementation of each platform interface in `src/platforms/vxworks` (the
  fragment and the branch in `cmake/Platform.cmake` already exist), with a
  toolchain file for the cross compiler. First find out which APIs the 178C
  profile allows (sockets, a file system, `pthread`, dynamic allocation after
  start-up) and settle the priority plan; both are open;
- the libraries built to what that target allows. That probably means no
  exceptions, no RTTI and no heap after start-up, and each of those is a
  constraint `adl::arinc665` does not meet yet (the model types hold `std::string`
  and `std::vector`, and the library uses `std::expected` and `std::variant`,
  which need a standard library that has them). It already returns errors as
  values, computes its CRC tables at compile time, has no static initialisation
  and no `iostream`, which is the direction. The platform layer and the 615A
  engine are written to meet them: no exceptions, no RTTI, caller-supplied
  buffers, and a sans-I/O core (the 615A codec already is: it allocates nothing and
  throws nothing, though building it with exceptions and RTTI off is not yet a CI job);
- a CI job that builds the library for the target with those constraints on, so
  that they hold.

What the target's certification needs from the code is not decided here.

## 7. Around the edges

clang-tidy as a CI job, the Release presets in CI, a coverage threshold once the
figures have been looked at, packaging, and a `docs/` site.
