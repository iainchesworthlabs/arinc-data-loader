# Building

This page builds ARINC Data Loader from source. What CI runs is in [CI](ci.md).

Only Windows with MSVC 19.51 (Visual Studio 2026) has been built and tested so
far. The Linux and macOS presets and platform code are written to the same
pattern and run in CI, and have not been tried by hand.

## Requirements

| What | Version | Notes |
| --- | --- | --- |
| CMake | 4.0 or newer | The presets require it. 4.4.0 was used |
| Ninja | any recent | The presets use the Ninja generator |
| A C++23 compiler | MSVC from Visual Studio 2022 17.x or Visual Studio 2026; GCC 14 or newer; AppleClang 16 or newer | The code uses `std::expected` and `<span>` |
| Git | any recent | The vendored vcpkg is a submodule |
| PowerShell 7 (`pwsh`) | 7.0 or newer | For the [platform-isolation check](platform-isolation.md#enforcement). Optional: without it the `platform.isolation` test is not registered |
| clang-tidy | 23.1.3 | Only for the `linux-gcc-tidy` preset: the PyPI wheel pinned in `.github/requirements/static-analysis.txt` ([nightly analysis](static-analysis.md)) |
| Network access | | The first configure bootstraps vcpkg and builds Catch2 |
| Qt | 6.12 | Only for the application (`-DADL_BUILD_APP=ON`): the prebuilt kit, found through `QT_ROOT_DIR`. See [Building the application](#building-the-application) |

## Clone

```sh
git clone --recurse-submodules https://github.com/iainchesworthlabs/arinc-data-loader
cd arinc-data-loader
```

vcpkg is the git submodule `deps/vcpkg` (the labs' fork of vcpkg), so the
dependency provider is pinned by the commit and nothing has to be installed
machine-wide. If you cloned without `--recurse-submodules`, the first configure
initialises it for you when it finds a `.git` directory; or do it yourself:

```sh
git submodule update --init --depth 1 deps/vcpkg
```

On a machine that keeps one shared vcpkg (`C:\Repos\vcpkg`), `deps/vcpkg` is a
junction to it, and the submodule is not updated:

```powershell
. C:\Repos\common\env.ps1      # VCPKG_DOWNLOADS, the shared binary cache, QT_ROOT_DIR
cmd /c mklink /J deps\vcpkg C:\Repos\vcpkg
```

## First build

Run these from a shell where the compiler is available: on Windows a Developer
PowerShell for Visual Studio (the Ninja generator does not find MSVC by itself).

```sh
# Windows
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug

# Linux
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug

# macOS
cmake --preset macos-clang-debug
cmake --build --preset macos-clang-debug
ctest --preset macos-clang-debug
```

The build trees are `build/<preset>`, and the programs are in
`build/<preset>/bin`.

## Building the application

`ADL_BUILD_APP` builds all three Qt applications, not only the loader: **`adl_app`** (the data
loader, [described here](app.md)), **`adl_media_gen`** (the media set generator,
[described here](media-gen.md)) and **`adl_mock_target`** (a 615A target over real sockets,
[described here](mock-target.md)). They are off by default, so that the libraries build on a
machine with no Qt. They need the prebuilt Qt 6.12 kit, not vcpkg: `cmake/FindQt6.cmake` looks in
`QT_ROOT_DIR` first (on a labs machine `. C:\Repos\common\env.ps1` sets it to
`C:\Repos\common\qt\6.12.0\msvc2022_64`), then in the places the platform's fragment lists for an
aqtinstall layout (`C:/Qt`, `~/Qt`, `/opt/Qt`), and an explicit `-DCMAKE_PREFIX_PATH=...` always wins.

```sh
cmake --preset windows-msvc-debug -DADL_BUILD_APP=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
build/windows-msvc-debug/bin/adl_app
build/windows-msvc-debug/bin/adl_media_gen
build/windows-msvc-debug/bin/adl_mock_target
```

On Windows the build runs windeployqt after linking, so the programs start from anywhere (Qt's DLLs,
its plugins and the QML modules the windows import are copied beside them). On Linux they find Qt the
way the build did; on macOS the programs are bundles, and packaging them (`macdeployqt`) is not done yet.

The tests that need Qt are `app.tests` and `app.qml`, `media_gen.tests` and `media_gen.qml`, and
`mock.engine`, `mock.model` and `mock.qml`; they run in the same `ctest` as the rest when
`ADL_BUILD_APP` is on, and look for the offscreen platform plugin in the kit's own `plugins`
directory. The `loopback.integration` test runs the loader's runner over real sockets on
127.0.0.1 against the mock target's engine (nothing below port 1024, a few seconds per
operation); it is labelled `integration` and needs `ADL_BUILD_APP` too.

### The demonstration

The mock target and the loader are two programs, so they can run on one machine (the target's
transfer ports start at 40000 and grow, and it honors the loader's `port` option) or on two.
On one machine:

1. Start `adl_mock_target`, set the identity and the storage directory as you like, press **Start**.
2. Start `adl_app`. In **File > Preferences…**, set the control port to anything but 59
   (say 5900) so the loader and the target do not fight over port 59, and type the target's address
   (`127.0.0.1`) and `<THW_ID_POS>` (`HNPFMS_L` for the default identity). FIND on one machine is
   not reliable (two sockets sharing port 1001), so name the target by hand.
3. Generate a media set with `adl_media_gen`, or use `adl_write_payloads` to write the sample sets,
   open its directory in the loader (**Media set > Choose directory…**), wait for the check, tick a
   load and **Upload**. The mock target's window shows each file arrive, and stores them under its
   storage directory.
4. To watch the loader cope, set an injection in the mock target (say "answer WAIT", or "go
   silent"), run the operation again, and watch the loader's log and the failure dialog.

On Linux and macOS binding ports 59 and 1001 may need privileges; the mock target's ports are
editable, and the loader takes the target's port from the address you type.

## Writing sample payloads

`adl_write_payloads` writes the sample media sets (the scenario of the golden
tests and the ARINC 615 and Boeing 777 layouts of Appendix F) to a directory, one
directory per scenario and one per media set member, for trying other loaders:

```sh
build/windows-msvc-debug/bin/adl_write_payloads --list
build/windows-msvc-debug/bin/adl_write_payloads out/payloads
build/windows-msvc-debug/bin/adl_write_payloads --scenario arinc615 out/payloads
```

It reads each set back from the disk and verifies it before reporting it, and it
does not write over a directory that has files in it.

## Presets

| Preset | Compiler | Notes |
| --- | --- | --- |
| `windows-msvc-debug`, `windows-msvc-release` | MSVC | triplet `x64-windows` |
| `windows-msvc-analyze` | MSVC | Debug with `/analyze` (MSVC Code Analysis); tests and applications on; nightly |
| `linux-gcc-debug`, `linux-gcc-release` | GCC | triplet `x64-linux` |
| `linux-gcc-coverage` | GCC | Debug with `--coverage`; sanitizers off; the `coverage` target runs the tests and enforces 90% lines / 80% branches ([Testing](testing.md#coverage)) |
| `linux-gcc-tidy` | GCC + clang-tidy | clang-tidy is the compiler launcher, warnings are errors; tests and applications off; nightly |
| `linux-gcc-sonar` | GCC | Configure only, to write the `compile_commands.json` the optional SonarQube scan reads |
| `linux-clang-debug` | Clang | Debug ASan/UBSan; tests on, applications off; nightly |
| `macos-clang-debug`, `macos-clang-release` | AppleClang | triplet `arm64-osx` |
| `ghs508-ghs-debug`, `ghs508-ghs-release` | GHS `cxppc` | INTEGRITY 5.0.8 cross build; `ADL_ACCEPT_UNBUILT_PLATFORM=ON`, tests and the app off; needs `INTEGRITY_ROOT`/`INTEGRITY_BSP_ROOT`/`INTEGRITY_TOOLCHAIN_ROOT` (or the compilers on PATH) |
| `ghs178b-ghs-debug`, `ghs178b-ghs-release` | GHS `cxppc` | INTEGRITY-178B cross build; as above, with `INTEGRITY178_ROOT` |

Each preset is available only on its own host. Debug presets turn on
AddressSanitizer (and UndefinedBehaviorSanitizer where the compiler has it),
except the analysis presets (`windows-msvc-analyze`, `linux-gcc-tidy`,
`linux-gcc-sonar`), which turn the sanitizers off on purpose; every first-party
target is built with warnings as errors.

## Options

| Option | Default | Does |
| --- | --- | --- |
| `ADL_BUILD_TESTS` | `ON` | Builds the unit tests and pulls Catch2 from vcpkg |
| `ADL_ENABLE_SANITIZERS` | `ON` | ASan (and UBSan) in Debug |
| `ADL_ENABLE_COVERAGE` | `OFF` | gcov-style instrumentation and the `coverage` target (the coverage preset sets it) |
| `ADL_COVERAGE_LINE_THRESHOLD` | `90` | The line coverage the `coverage` target requires (percent) |
| `ADL_COVERAGE_BRANCH_THRESHOLD` | `80` | The branch coverage the `coverage` target requires (percent) |
| `ADL_ENABLE_CLANG_TIDY` | `OFF` | Runs clang-tidy as the compiler launcher for the first-party targets under `src/`, with `.clang-tidy` and warnings as errors (the tests clear the launcher; the `linux-gcc-tidy` preset sets it) |
| `ADL_ENABLE_MSVC_ANALYSIS` | `OFF` | Adds MSVC Code Analysis (`/analyze /analyze:external-`) to every first-party target; MSVC only (the `windows-msvc-analyze` preset sets it) |
| `ADL_ENABLE_TRACY` | `OFF` | Links Tracy and pulls it from vcpkg; use a Release build for numbers worth reading |
| `ADL_TARGET_PLATFORM` | empty | Names the platform for a cross build, where the toolchain does not (`windows`, `linux`, `macos`, `qnx`, `ghs508`, `ghs178b`, `vxworks`, `freertos`, `zephyr`). A desktop build leaves it empty. See [platform isolation](platform-isolation.md#how-cmake-selects) |
| `ADL_ACCEPT_UNBUILT_PLATFORM` | `OFF` | Allows building a platform whose code has never been compiled (QNX and the two INTEGRITY ports). A platform with no code at all is refused whatever this says |

The Green Hills INTEGRITY platforms are [reviewed in
docs/platform-ghs.md](platform-ghs.md): what the two source trees provide, how
`cmake/FindIntegrity.cmake` and `cmake/FindIntegrity178.cmake` find them, and
what is still open before a first compile. They have never been compiled and
their code says so.

## Formatting

The repository's `.clang-format` is applied with one pinned release:

```sh
pip install -r .github/requirements/lint.txt
clang-format -i $(git ls-files 'src/*.cpp' 'src/*.hpp' 'tests/*.cpp')
```

## Static analysis

The same presets CI's nightly analysers use run locally; the pinned clang-tidy
is the one from `.github/requirements/static-analysis.txt`:

```sh
pip install -r .github/requirements/static-analysis.txt
cmake --preset linux-gcc-tidy
cmake --build --preset linux-gcc-tidy

cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

On Windows, from a Developer PowerShell:

```sh
cmake --preset windows-msvc-analyze
cmake --build --preset windows-msvc-analyze
```

[Nightly static and dynamic analysis](static-analysis.md) has the jobs, the
suppression rules and the SonarQube setup. Compiler warnings are errors in every
build regardless of these switches.
