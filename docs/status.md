# What has been checked

This page is for engineers, technicians and programmers who need to know in one place what has been checked in these programs and libraries and what has not. It defines the levels of evidence, sets each component against them, lists the operating systems and RTOS ports, and lists what is not done. Every entry comes from a page named beside it or from the code and tests.

## In short

- **Nothing has been checked against real equipment, against another vendor's loader or target, or against another tool's media sets** ([the user's guide](user-guide/index.md#about-this-release), [the README](../README.md#status)).
- The tests are the project's own. Both ends of a test, the library and the simulated or mock target, are built from the same reading of the standards. The tests show that the code does what that reading says. They cannot show that the reading is right.
- The automated tests run on Windows, Linux and macOS in continuous integration. The programs have been used by hand on Windows only.
- FreeRTOS and Zephyr have run on simulators only. No board has run them.

## The evidence levels

Levels 1 to 6 are checks the project made itself, with code it wrote. Levels 7 and 8 would be checks against something the project did not write. The levels are not a ranking of the first six.

| Level | What it means | What it does not show |
| --- | --- | --- |
| 1. Unit-tested | A test program checks a piece of code against its contract: the standard's own numbers and examples, the test vectors of RFC 1321 (MD5) and FIPS 180 (SHA-1), round trips, and every truncation, bit change and random input of a good file, under the sanitizers of the Debug build. For the programs it includes the windows loaded offscreen and driven by Qt Quick Test | That the standard was read correctly |
| 2. Second implementation | The code is compared with a separately written implementation of the same thing. The ARINC 665 header file, batch file and list files, and the 615A packets and protocol files, are compared byte for byte with the output of Python scripts in `tests/golden` that share no code with the library. The TFTP of the simulated target and of the mock target is `adl::arinc615a_mini`, written separately from the engine's. The platform's conformance suite runs the same checks on the fake platform and on the real host | A misreading both share. `adl::arinc615a_mini` uses the codec and the figures in `limits.hpp`, so the 2 second time-out, the retry count and the default block size are not independently checked |
| 3. Simulated target | Whole operations run through the runner on the fake platform against the simulated target of `tests/engine`. The clock moves only when told. The network can lose, repeat and reorder datagrams, from fixed seeds, and the target can wait, go silent, abort, send a wrong checksum and so on | That a real target behaves like the simulated one |
| 4. Loopback sockets | `loopback.integration` runs the loader's runner over the real host's UDP sockets on 127.0.0.1 against the mock target's engine | Anything between two machines. The mock target's TFTP is the same second implementation, so this checks the sockets and the runner and not the choices made where the standard is unclear |
| 5. Whole programs on Windows | The three programs started together on one Windows machine and driven by a script through the whole demonstration. The system tests also start each program on all three desktops, and each must draw its first window and exit with 0 | Linux and macOS by hand. A real target. More than one machine |
| 6. Simulator | The platform conformance suite and the 615A engine tests run on an RTOS simulator: FreeRTOS on its POSIX simulator port, Zephyr on `native_sim`, and Zephyr on QEMU (a Cortex-A9 runs the whole suite, and a Cortex-M3 runs a smoke program without exceptions) | A board: interrupts, a real link, real flash |
| 7. Real equipment | An operation run against a real target, or a build run on a board | None has been done |
| 8. Another tool | A media set written here read by another tool, a media set written by another tool read here, or the 615A dialogue exchanged with another vendor's loader or target | None has been done |

### How the tests were tested

The tests were also checked by breaking the code on purpose and running them. The ARINC 665 library was broken in six ways, the 615A codec in 25 and the engine in 38, one at a time. Each break was caught. One was not caught at first in the codec and six were not caught at first in the engine, and each of those got a test. One engine break survives and is equivalent to the code as written. See [Testing](testing.md#breaking-the-code-on-purpose).

The coverage job fails below 90% of lines and 80% of branches of the portable code, and below a lower floor for the programs' window-free logic ([Testing](testing.md#coverage)).

## Components against the levels

The columns are the levels above. **yes**: the pages named below say it was done. **none**: nothing has been done. **n/a**: the level does not apply to the component. **via X**: the component runs inside the tests of X and has none of its own at this level. **partly**: see the notes.

| Component | 1 Unit | 2 Second impl. | 3 Simulated target | 4 Loopback | 5 Windows | 6 Simulator | 7 Real equipment | 8 Another tool |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ARINC 665 library | yes | yes | via engine | via engine | via programs | partly | none | none |
| 615A codec | yes | yes | via engine | via engine | via programs | via engine | none | none |
| 615A engine | yes | partly | yes | yes | via programs | yes | none | none |
| Runner | yes | none | yes | yes | via programs | yes | none | none |
| Media tree | yes | none | n/a | n/a | via programs | n/a | none | none |
| Loader (`adl_app`) | yes | n/a | yes | via runner | yes | n/a | none | none |
| Generator (`adl_media_gen`) | yes | n/a | n/a | n/a | yes | n/a | none | none |
| Mock target (`adl_mock_target`) | yes | n/a | yes | yes | yes | n/a | none | none |
| Platform layer (`adl::platform`) | yes | yes | n/a | yes | via programs | yes | none | none |

### Where each entry comes from

- **ARINC 665 library.** Levels 1 and 2: [ARINC 665-3 coverage](arinc665-coverage.md#in-short) and [Testing](testing.md#golden-payloads). Level 3: an upload loads a media set built with `build_media_set` ([ARINC 615A engine](arinc615a-engine.md#what-has-been-checked-and-what-has-not)). Level 4: the loopback test builds its load with the library. Level 6: the tests of the codec and of ARINC 665 (`adl_unit_tests`) are not run on the simulators, and the library runs there inside the engine tests ([Testing](testing.md#the-rtos-test-programs)). The Zephyr job on QEMU also compiles it without exceptions. Levels 7 and 8: [ARINC 665-3 coverage](arinc665-coverage.md#not-yet-verified).
- **615A codec.** Levels 1 and 2: [ARINC 615A codec](arinc615a-codec.md#what-has-been-checked-and-what-has-not). The other levels are the engine's. Levels 7 and 8: the same section ("Not checked: against any other implementation of 615A").
- **615A engine.** Level 1 and 3: [ARINC 615A engine](arinc615a-engine.md#what-has-been-checked-and-what-has-not). Level 2 is "partly" because the simulated target's TFTP is a second implementation of the transfer, and the state machines and operations have none. Level 4: `loopback.integration`, through the runner. Level 6: the RTOS test programs run everything `adl_engine_tests` runs ([Testing](testing.md#the-rtos-test-programs)). The tests of the 615A code with exceptions and RTTI switched off are not run anywhere ([ARINC 615A-3 coverage](arinc615a-coverage.md#not-yet-verified)).
- **Runner.** Levels 1 and 3: the engine tests run whole operations through the runner on the fake platform. Level 4: `loopback.integration`. Level 6: [Testing](testing.md#the-rtos-test-programs). How the time-outs behave on a desktop that can be late is unmeasured.
- **Media tree.** Level 1: `media_tree_test` ([Testing](testing.md#what-there-is)). The folder layout is the project's own, and no other tool has read a tree written here ([Testing](testing.md#known-gaps)).
- **Loader.** Levels 1 and 3: [The application](app.md#what-has-and-has-not-been-checked). Level 4: the integration test runs the runner, not the window. Level 5: the window has been driven on a real Windows desktop by a script, against the mock target on the same machine ([The application](app.md#what-has-and-has-not-been-checked)). Windows is the platform used by hand; Linux and macOS build and run their tests in CI, and nobody has driven the windows on them ([Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked)).
- **Generator.** Levels 1 and 5: [The media set generator](media-gen.md#what-has-and-has-not-been-checked). A generated set reads back with no findings and is accepted by the loader's own media set summary.
- **Mock target.** Levels 1, 3, 4 and 5: [The mock target](mock-target.md#what-has-and-has-not-been-checked). It is not a real target. FIND between the loader and the mock target on one machine does not work on Windows, so the demonstration names the target by hand.
- **Platform layer.** Levels 1, 2, 4 and 6: [Testing](testing.md#the-fake-and-the-conformance-suite) and [Platform abstraction](platform-abstraction.md#the-freertos-and-zephyr-ports). The conformance suite runs each check on the fake and on the real host of the platform.

## Operating systems and RTOS ports

| Platform | Status | What has run |
| --- | --- | --- |
| Windows | Supported | Built and tested by CI (MSVC, the Debug preset under AddressSanitizer, on every change). The release workflow builds and tests the Release preset. Used by hand |
| Linux | Supported | Built and tested by CI (GCC, the Debug preset under AddressSanitizer and UBSan). Run in a container by the maintainer (GCC 15, Debug under AddressSanitizer and UBSan, and Release). The windows have not been driven by hand |
| macOS | Supported, not tried by hand | Built and tested by CI (AppleClang, the Debug preset under AddressSanitizer and UBSan). Not run on a Mac by hand |
| FreeRTOS | Experimental | Built in CI. The conformance suite and the 615A engine tests run on the kernel's POSIX simulator port (FreeRTOS-Kernel V11.2.0, lwIP 2.2.1 over its loopback interface, littlefs on a RAM block device). No target hardware, real network interface or flash has run it. It has not been built with an embedded toolchain |
| Zephyr | Experimental | Built in CI. The conformance suite and the 615A engine tests run on `native_sim` (Zephyr 4.4.2, its own IPv4/UDP stack over its loopback interface, littlefs on simulated flash). The whole suite also runs on QEMU as a Cortex-A9 with the Zephyr SDK's own toolchain, and a smoke program runs on a Cortex-M3 without exceptions. No board has run it |
| QNX Neutrino | Unbuilt | Code exists and has never been compiled or run. The selector refuses it unless `-DADL_ACCEPT_UNBUILT_PLATFORM=ON` is given |
| INTEGRITY 5.0.8 (GHS) | Unbuilt | Code exists and has never been compiled or run. No GHS toolchain exists on the project's machines |
| INTEGRITY-178B (GHS) | Unbuilt | Code exists and has never been compiled. `Threading::spawn` and the `FileSystem` are `unsupported` on purpose |
| VxWorks | Design only | No code. A build is refused with an explanation |

"Experimental" is a status of its own: more than unbuilt code, because it compiles and a test suite passes on it in CI, and less than supported, because what ran is a simulator. The sources are [Platform abstraction](platform-abstraction.md#what-is-verified-and-what-is-a-design) and, for the INTEGRITY ports, [Green Hills INTEGRITY](platform-ghs.md).

### What CI and the release workflow build and run

| Desktop | CI, on every change | Release workflow |
| --- | --- | --- |
| Windows | `Windows (MSVC)`: runs on `windows-2025-vs2026`, builds the preset `windows-msvc-debug` warning-free (`/W4 /WX`) with AddressSanitizer, and runs all of ctest | `Windows (MSVC, Release)`: builds `windows-msvc-release`, runs ctest (every tier) on the optimised build, packages it, then unpacks the package and starts the programs with no Qt in the environment |
| Linux | `Linux (GCC)`: runs on `ubuntu-26.04`, builds `linux-gcc-debug` warning-free with AddressSanitizer and UBSan, and runs all of ctest. `Coverage (Linux, GCC)` also enforces the coverage minimum | `Linux (GCC, Release)`: as above for `linux-gcc-release` |
| macOS | `macOS (AppleClang)`: runs on `macos-26` (Apple silicon), builds `macos-clang-debug` warning-free with AddressSanitizer and UBSan, and runs all of ctest | `macOS (AppleClang, Release)`: as above for `macos-clang-release` |

The release workflow also builds a program against the development package and writes the software bill of materials. On a release branch or a version tag it signs and attests the files. The Release configuration is compiled nowhere else. The three simulator jobs (`Zephyr (native_sim)`, `FreeRTOS (POSIX simulator)` and `Zephyr (QEMU, SDK toolchain)`) are not required checks. The sources are [CI](ci.md#the-jobs-in-ciyml) and [Releasing](releasing.md).

## What is not done

### Real equipment and other tools

- No ARINC 615A operation (FIND, Information, Upload or Download) has been run against a real target or another vendor's loader ([Testing](testing.md#known-gaps) and [Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked)).
- No media set written by the generator or by `adl_write_payloads` has been read by another tool, and no media set from another tool has been read here ([Testing](testing.md#known-gaps)).
- The places where the standards are unclear were settled by the project's own choices, listed in [ARINC 665-3 coverage](arinc665-coverage.md#places-the-standard-is-unclear), [ARINC 615A codec](arinc615a-codec.md#places-the-standard-is-unclear) and [ARINC 615A-3 coverage](arinc615a-coverage.md#places-the-standard-is-unclear). The first to confirm against a real device are the order of the files in the Load CRC (665), the text length of a protocol file, the status codes, the checksum option's value and the FIND manufacturer code (codec), and the time-outs applied to `.LNO`, `.LCL` and `.LNL` (engine).
- Which check value a real target asks for each file of an upload, and how a real target forms its `THW_ID_POS`, are unconfirmed ([Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked)).
- The random reference files of ARINC 665-3 Appendix L (T05 to T10) are not in the standard, and their CRCs are not tested. Manufacturer's codes are checked for form only ([ARINC 665-3 coverage](arinc665-coverage.md#not-yet-verified)).

### Conditions not tried

- A media set of many gigabytes, a very slow or lossy network, and a target that is busy with another loader.
- FIND with more than one network adapter, or across routers.
- The windows at scales other than 100%, keyboard use and screen readers beyond the controls' accessible names.
- The translations. Seven of the eight languages are machine-drafted first versions that no speaker has read ([Translations](translations.md)).
- Timing on a real-time system. The time-outs are tested on a clock that moves only when told ([ARINC 615A-3 coverage](arinc615a-coverage.md#not-yet-verified)).

The sources of this group are [Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked), [The application](app.md#what-has-and-has-not-been-checked), [The mock target](mock-target.md#what-has-and-has-not-been-checked) and [The media set generator](media-gen.md#what-has-and-has-not-been-checked).

### In the tests

- No job runs the 615A tests with exceptions and RTTI switched off. The Zephyr job on QEMU compiles the libraries that way for a Cortex-M3 and runs only a platform smoke program. No job builds them that way with a desktop compiler ([ARINC 615A codec](arinc615a-codec.md#shape) and [Testing](testing.md#known-gaps)).
- FIND over real sockets is not in the loopback test, because two sockets sharing port 1001 on one machine are not reliable on every platform ([Roadmap](roadmap.md#4-the-application)).
- The conformance suite cannot show a data race. A real allocator is not asked for an impossible amount of memory under the sanitizers ([Testing](testing.md#known-gaps)).

### Platforms and embedded targets

- FreeRTOS and Zephyr: no board has run them. Still open are interrupt latency, a real link (FIND's broadcast leaving a machine, a 65 KB datagram fragmented on the wire), real flash, a priority plan settled against the network stack's threads, and FreeRTOS built with an embedded toolchain ([Platform abstraction](platform-abstraction.md#the-freertos-and-zephyr-ports) and [Roadmap](roadmap.md#6-an-embedded-target)).
- QNX and the two INTEGRITY ports have never been compiled. VxWorks has no code.
- "No allocation after start-up" is a goal and not a fact: `adl::arinc665` allocates, and so does the platform layer for each handle ([Platform abstraction](platform-abstraction.md#gaps-and-risks)).
- There is no program for the target side. The libraries build for a target's platform, and `adl_mock_target` answers on a desktop for demonstrations ([Roadmap](roadmap.md#3-arinc-615a), [Building](building.md#what-cmake-builds)).


### Packages and profiling

- Not shipped: a Windows installer, a `.deb`, an AppImage, an Authenticode or Developer ID signature, notarisation, icons for Windows and macOS, and builds for architectures other than Windows x86-64, Linux x86-64 and macOS arm64 ([Releasing](releasing.md#what-is-not-shipped-and-why)).
- The profiling work that remains: a trace of an I/O-bound upload of a many-gigabyte media set, the target's transfer timing over a real network, and GPU zones ([Roadmap](roadmap.md#5-profiling)).
