# Architecture

## The shape

```text
            +--------------------------------------+
            |  application (Qt 6.12, apps/app)     |
            +------------------+-------------------+
                               |
                    +----------v-----------+
                    | adl::arinc615a_runner|  drives the engine over the platform
                    +-----+-----------+----+
                          |           |
          +---------------v--+   +----v-----------------+
          | adl::arinc615a   |   |  adl::platform       |
          | codec and engine |   |  libs/platform       |
          | (sans-I/O)       |   |                      |
          +--------+---------+   |  files, memory,      |
                   |             |  threads, clock,     |
          +--------v---------+   |  UDP, queues         |
          | adl::arinc665    |   +----------------------+
          | libs/arinc665    |
          +------------------+

   adl::media_tree (libs/media_tree): members <-> a directory tree, over adl::arinc665
   adl::samples (libs/samples): the sample sets; adl_write_payloads writes them out
   adl::arinc615a_mini (libs/arinc615a_mini): a second TFTP over the codec, for the tests' simulated
   target and the mock target application
```

- **`libs/arinc665`** is the ARINC 665 library: loads, batches and media sets
  (written, read and verified), media labels, part numbers, CRCs and check values.
  It is GUI-free and platform-free. It includes no operating
  system header and nothing from Qt.
- **`libs/media_tree`** puts a media set on a disk: the members of a set written to
  a directory tree, one directory per member, and read back, with the standard's
  backslash pathnames mapped to native paths. It uses `std::filesystem`, which is
  the same on every platform, so it holds no platform code and `adl::arinc665`
  keeps its freedom from file I/O. It refuses names that cannot be files on every
  platform, so that a tree written on one system reads on another, and never
  overwrites or follows a link.
- **`libs/samples`** is the sample loads and media sets (the golden scenario of
  `tests/golden/make_golden.py` and the ARINC 615 and Boeing 777 layouts of
  Appendix F), as data. The tests build from it, and so does
  **`apps/write_payloads`**, the `adl_write_payloads` program that writes them to a
  directory so that other loaders can be tried on them.
- **`libs/platform`** is the only place that knows which operating system this
  is. Each thing the program needs from the system is a header in
  `libs/platform/interfaces/adl/platform/`, with one implementation per platform
  beside it: files, memory, threads and priorities, a clock, UDP sockets and
  message queues, gathered in a `Host` that the runner is handed (a real one
  from `make_host()`, or the fake in `tests/fake`). [Platform
  isolation](platform-isolation.md) has the rules and
  [platform abstraction](platform-abstraction.md) the design, how each of seven operating systems and two INTEGRITY
  ports (which have [a page of their own](platform-ghs.md)) provides each service, and what has and has not been
  verified.
- **`libs/arinc615a`** is the ARINC 615A library, the data loader's side of the protocol. Its codec
  is the TFTP packets with their options (and the WAIT and ABORT messages), the FIND packets and the
  twelve protocol files of section 6.4, each written into a buffer the caller supplies and read back
  as views over the bytes, with every length checked. Its engine is **sans-I/O**: state machines
  driven by the time and the bytes that arrived, that return the bytes to send and the next time they
  want to be woken, with no thread, socket, clock or file system inside. `TftpTransfer` is one file
  transfer (client or server, sending or receiving); `DataLoader` runs an Information, Upload or
  Download operation over a few of them at once, with the time-outs and retries of Attachment 4 and the
  abort paths; `FindClient` is FIND. The files of an upload and a download are reached through two small
  interfaces, `FileSource` and `FileSink`. It is platform-free and allocates nothing of its own (the one exception
  is the value of a checksum, which `adl::arinc665` returns in a `std::vector`): no `std::string`
  or `std::vector`, no exceptions, no `iostream`, and it never builds an `adl::arinc665` model (it uses
  that library's check value numbers and checksums only). [ARINC 615A codec](arinc615a-codec.md) has the
  layouts, [ARINC 615A engine](arinc615a-engine.md) the design and the choices made where the standard is
  unclear, and [ARINC 615A coverage](arinc615a-coverage.md) the standard section by section.
- **`libs/arinc615a_mini`** is a second, independent TFTP implementation (lock-step blocks, the time-out and
  retry, the options the loader uses, WAIT and ABORT), deliberately plain and written on the codec, so that the engine is checked against another reading of the standard and not against itself. It uses the codec and the figures in `limits.hpp` (the time-outs, the retry count and the default block size), so those are not independently checked. It is the TFTP of the simulated target of `tests/engine` and of the mock target application.
- **`libs/arinc615a_runner`** is the thin layer that is the only 615A code that calls the platform: it
  opens the sockets the engine asks for, waits on all of them from one thread with
  `Network::wait_readable`, reads the platform's monotonic clock, and carries out what the engine answers.
  It also has a `FileSource` and a `FileSink` over a directory of the platform's files. That is what
  makes the engine portable to an RTOS (a port replaces the runner) and testable without a network (the
  tests replace the platform with the fake and the target with a simulated one).
- **`apps/app`**, **`apps/media_gen`** and **`apps/mock_target`** are the Qt 6.12 applications, thin layers
  over the libraries: a window to choose a media set, run FIND, pick a target and run the Information,
  Upload and Download operations; a generator for the media set those operations load; and a mock
  target over real sockets. The engine runs on a thread of its own (the runner blocks), reports to the
  window through queued signals, and is aborted through an atomic flag the runner polls. Each
  application's `core/` is its logic without a window, tested headless; each `qml/` is its view.
  **`libs/ui`** is the shared design system and window chrome for all three (the same QML source is
  compiled into each application's module). Qt is used in these directories only, and the
  applications build when `ADL_BUILD_APP` is on (the default on a desktop where Qt 6.12 is found). [The application](app.md) has the threading,
  what it shows and what it does not do yet; [the design overhaul](design-overhaul.md) has the
  human-factors analysis behind the three windows.

A library never depends on one above it. `adl::arinc665` does not link
`adl::platform`; only a library that needs the system does.

## The embedded direction

The roadmap includes a build of the loader library for VxWorks 178C. The platform
layer is shaped for it: a platform is a directory and a branch in the selector;
interfaces are small enough for an RTOS with a flash file system or none; memory
comes from caller-supplied buffers, an arena and a block pool; priorities are
abstract; the engine is driven by time and bytes, not by threads; and nothing
throws. What is still not true is "no allocation after start-up": the ARINC 665
model types hold `std::string` and `std::vector`. [Platform
abstraction](platform-abstraction.md#area-2-memory-memoryhpp) says where, and the
route to remove it.

## Targets and names

| Directory | Target | Alias | Namespace | Headers |
| --- | --- | --- | --- | --- |
| `libs/arinc665` | `adl_arinc665` | `adl::arinc665` | `adl::arinc665` | `adl/arinc665/*.hpp` |
| `libs/media_tree` | `adl_media_tree` | `adl::media_tree` | `adl::media_tree` | `adl/media_tree/*.hpp` |
| `libs/samples` | `adl_samples` | `adl::samples` | `adl::samples` | `adl/samples/*.hpp` |
| `libs/arinc615a` | `adl_arinc615a` | `adl::arinc615a` | `adl::arinc615a` | `adl/arinc615a/*.hpp` |
| `libs/arinc615a_runner` | `adl_arinc615a_runner` | `adl::arinc615a_runner` | `adl::arinc615a` | `adl/arinc615a/runner.hpp`, `directory_files.hpp` |
| `libs/arinc615a_mini` | `adl_arinc615a_mini` | `adl::arinc615a_mini` | `adl::arinc615a_mini` | `adl/arinc615a_mini/*.hpp` |
| `apps/app/core` | `adl_app_core` | `adl::app_core` | `adl::app` | `adl/app/*.hpp` |
| `apps/media_gen/core` | `adl_gen_core` | `adl::gen_core` | `adl::gen` | `adl/gen/*.hpp` |
| `apps/mock_target/core` | `adl_mock_core` | `adl::mock_core` | `adl::mock` | `adl/mock/*.hpp` |
| `libs/platform` | `adl_platform` | `adl::platform` | `adl::platform` | `adl/platform/*.hpp` |
| `libs/ui` | `adl_ui_message` | `adl::ui_message` | `adl::ui` | `adl/ui/message.hpp` |

The programs are `adl_app`, `adl_media_gen`, `adl_mock_target` and `adl_write_payloads`. The rest of `libs/ui` is not a library: `cmake/AdlUi.cmake` adds its sources and QML to each of the three Qt
applications.

A public header is included as `"adl/<library>/<name>.hpp"`; the include
directory of a library is its `include/` (or, for the platform, `interfaces/`).
The `adl::warnings`, `adl::sanitizers`, `adl::coverage` and `adl::tracy` targets
are linked into every first-party target by the build, not by the libraries'
users.

## Names in the code

The repository keeps the names it has, and `.clang-tidy` states them (`readability-identifier-naming`, checked
by the nightly clang-tidy run). They are not the organisation template's; where the two differ, this
repository's own convention governs its code.

| Kind | Case | Example |
| --- | --- | --- |
| Namespaces, functions, methods, variables, parameters | `snake_case` | `adl::arinc665::decode_header_file` |
| Classes, structs, enums, template parameters, type aliases | `PascalCase` | `MediaSetIndex`; a standard-library alias keeps its name (`value_type`) |
| Enumerators | `snake_case` | `CheckValueType::md5` |
| Constants, `constexpr` variables | `snake_case` | `sample_load_part_number` |
| Private and protected data members | `snake_case` with a trailing underscore | `current_language_` |

The classes QML reads (`apps/*/core` and `libs/ui`) name their properties, signals, slots and invokable functions
in `camelCase`, which is QML's convention, and the roles of a list model take Qt's `PascalCase` (`SourcePathRole`);
the `.clang-tidy` file in each of those directories allows both cases for a method. A new class follows the case of
its neighbours.

## Choices the libraries make

- **C++23, extensions off.** Every first-party target is built with warnings as
  errors (`cmake/CompilerWarnings.cmake`), and Debug builds run under
  AddressSanitizer (and UBSan where the compiler has it).
- **Failures are values.** An operation that can fail returns
  `std::expected<T, adl::arinc665::Error>` (`adl::platform::Result<T>` in the
  platform layer, whose interface is `noexcept` throughout; `adl::arinc615a::Error` in the 615A library and
  `adl::media_tree::TreeError` in the tree writer). Nothing in the
  library throws for an expected failure. (Running out of memory is not an expected failure: the model
  types hold `std::string` and `std::vector`, so the library is not yet free of
  the heap.)
- **Nothing happens before `main`.** The CRC tables are computed by the compiler
  (`constexpr`) and a checker is chosen by calling `IntegrityChecker::make`. The
  earlier design registered checkers in a factory from static initialisers, which
  depends on the order translation units are initialised in.
- **Closed sets are variants.** The five check-value algorithms (three CRCs, MD5 and SHA-1) are held in a
  `std::variant`, so choosing one at run time costs no allocation.
- **Values, not hierarchies.** The load model is plain types with defaulted
  comparison. There is no virtual `OutputStream`: writing a file is a function
  that returns bytes, not a stream operator on a base class.
- **No `iostream` in `adl::arinc665`.** It takes and returns spans of bytes. Files
  and sockets belong to the layers above: sockets through `adl::platform`, files
  through `adl::media_tree`. A media set is read from `MediaMember`s, the files of
  each in memory; getting them from a directory is `adl::media_tree`'s job.
- **Three layers inside the library.** The bottom is bytes: `WordWriter` and
  `WordReader` (private, `libs/arinc665/src/wire.hpp`), the CRCs, MD5 and SHA-1. Above it each
  file format is a plain model type with an `encode_*` and a `decode_*` function:
  `HeaderFile`, `BatchFile`, `LoadsList`, `FilesList`, `BatchesList`. At the top
  are the operations that need several files, `build_load`/`verify_load` and
  `build_media_set`/`read_media_set`.
- **A decoder reads by pointer and checks what it reads.** Lengths, pointers and
  CRCs are checked before a field is believed, and a damaged file is an
  `Error`, not undefined behaviour: `tests/unit/robustness_test.cpp` runs each
  decoder over every truncation and bit change of a good file.
- **"Must" is an error, "should" is a finding.** Where the standard requires
  something (a data file, a CRC that matches) the operation fails; where it
  recommends (a part number format, a file name) the work goes ahead and a
  `Finding` names the section. A loader has to read loads that are not
  perfect.

## From the earlier project

| Earlier | Now |
| --- | --- |
| `ARINC 665` static library (`ARINC_665::*`) | `adl::arinc665` |
| `ARINC 615A` static library (empty) | `adl::arinc615a` (the codec and the sans-I/O engine) and `adl::arinc615a_runner` |
| `IIntegrityChecker`, `IntegrityChecker_CRC<>`, `IntegrityCheckerFactory`, `Singleton` | `Crc<>`, `Md5`, `Sha1` and `IntegrityChecker::make`; the singleton is gone |
| `LoadCheck` | `CheckValueType` (the type) and `IntegrityChecker` (the calculation) |
| `GenericFile`, `UserSuppliedFile`, `DataFile`, `SupportFile` | `UserSuppliedFile`, `DataFile`, `SupportFile`, `ConfigLdrFile` and `ExcConfigLdrFile`, plain structs in `files.hpp`; `GenericFile` has no counterpart |
| `HeaderFile` | `HeaderFile` (the fields of the header file) with `encode_header_file` and `decode_header_file` |
| `Load` | `LoadSpec` (what a person decides), `Load` (the header, its bytes and the files), `build_load`, `verify_load` |
| `MediaSet`, `MediaSetMember` | `MediaSetSpec`, `MediaMember`, `MediaSet`, `build_media_set`, `read_media_set`; `LoadsList`, `FilesList`, `BatchesList` |
| C++/CLI MSTest project | Catch2, in `tests/unit` |

The earlier code (which is not in this repository) did not do most of this: its header file `OutputStream` did not
compile and its pointers were placeholders, writing a load to a directory
returned `false`, and MD5 and SHA-1 were declared and never written. Writing a
media set to a directory and reading one from it is `adl::media_tree`.

## The standards

Section numbers in comments
and tests refer to ARINC 665-3 and ARINC 615A-3, which are not in the repository
([docs/specs](specs/README.md)). [ARINC 665 coverage](arinc665-coverage.md) maps
the standard onto the code.
