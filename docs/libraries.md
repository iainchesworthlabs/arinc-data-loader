# Using the libraries

*For a programmer who wants to build on the ARINC 665 or ARINC 615A code in a program of their own. People who
only run the programs need the [user's guide](user-guide/index.md).*

Each release has a **development package** beside the programs: the headers, the static libraries and a CMake
package called `adl`. It holds the libraries that do not need Qt and that include no operating-system header
themselves:

| Target | What it is |
| --- | --- |
| `adl::arinc665` | ARINC 665 media sets, loads, batch files and check values |
| `adl::arinc615a` | The ARINC 615A codec and the data loader's state machines (no sockets, no threads) |
| `adl::arinc615a_runner` | The runner that drives those state machines over `adl::platform`'s sockets, and a file source and sink over a directory |
| `adl::media_tree` | A media set as a directory tree on disk |
| `adl::platform` | The platform interfaces (files, memory, threads, clock, UDP sockets, queues) and this host's implementation of them |

What is not in the package, and why, is in [Releasing](releasing.md#the-development-package). The pages that
describe the design are [Architecture](architecture.md), [Platform abstraction](platform-abstraction.md),
[ARINC 665 coverage](arinc665-coverage.md) and [ARINC 615A engine](arinc615a-engine.md).

## Use it from CMake

The package is a zip (Windows) or a tar.gz (Linux, macOS) named
`arinc-data-loader-dev-<version>-<system>-<arch>`. It unpacks into a directory called
`arinc-data-loader-<version>-<system>-<arch>`, which is the directory to give CMake:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/arinc-data-loader-0.1.0-linux-x86_64
```

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_tool LANGUAGES CXX)

find_package(adl 0.1 REQUIRED)

add_executable(my_tool main.cpp)
target_compile_features(my_tool PRIVATE cxx_std_23)
target_link_libraries(my_tool PRIVATE adl::media_tree)    # brings adl::arinc665 with it
```

- The libraries are **static**, so nothing is needed when the program runs, and the package needs no Qt.
- The code is C++23: it uses `std::expected` and `std::span`, so the compiler and its standard library must have
  them ([Building](building.md#requirements) lists the compilers CI uses).
- **Before 1.0 the interfaces are not stable.** The package's version file accepts the same minor version only:
  `find_package(adl 0.1 ...)` finds 0.1.x and not 0.2.
- CI installs the development package into an empty folder and builds a program against it
  (`tests/packaging/consumer`, the test `system.install_consumer`), so a header or a library missing from the
  package fails the release.

## Example: check a media set folder

This program reads a media set from disk, the way the loader does when it checks one, and prints what the
library finds. It is `tests/packaging/consumer/check_media_set.cpp`, which CI builds against the installed
package.

```cpp
#include <iostream>

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: check_media_set <folder that holds member_001, member_002, ...>\n";
        return 2;
    }

    // The members of the set, as they are on disk.
    const auto members = adl::media_tree::read_media_set_tree(argv[1]);
    if (!members) {
        std::cerr << "cannot read " << members.error().path.string() << ": "
                  << to_string(members.error().error) << '\n';
        return 1;
    }

    // The set the members describe: its loads and batches, and where it departs from the standard.
    const auto set = adl::arinc665::read_media_set(*members);
    if (!set) {
        std::cerr << "not a media set: " << to_string(set.error()) << '\n';
        return 1;
    }

    std::cout << members->size() << " member(s), " << set->loads.size() << " load(s), "
              << set->batches.size() << " batch(es)\n";
    for (const adl::arinc665::Finding& finding : set->findings) {
        std::cout << "  " << finding.clause << ": " << finding.message << '\n';
    }
    return 0;
}
```

Two things to see in it:

- **Failure is a value.** Every call that can fail returns a `std::expected`, and nothing in the libraries
  throws. A *finding* is not a failure: a set that departs from what the standard recommends (a part number
  format, a file name) is still read, and each departure is a `Finding` that names the clause.
- **Reading is in two steps.** `adl::media_tree` reads the members from disk and does no ARINC 665 parsing;
  `adl::arinc665::read_media_set` parses and checks them and does no file input. The same split lets a program
  that has the members some other way use the second step alone.

To make a media set instead, `adl::arinc665::build_media_set` takes a description of the loads and returns the
members, and `adl::media_tree::write_media_set_tree` writes them to a folder. `src/write_payloads/main.cpp` does
both and then reads the folder back, and is the shortest complete program of that kind in the repository.

## The ARINC 615A part

The codec and the engine are **sans-I/O**: they hold no socket, thread, clock or file system, and work on bytes.
The engine is told the time and the bytes that arrived, and returns the bytes to send. To run a whole operation
over UDP a program uses `adl::arinc615a_runner`, which opens the sockets and drives the engine through
`adl::platform` (`adl::platform::make_host()` gives this machine's implementation). A program for another
system, or a test, can drive the engine itself with no network at all.
[ARINC 615A engine](arinc615a-engine.md) describes the interfaces, the memory the engine needs and the choices
made where the standard is unclear.

Nothing has been checked against a real target or another implementation of ARINC 615A
([the user's guide](user-guide/files-and-limits.md#what-has-not-been-checked) says what has been checked and how).

## Zephyr and FreeRTOS

The development package is for the three desktops. The experimental Zephyr and FreeRTOS ports are built by
projects of their own, not by this package; [Building](building.md#zephyr-and-freertos) says how.
