# ARINC Data Loader

An ARINC 615A and 665 data loader, written from the standards, in C++23, built with CMake for
Windows, macOS and Linux.

ARINC 665 describes how loadable software is packaged: the header file, the data
and support files, their check values, and the media set they travel on. ARINC
615A describes how a data loader moves a load to (and from) a target over a
network. The two standards are not in the repository (see
[docs/specs](docs/specs/README.md)); the code cites them by section.

The documentation, with a [user's guide](docs/user-guide/index.md), is published at
<https://iainchesworthlabs.github.io/arinc-data-loader/> from the first release on. The same pages are in
[`docs/`](docs/README.md).

## Status

Version 0.1.0, a preliminary release. The libraries, the data loader and its two companion
applications work end to end against each other on one machine, and the libraries are tested
against simulated targets and over loopback sockets. **Nothing has been checked against a real
target, a real loader, or another tool's media sets**, and there is no installer yet (the programs
are built from source). What exists:

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
  fake that runs the whole engine without a socket or a real second. FreeRTOS and
  Zephyr are experimental: their platform conformance suite and the 615A engine tests
  run in CI on simulators (the FreeRTOS POSIX port, Zephyr's native_sim), and on no
  board. QNX and the two Green Hills INTEGRITY ports (5.0.8 and 178B) have code that
  has never been compiled; VxWorks is a design.
  [docs/platform-abstraction.md](docs/platform-abstraction.md) has the design and
  says what has been verified; [docs/platform-ghs.md](docs/platform-ghs.md) is the
  review behind the INTEGRITY ports.
- **`adl::arinc615a`**, the data loader's side of ARINC 615A-3. The codec: the TFTP
  packets with their options (and the WAIT and ABORT messages), the FIND packets and
  the twelve protocol files (`.LCI`, `.LCL`, `.LCS`, `.LUI`, `.LUR`, `.LUS`, `.LND`,
  `.LNO`, `.LNR`, `.LNS`, `.LNL`, `.LNA`), written into caller-supplied buffers and
  read back as checked views. The engine: a TFTP client and server and the
  Information, Upload and Download operations, with the time-outs and retries of
  Attachment 4, the abort paths and FIND, as state machines that are told the time
  and the bytes that arrived and answer with the bytes to send. They hold no socket,
  thread or clock, allocate nothing and throw nothing, so they can run on an embedded
  target and be tested without a network. **`adl::arinc615a_runner`** drives them over
  `adl::platform`. A full load of a media set built with `adl::arinc665` is tested
  against a simulated target on the fake platform, and files are downloaded back.
  [docs/arinc615a-codec.md](docs/arinc615a-codec.md) has the layouts,
  [docs/arinc615a-engine.md](docs/arinc615a-engine.md) the design and
  [docs/arinc615a-coverage.md](docs/arinc615a-coverage.md) the standard section by
  section, with the places it is unclear. The loopback integration test runs whole
  operations over real sockets; nothing has been checked against another
  implementation yet.
- **The three Qt 6.12 applications** (Windows, macOS and Linux, behind `ADL_BUILD_APP`):
  - **`adl_app`**, the data loader ([docs/app.md](docs/app.md)): a media set chosen as a
    directory and checked before anything is uploaded (reading it is the check; an
    unreadable set cannot be uploaded, and the standard's findings are shown as
    warnings), FIND, the Information, Upload and Download operations with progress and an abort,
    preferences that persist, properties for the media set and the target, and failures
    that say what went wrong and, where it is known, what to do about it. The window is in
    eight languages (Hebrew and Yiddish mirror the whole window; the translations are machine-drafted and not yet reviewed by speakers, see [docs/translations.md](docs/translations.md)), and a profiling
    build instruments the libraries, the engine and the window's frame loop with Tracy
    0.14.1 ([docs/profiling.md](docs/profiling.md)).
  - **`adl_media_gen`**, the media set generator ([docs/media-gen.md](docs/media-gen.md)):
    files chosen from disk are turned into a valid ARINC 665 load and media set, written
    as the directory tree the loader opens, and verified by reading it back.
  - **`adl_mock_target`**, a 615A target over real sockets
    ([docs/mock-target.md](docs/mock-target.md)): it answers FIND, runs the Information,
    Upload and Download dialogues and stores what an upload left in it, so the loader can
    be demonstrated end to end without an embedded box — and it can be made to misbehave
    (WAIT, refuse, drop status files, a wrong checksum, an abort, silence) to watch the
    loader cope. Its TFTP is `adl::arinc615a_mini` (`src/arinc615a_mini`), the second,
    independent TFTP implementation the tests were written around, shared now with the
    mock target so the two ends of a dialogue share no transfer code.
  - **One design system** (`src/ui`) for all three windows, in the ICL Forge Modernist
    language: the Signal palette, Archivo, zero radius, the 4/8/12/16/24/32 rhythm, and one
    set of controls. The human-factors analysis behind it, with before/after screenshots, is
    in [docs/design-overhaul.md](docs/design-overhaul.md); the token and component reference
    is [docs/design-system.md](docs/design-system.md).
- The build, the tests, the CI and the repository rules. The tests are labelled
  `unit`, `integration` and `system`; a Qt Quick Test suite drives the loader
  window through its real buttons and fields, and coverage of the portable code
  is gated at 90% of lines and 80% of branches
  ([docs/testing.md](docs/testing.md)). A nightly
  static-analysis suite (clang-tidy, MSVC Code Analysis, CodeQL and Clang's
  sanitizers) is described in
  [docs/static-analysis.md](docs/static-analysis.md).

What is planned is in [docs/roadmap.md](docs/roadmap.md): testing the format
against another tool's files and ARINC 615A against a real target, review of the
machine-drafted translations by speakers, profiling a large upload over a real network, and a build of
the loader library for an embedded target (the Green Hills INTEGRITY ports are written and
unbuilt; VxWorks is a design).

## Using it

[The user's guide](docs/user-guide/index.md) explains the two standards in a paragraph each, how to
start the three programs, and a complete demonstration on one machine: generate a media set,
start the mock target, then find it, read its information, upload the set and download files
from it, with a picture of every screen.

![The loader after an upload to the mock target](docs/images/app-upload-complete.png)

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
src/arinc615a/       the ARINC 615A codec and engine: TFTP, FIND, the operations (platform-free)
src/arinc615a_mini/  the second, independent TFTP implementation, shared by the tests' simulated
                     target and the mock target application
src/arinc615a_runner/ the runner that drives the engine over adl::platform
src/media_tree/      media sets on a disk: a directory per member
src/samples/         the sample loads and media sets
src/write_payloads/  adl_write_payloads, which writes the samples to a directory
src/app/             adl_app: the data loader window (Qt Quick), preferences, validation, FIND,
                     Information, Upload and Download
src/ui/              the shared design system and window chrome for the three applications
src/media_gen/       adl_media_gen: the media set generator window (Qt Quick)
src/mock_target/     adl_mock_target: a 615A target over real sockets, for demonstrations
src/platforms/       everything that differs between operating systems
tests/               unit, integration and system tests; the fake platform; the Qt Quick Test UI suite
cmake/               the platform selector, warnings, static analysis, sanitizers, coverage, Tracy, Qt
tools/               repository checks, commit policy, git hooks, release and repository-setup
                     scripts, the simulator jobs' scripts, the documentation build and screenshots,
                     firewall and AppArmor helpers
docs/                the user's guide, architecture, roadmap, CI, static analysis, coverage of 665 and 615A,
                     the three applications, the 615A codec and engine, docs/specs
```

The operating system is chosen by CMake, never by the source: there is no `#ifdef`
for a platform anywhere. [docs/platform-isolation.md](docs/platform-isolation.md)
explains how, and [docs/architecture.md](docs/architecture.md) the rest.

## Documentation

The full index is [docs/README.md](docs/README.md).

| Page | What it is |
| --- | --- |
| [User's guide](docs/user-guide/index.md) | Installing, the demonstration, every screen and setting of the three programs, troubleshooting |
| [Building](docs/building.md), [Testing](docs/testing.md), [CI](docs/ci.md), [Static analysis](docs/static-analysis.md), [Profiling](docs/profiling.md) | Build, test, continuous integration and analysis |
| [Releasing](docs/releasing.md), [Verifying a release](docs/verifying-releases.md) | For the maintainer: how a release is cut, built and signed. For downloaders: how to check one |
| [Screenshots](docs/screenshots.md) | How the pictures in the documentation are regenerated |
| [Architecture](docs/architecture.md), [Platform abstraction](docs/platform-abstraction.md), [Platform isolation](docs/platform-isolation.md), [Green Hills INTEGRITY](docs/platform-ghs.md) | How the code is arranged and how it stays portable |
| [ARINC 665 coverage](docs/arinc665-coverage.md), [615A codec](docs/arinc615a-codec.md), [615A engine](docs/arinc615a-engine.md), [615A coverage](docs/arinc615a-coverage.md) | The libraries, section by section |
| [The application](docs/app.md), [the generator](docs/media-gen.md), [the mock target](docs/mock-target.md) | The three programs, for developers |
| [Design overhaul](docs/design-overhaul.md), [design system](docs/design-system.md) | The human-factors analysis and the shared look |
| [Translations](docs/translations.md), [translation glossaries](docs/translation-glossaries.md) | The eight languages, how a language is switched and the window mirrored, and the glossaries the drafts chose |
| [Roadmap](docs/roadmap.md), [licensing](docs/licensing.md), [branch protection](docs/branch-protection.md), [specs](docs/specs/README.md) | What is planned, licences, repository rules, the standards |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Commit messages and pull requests carry
no attribution of any kind; [docs/ci.md](docs/ci.md) says how that is checked.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE) and [docs/licensing.md](docs/licensing.md).
