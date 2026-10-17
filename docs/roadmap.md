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
`adl::arinc665` makes. Built in three pull requests:

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
3. **The operations and state machines, and the coverage page (next).** The sans-I/O core
   (TFTP client and server, the Information, Upload and Download operations, FIND,
   the time-outs and retries of Attachment 4), a runner over `adl::platform`, a full
   load of a media set built with `build_media_set` to a simulated target and back,
   and `docs/arinc615a-coverage.md`.

What is left from stage 1: the macOS leg has not yet run; the embedded ports are
unbuilt or designs.

## 4. The application

A Qt 6.12 application in `src/app`, a thin layer over the libraries. Qt is not
from vcpkg: it is the prebuilt kit found through `QT_ROOT_DIR` (the shared
toolchain at `C:\Repos\common\qt\6.12.0`), by `cmake/FindQt6.cmake`. The platform
fragments already declare where each system keeps Qt and the two hooks the
application needs (`adl_platform_configure_app`, `adl_platform_deploy_qt_runtime`).
CI needs a Qt install step when this arrives.

## 5. Profiling

Tracy, as an opt-in build: `cmake/Tracy.cmake` and the `profiling` vcpkg feature
are in place, behind `ADL_ENABLE_TRACY`. What is missing is the zones, which
come with the first code worth profiling. Nothing that ships opens Tracy's
listener.

## 6. An embedded target

A build of the loader library for VxWorks 178C, and possibly other targets
after it. The platform layer is shaped for it ([platform
abstraction](platform-abstraction.md)): a design for VxWorks, FreeRTOS, Zephyr and
QNX, code for QNX that has never been compiled, and a selector that takes
`-DADL_TARGET_PLATFORM`. The structure is meant to make this a new platform and not
a new codebase:

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
