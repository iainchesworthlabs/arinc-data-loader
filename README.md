# ARINC Data Loader

An ARINC 615A and 665 compliant data loader, in C++23, built with CMake for
Windows, macOS and Linux.

ARINC 665 describes how loadable software is packaged: the header file, the data
and support files, their check values, and the media set they travel on. ARINC
615A describes how a data loader moves a load to (and from) a target over a
network. The two standards are not in the repository (see
[docs/specs](docs/specs/README.md)); the code cites them by section.

## Status

This is the data format and the foundation, not a loader yet. What exists:

- **`adl::arinc665`**, a library with no GUI and no operating system code, that
  writes and reads everything ARINC 665-3 defines: loads (header, data and
  support files), batch files, media sets with their `LOADS.LUM`, `FILES.LUM` and
  `BATCHES.LUM`, media labels, part numbers and the CRCs and check values (CRC-8,
  -16, -32, MD5, SHA-1) that protect them. It checks a load or a media set end to
  end and reports where it departs from the standard's recommendations.
  [docs/arinc665-coverage.md](docs/arinc665-coverage.md) goes through the
  standard section by section, with the tests that hold each part down and the
  places where the standard is unclear.
- **`adl::media_tree`**, which writes the members of a media set to a directory
  tree (one directory per member) and reads one back, and
  **`adl_write_payloads`**, a command-line program that writes sample media sets
  (including the ARINC 615 and Boeing 777 layouts of Appendix F) to a directory,
  for trying other loaders.
- **`adl::platform`**, the one place that knows which operating system this is:
  files, memory, threads and priorities, a clock, UDP sockets and message queues,
  as interfaces with an implementation each for Windows, Linux and macOS, and a
  fake that runs the whole engine without a socket or a real second. Designed so
  that QNX, VxWorks, FreeRTOS and Zephyr can be added.
  [docs/platform-abstraction.md](docs/platform-abstraction.md) has the design and
  says what has been verified.
- **`adl::arinc615a`**, the codec half of ARINC 615A-3: the TFTP packets with their
  options (and the WAIT and ABORT messages), the FIND packets and the twelve
  protocol files (`.LCI`, `.LCL`, `.LCS`, `.LUI`, `.LUR`, `.LUS`, `.LND`, `.LNO`,
  `.LNR`, `.LNS`, `.LNL`, `.LNA`), written into caller-supplied buffers and read
  back as checked views, with no allocation and no exceptions, so it can run on
  an embedded target. It does not yet send or receive anything: the operations
  and state machines are the next stage.
  [docs/arinc615a-codec.md](docs/arinc615a-codec.md) has the layouts and the
  choices made where the standard is unclear.
- The build, the tests, the CI and the repository rules.

What is planned is in [docs/roadmap.md](docs/roadmap.md): testing the format
against another tool's files, the ARINC 615A operations and state machines, a Qt 6.12 application,
Tracy profiling, and a build of the loader library for an embedded VxWorks
target.

## Build

```sh
git clone --recurse-submodules https://github.com/iainchesworthlabs/arinc-data-loader
cd arinc-data-loader
cmake --preset windows-msvc-debug      # or linux-gcc-debug, macos-clang-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

[docs/building.md](docs/building.md) has the requirements and the options.

## Layout

```text
src/arinc665/        the ARINC 665 library (platform-free, GUI-free)
src/arinc615a/       the ARINC 615A codec: TFTP, FIND and the protocol files (platform-free)
src/media_tree/      media sets on a disk: a directory per member
src/samples/         the sample loads and media sets
src/write_payloads/  adl_write_payloads, which writes the samples to a directory
src/platforms/       everything that differs between operating systems
tests/               Catch2 unit tests, the platform conformance suite and the fake platform
cmake/               the platform selector, warnings, sanitizers, coverage, Tracy, Qt
tools/               repository checks, commit policy, git hooks
docs/                architecture, roadmap, CI, coverage of ARINC 665, the 615A codec, docs/specs
```

The operating system is chosen by CMake, never by the source: there is no `#ifdef`
for a platform anywhere. [docs/platform-isolation.md](docs/platform-isolation.md)
explains how, and [docs/architecture.md](docs/architecture.md) the rest.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Commit messages and pull requests carry
no attribution of any kind; [docs/ci.md](docs/ci.md) says how that is checked.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE) and [docs/licensing.md](docs/licensing.md).
