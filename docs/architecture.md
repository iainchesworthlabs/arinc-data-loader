# Architecture

## The shape

```text
            +--------------------------------------+
            |  application (Qt 6.12, not built yet) |
            +------------------+-------------------+
                               |
              +----------------+----------------+
              |                                 |
   +----------v-----------+          +----------v-----------+
   | adl::arinc615a       |          |  adl::platform       |
   | (not built yet)      |          |  src/platforms       |
   +----------+-----------+          +----------------------+
              |
   +----------v-----------+
   | adl::arinc665        |
   | src/arinc665         |
   +----------------------+
```

- **`src/arinc665`** is the ARINC 665 library: loads, batches and media sets
  (written, read and verified), media labels, part numbers, CRCs and check values.
  It is GUI-free and platform-free. It includes no operating
  system header and nothing from Qt.
- **`src/platforms`** is the only place that knows which operating system this
  is. Each thing the program needs from the system is a header in
  `src/platforms/interfaces/adl/platform/`, with one implementation per platform
  beside it. [Platform isolation](platform-isolation.md) has the rules.
- **`src/arinc615a`** (planned) is the ARINC 615A protocol engine. It will use
  `adl::arinc665` for what it transfers and ask `adl::platform` for what the
  system provides, a UDP socket for TFTP being the obvious case.
- **`src/app`** (planned) is a thin Qt 6.12 layer over the libraries. Qt belongs
  here and nowhere else.

A library never depends on one above it. `adl::arinc665` does not link
`adl::platform`; only a library that needs the system does.

## Targets and names

| Directory | Target | Alias | Namespace | Headers |
| --- | --- | --- | --- | --- |
| `src/arinc665` | `adl_arinc665` | `adl::arinc665` | `adl::arinc665` | `adl/arinc665/*.hpp` |
| `src/platforms` | `adl_platform` | `adl::platform` | `adl::platform` | `adl/platform/*.hpp` |

A public header is included as `"adl/<library>/<name>.hpp"`; the include
directory of a library is its `include/` (or, for the platform, `interfaces/`).
The `adl::warnings`, `adl::sanitizers`, `adl::coverage` and `adl::tracy` targets
are linked into every first-party target by the build, not by the libraries'
users.

## Choices the libraries make

- **C++23, extensions off.** Every first-party target is built with warnings as
  errors (`cmake/CompilerWarnings.cmake`), and Debug builds run under
  AddressSanitizer (and UBSan where the compiler has it).
- **Failures are values.** An operation that can fail returns
  `std::expected<T, adl::arinc665::Error>`. Nothing in the library throws for an
  expected failure. (Running out of memory is not an expected failure: the model
  types hold `std::string` and `std::vector`, so the library is not yet free of
  the heap.)
- **Nothing happens before `main`.** The CRC tables are computed by the compiler
  (`constexpr`) and a checker is chosen by calling `IntegrityChecker::make`. The
  earlier design registered checkers in a factory from static initialisers, which
  depends on the order translation units are initialised in.
- **Closed sets are variants.** The three CRCs are held in a `std::variant`, so
  choosing one at run time costs no allocation.
- **Values, not hierarchies.** The load model is plain types with defaulted
  comparison. There is no virtual `OutputStream`: writing a file is a function
  that returns bytes, not a stream operator on a base class.
- **No `iostream` in the libraries.** They take and return spans of bytes. Files
  and sockets belong to the layers above, through `adl::platform`. A media set is
  read from `MediaMember`s, the files of each in memory; getting them from a
  directory is that layer's job.
- **Three layers inside the library.** The bottom is bytes: `WordWriter` and
  `WordReader` (private, `src/wire.hpp`), the CRCs, MD5 and SHA-1. Above it each
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
| `ARINC 615A` static library (empty) | not present; `adl::arinc615a` starts when the protocol does |
| `IIntegrityChecker`, `IntegrityChecker_CRC<>`, `IntegrityCheckerFactory`, `Singleton` | `Crc<>`, `Md5`, `Sha1` and `IntegrityChecker::make`; the singleton is gone |
| `LoadCheck` | `CheckValueType` (the type) and `IntegrityChecker` (the calculation) |
| `GenericFile`, `UserSuppliedFile`, `DataFile`, `SupportFile` | plain structs in `files.hpp` |
| `HeaderFile` | `HeaderFile` (the fields of the header file) with `encode_header_file` and `decode_header_file` |
| `Load` | `LoadSpec` (what a person decides), `Load` (the header, its bytes and the files), `build_load`, `verify_load` |
| `MediaSet`, `MediaSetMember` | `MediaSetSpec`, `MediaMember`, `MediaSet`, `build_media_set`, `read_media_set`; `LoadsList`, `FilesList`, `BatchesList` |
| C++/CLI MSTest project | Catch2, in `tests/unit` |

The earlier code did not do most of this: its header file `OutputStream` did not
compile and its pointers were placeholders, writing a load to a directory
returned `false`, and MD5 and SHA-1 were declared and never written. What is
still not here is writing a media set to a directory and reading one from it
([the roadmap](roadmap.md)).

## The standards

Section numbers in comments
and tests refer to ARINC 665-3 and ARINC 615A-3, which are not in the repository
([docs/specs](specs/README.md)). [ARINC 665 coverage](arinc665-coverage.md) maps
the standard onto the code.
