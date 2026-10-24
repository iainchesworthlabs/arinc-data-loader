# Security and threat model

*For developers. How to report a problem is [SECURITY.md](../SECURITY.md); how to check a download is
[Verifying a release](verifying-releases.md).*

## What is protected

- The computer the programs run on: a load, a media set or a message from a target must not make the
  programs read or write outside the directories they were given, or run code the author of the data chose.
- The target's loads: the data the loader sends is the file the operator picked, unchanged.
- The release: a download is the file the maintainer's workflow built from this repository.

## Where untrusted data comes in

| Entry | What arrives | Where it is parsed |
| --- | --- | --- |
| Media set files | ARINC 665 header, list and batch files, and the data files they name | `libs/arinc665` |
| UDP from a target | TFTP packets and FIND replies | `libs/arinc615a` (codec), then the engine |
| Files a target sends | ARINC 615A protocol files: status, file lists, operator messages | `libs/arinc615a` (`protocol_files`) |
| File names | A name in a media set, a file list or a TFTP request | `adl::arinc665::is_portable_file_name` |
| The loader's own files | Preferences and saved settings, written by the programs | `libs/ui` |

The platform layer (`libs/platform`) is where files and sockets are opened, so every read and write goes
through one place ([Platform isolation](platform-isolation.md)).

## What the code does about it

- **Decoders check before they read.** A codec returns an error for a short buffer, a length that does
  not fit and a value out of range, and does not throw ([Architecture](architecture.md)). The limits are
  those of the standards (`libs/arinc615a/include/adl/arinc615a/limits.hpp`).
- **A name is a name, not a path.** `is_portable_file_name` refuses separators, `.` and `..`, device
  names, alternate-data-stream colons and trailing dots and spaces, so a name from a target cannot leave
  the directory it is joined to.
- **The parsers are fuzzed.** Sixteen libFuzzer harnesses cover the ARINC 665 files and the ARINC 615A
  packets and files, with the seeds and crash inputs kept in the repository ([Fuzzing](fuzzing.md)).
- **Sanitizers and hardening.** CI builds and tests with AddressSanitizer and UndefinedBehaviorSanitizer
  on Linux and Windows and ThreadSanitizer on Linux. Release builds use the hardening flags of
  `cmake/icl/IclHardening.cmake` (stack protection, control-flow protection, RELRO and now-binding on
  Linux; `/GS /guard:cf /sdl` and the matching linker flags on Windows).
- **Static analysis** runs nightly: clang-tidy, MSVC Code Analysis and CodeQL ([Static analysis](static-analysis.md)).
- **Supply chain.** Actions are pinned to commit SHAs, the Qt kit's archives and the Python tools are
  pinned by hash, every release has an SBOM, signatures and build-provenance attestations, and a pull
  request that changes a dependency is checked by the dependency diff scan ([CI](ci.md), [Releasing](releasing.md)).

## What it does not defend against

- **A hostile network.** ARINC 615A over TFTP has no authentication and no encryption, so anything that can
  reach the loader's UDP ports can pose as a target, and anything on the path can read or change a load.
  The loader tells the operator what the target reported; it does not decide that a target is genuine.
- **A hostile operator.** The programs run with the operator's rights and write where the operator asks.
- **The standards' own limits.** A checksum in a load detects damage, not tampering.
- **Qt and the operating system.** The packages carry Qt as shared libraries; keeping them current is a
  release each time Qt publishes a fix.

A report that depends on one of these is a limit of the protocol or of the setting, not a flaw in this code
([SECURITY.md](../SECURITY.md), "What counts").
