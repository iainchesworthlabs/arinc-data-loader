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

The Qt application (`src/app`, [described here](app.md)) is off by default, so that the libraries build
on a machine with no Qt. It needs the prebuilt Qt 6.12 kit, not vcpkg: `cmake/FindQt6.cmake` looks in
`QT_ROOT_DIR` first (on a labs machine `. C:\Repos\common\env.ps1` sets it to
`C:\Repos\common\qt\6.12.0\msvc2022_64`), then in the places the platform's fragment lists for an
aqtinstall layout (`C:/Qt`, `~/Qt`, `/opt/Qt`), and an explicit `-DCMAKE_PREFIX_PATH=...` always wins.

```sh
cmake --preset windows-msvc-debug -DADL_BUILD_APP=ON
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
build/windows-msvc-debug/bin/adl_app
```

On Windows the build runs windeployqt after linking, so `adl_app.exe` starts from anywhere (Qt's DLLs,
its plugins and the QML modules the window imports are copied beside it). On Linux it finds Qt the way
the build did; on macOS the program is a bundle, and packaging it (`macdeployqt`) is not done yet.

The tests that need Qt are `app.tests` (QtTest, headless) and `app.qml` (the window loaded offscreen);
they run in the same `ctest` as the rest when `ADL_BUILD_APP` is on, and look for the offscreen platform
plugin in the kit's own `plugins` directory.

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
| `linux-gcc-debug`, `linux-gcc-release` | GCC | triplet `x64-linux` |
| `linux-gcc-coverage` | GCC | Debug with `--coverage`; sanitizers off |
| `macos-clang-debug`, `macos-clang-release` | AppleClang | triplet `arm64-osx` |

Each preset is available only on its own host. Debug presets turn on
AddressSanitizer (and UndefinedBehaviorSanitizer where the compiler has it);
every first-party target is built with warnings as errors.

## Options

| Option | Default | Does |
| --- | --- | --- |
| `ADL_BUILD_TESTS` | `ON` | Builds the unit tests and pulls Catch2 from vcpkg |
| `ADL_ENABLE_SANITIZERS` | `ON` | ASan (and UBSan) in Debug |
| `ADL_ENABLE_COVERAGE` | `OFF` | gcov-style instrumentation (the coverage preset sets it) |
| `ADL_ENABLE_TRACY` | `OFF` | Links Tracy and pulls it from vcpkg; use a Release build for numbers worth reading |
| `ADL_TARGET_PLATFORM` | empty | Names the platform for a cross build, where the toolchain does not (`windows`, `linux`, `macos`, `qnx`, `vxworks`, `freertos`, `zephyr`). A desktop build leaves it empty. See [platform isolation](platform-isolation.md#how-cmake-selects) |
| `ADL_ACCEPT_UNBUILT_PLATFORM` | `OFF` | Allows building a platform whose code has never been compiled (today only QNX). A platform with no code at all is refused whatever this says |

## Formatting

The repository's `.clang-format` is applied with one pinned release:

```sh
pip install -r .github/requirements/lint.txt
clang-format -i $(git ls-files 'src/*.cpp' 'src/*.hpp' 'tests/*.cpp')
```
