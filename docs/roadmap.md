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
UDP, with the Find, Information, Upload and Download operations. The transport
is an interface in `src/platforms/interfaces`, so that the engine does not
include a socket header and can be tested against a fake target. It loads what
`adl::arinc665` makes.

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
after it. The structure is meant to make this a new platform and not a new
codebase:

- a platform directory and a branch in `cmake/Platform.cmake` for the target,
  with a toolchain file for the cross compiler;
- the libraries built to what that target allows. That probably means no
  exceptions, no RTTI and no heap after start-up, and each of those is a
  constraint the library does not meet yet (the model types hold `std::string`
  and `std::vector`, and the library uses `std::expected` and `std::variant`,
  which need a standard library that has them). It already returns errors as
  values, computes its CRC tables at compile time, has no static initialisation
  and no `iostream`, which is the direction;
- a CI job that builds the library for the target with those constraints on, so
  that they hold.

What the target's certification needs from the code is not decided here.

## 7. Around the edges

clang-tidy as a CI job, the Release presets in CI, a coverage threshold once the
figures have been looked at, packaging, and a `docs/` site.
