# Building

This page builds ARINC Data Loader from source. What CI runs is in [CI](ci.md).

Only Windows with MSVC 19.51 (Visual Studio 2026) has been built and tested by
hand so far; Linux (GCC 15, in a container) has been built and tested too. The
Linux and macOS presets and platform code are written to the same pattern and run
in CI; macOS has never been tried by hand.

## Requirements

| What | Version | Notes |
| --- | --- | --- |
| CMake | 4.0 or newer | The presets require it. 4.4.0 was used |
| Ninja | any recent | The presets use the Ninja generator |
| A C++23 compiler | MSVC from Visual Studio 2022 17.x or Visual Studio 2026; GCC 14 or newer; AppleClang 16 or newer | The code uses `std::expected` and `<span>`. The `linux-gcc-*` presets set the compiler to `g++`, so that is the one used; with an older `g++` on the path, configure with `-DCMAKE_CXX_COMPILER=<path to a newer g++>` |
| Git | any recent | The vendored vcpkg is a submodule |
| PowerShell 7 (`pwsh`) | 7.0 or newer | For the [platform-isolation check](platform-isolation.md#enforcement). Optional: without it the `platform.isolation` test is not registered |
| Python 3 | | The project's own CMake does not use it. It installs the pinned tools with `pip` (clang-format, clang-tidy, gcovr, MkDocs: the files in `.github/requirements`) and, when someone regenerates the golden payloads by hand, runs `tests/golden/make_golden.py` and `make_golden_615a.py` |
| clang-tidy | 23.1.3 | Only for the `linux-gcc-tidy` preset: the PyPI wheel pinned in `.github/requirements/static-analysis.txt` ([nightly analysis](static-analysis.md)) |
| Network access | | The first configure bootstraps vcpkg and builds Catch2 |
| Qt | 6.12 | Only for the applications (`-DADL_BUILD_APP=ON`): the prebuilt desktop kit, with the modules and system libraries listed under [Building the application](#building-the-application) |

The macOS presets set the vcpkg triplet `arm64-osx` and a deployment target of macOS 14.4, so they are
for Apple silicon; an Intel Mac is not covered.

## Clone

```sh
git clone --recurse-submodules https://github.com/iainchesworthlabs/arinc-data-loader
cd arinc-data-loader
```

vcpkg is the git submodule `deps/vcpkg` (a fork of vcpkg that the project's organisation keeps), so the
dependency provider is pinned by the commit and nothing has to be installed
machine-wide. If you cloned without `--recurse-submodules`, the first configure
initialises it for you when it finds a `.git` directory; or do it yourself:

```sh
git submodule update --init deps/vcpkg
```

The submodule is cloned with its whole history (about 125 MB), and not with `--depth 1`. The manifest's
`builtin-baseline` (`vcpkg.json`) is a vcpkg commit other than the one the submodule pins, and vcpkg
reads that commit and the older versions of each port out of the submodule's history. In a shallow
submodule the first configure of a build with tests (the default, which needs Catch2) stops with
"failed to `git show` versions/baseline.json". If `deps/vcpkg` is shallow already, deepen it and
configure again:

```sh
git -C deps/vcpkg fetch --unshallow
cmake --fresh --preset <name>
```

A clone made this way needs nothing else for a libraries-only build, and no shared store:
`cmake --preset <name> -DADL_BUILD_APP=OFF` downloads and builds Catch2 on the first configure and
needs no Qt.

## Sharing vcpkg between checkouts

By default every checkout has its own copy of vcpkg: the `deps/vcpkg`
submodule. That is what a fresh clone and CI use, and nothing needs setting up.
If you keep several checkouts or worktrees (or several repositories that use
vcpkg), that is a lot of copies. Point them at one shared **store** instead;
set this once per machine, in your environment:

```sh
# Windows (new terminals and Visual Studio pick it up after a restart)
setx ICL_VCPKG_STORE C:\icl\vcpkg
# Linux/macOS: put this in your shell profile
export ICL_VCPKG_STORE=$HOME/.cache/icl/vcpkg
```

`ICL_VCPKG_STORE` is read by every Iain Chesworth Labs repository that carries
the same `cmake/VcpkgRoot.cmake`, so they all share the one directory.
`ADL_VCPKG_STORE` overrides it for ARINC Data Loader alone.

The first configure clones the fork the submodule points at into
`<store>/mirror`, and checks out the commit this checkout pins into
`<store>/<first 12 characters of the commit>`. Every checkout (of this
repository or another) that pins the same commit uses that directory; when a pin
moves (a vcpkg bump), the new commit gets a directory of its own, and nothing
that already exists is touched. That is the point of a directory per commit
rather than one shared checkout: a checkout of another branch, or a build that
is running, never has vcpkg change under it. A repository whose submodule points
at a different fork has its commit fetched into the same mirror. Directories of
old commits can be deleted when you no longer need them
(`git -C <store>/mirror worktree remove <dir>`).

Already have a full clone of your fork somewhere? Set `ICL_VCPKG_MIRROR` (or
`ADL_VCPKG_MIRROR`) to it and the store makes its checkouts from that instead of
cloning again.

A build directory remembers the vcpkg it was first configured with, so after
the pin moves, configure it again with `cmake --fresh --preset <name>` (CMake
says so). Build artifacts are separate from all this: share the downloads,
binary cache and installed tree with `VCPKG_DOWNLOADS`,
`VCPKG_BINARY_SOURCES` and `VCPKG_INSTALLED_DIR`, as vcpkg documents.

Two other ways in, for when the store does not fit:

- `ADL_VCPKG_ROOT` names one vcpkg checkout you manage yourself. There is no
  organisation-wide equivalent on purpose: one directory cannot be at the commit
  every repository pins. A different commit from the pin is a warning, since a
  baseline newer than the scripts it is built with can fail.
- `-DCMAKE_TOOLCHAIN_FILE=...` (or an IDE that sets it) is used as it is.

Avoid pointing `deps/vcpkg` itself at a shared directory with a junction or
symlink: deleting that checkout (or a worktree of it) can then empty the shared
copy for everyone, and a dead link makes git report only that it "cannot find
the current revision". The configure says which link is dead.

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

## What CMake builds

The build has a source side and a target side, and the libraries sit on both.

| What | Targets | Added by | Runs on |
| --- | --- | --- | --- |
| Portable libraries | `adl::platform`, `adl::arinc665`, `adl::arinc615a`, `adl::arinc615a_mini`, `adl::arinc615a_runner`, `adl::media_tree`, `adl::samples` | always | source and target |
| Payload writer | `adl_write_payloads` | always | source (desktop) |
| Source-side applications (Qt) | `adl_app` (the data loader), `adl_media_gen` (the media set generator), `adl_mock_target` (a 615A target, for demonstrations) | `ADL_BUILD_APP`, on by default on a desktop | source (desktop) |
| Test programs | `adl_unit_tests`, `adl_platform_tests`, `adl_engine_tests`; with the applications also `adl_app_tests`, `adl_app_qml_test`, `adl_ui_tests`, `adl_window_state_tests`, `adl_gen_tests`, `adl_gen_qml_test`, `adl_mock_tests`, `adl_mock_model_test`, `adl_mock_qml_test`, `adl_loopback_test` | `ADL_BUILD_TESTS=ON` | source (desktop) |
| Target side | the libraries above, for the target's platform | `ADL_TARGET_PLATFORM` (or a toolchain that sets it) | target (embedded) |

The **source side** is the operator's machine: the loader, the generator and the payload writer. The
**target side** is the aircraft equipment: the libraries build for it — the 615A engine is sans-I/O and
the platform layer has a fragment per platform — but there is no target-side application yet. The mock
target stands in for one in demonstrations. A target-side program (answering `.LCI` and writing its
status files) is on the [roadmap](roadmap.md#3-arinc-615a); it would be the fourth kind of output,
selected the way the applications are.

## Building the application

`ADL_BUILD_APP` builds all three Qt applications, not only the loader: **`adl_app`** (the data
loader, [described here](app.md)), **`adl_media_gen`** (the media set generator,
[described here](media-gen.md)) and **`adl_mock_target`** (a 615A target over real sockets,
[described here](mock-target.md)). They are the source side's programs, so they build **by default
on Windows, Linux and macOS** — the desktops are the only place Qt exists — provided the Qt 6.12
kit can be found. A target build (`ADL_TARGET_PLATFORM` names an RTOS) has no Qt and leaves them
off; a desktop where the kit is not found configures the libraries alone, says so in the status
block, and `-DADL_BUILD_APP=ON` turns that absence into an error, while `-DADL_BUILD_APP=OFF` is
the deliberate libraries-only build.

They need the prebuilt kit, not vcpkg, and `find_package(Qt6 6.12 ...)` has to succeed. The programs
use the modules Core, Gui, Qml, Quick, Quick Controls 2 and Linguist Tools (`lupdate` and `lrelease`),
and their tests Test and Quick Test. CI installs the desktop kit for each system with
`.github/actions/install-qt/install_qt.py`, which checks every archive against a SHA-256 pinned in this
repository ([CI](ci.md#toolchain-choices)).

`cmake/FindQt6.cmake` looks in `QT_ROOT_DIR` first (set it to the kit's directory, the one that holds
`lib/cmake/Qt6`), then in the places the platform's fragment lists (`src/platforms/<name>/platform.cmake`)
for a kit laid out as `<root>/6.x/<kit directory>`:

| System | Usual roots | Kit directories |
| --- | --- | --- |
| Windows | `C:/Qt`, `%USERPROFILE%/Qt` | `msvc2022_64`, `msvc2019_64` |
| Linux | `~/Qt`, `/opt/Qt`, `/usr/lib/qt6`, `/usr/lib/x86_64-linux-gnu/qt6` | `gcc_64` |
| macOS | `~/Qt`, `/opt/Qt`, `/usr/local/opt/qt6`, `/opt/homebrew/opt/qt6` | `macos`, `clang_64` |

A Qt installed directly under a root (`<root>/lib/cmake/Qt6`, which is how a system package such as
`qt6-base-dev` lays it out) is found too. A kit in one of the roots above needs no `QT_ROOT_DIR`, so an IDE that
does not pass it (Visual Studio's CMake integration, for one) still finds the kit. An explicit
`-DCMAKE_PREFIX_PATH=...` always wins.

On Linux the prebuilt kit also needs some of the system's libraries. Configuring against it asks for
OpenGL's headers and `libGL.so`, and the xkbcommon, EGL and fontconfig development packages bring the
libraries it links to. CI installs `libgl-dev`, `libegl-dev`, `libxkbcommon-dev`, `libfontconfig-dev`,
`libdbus-1-3` and `libglib2.0-0t64` with apt (`.github/actions/install-qt/action.yml`).

So the ordinary desktop presets contain the applications and their tests, and an IDE that reads
`CMakePresets.json` (Visual Studio, VS Code, CLion) lists them:

```sh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
build/windows-msvc-debug/bin/adl_app
build/windows-msvc-debug/bin/adl_media_gen
build/windows-msvc-debug/bin/adl_mock_target
```

The same without a preset:

```sh
cmake --preset windows-msvc-debug -DADL_BUILD_APP=OFF   # libraries only
cmake --preset windows-msvc-debug                       # applications and their tests
```

On Windows the build runs windeployqt after linking, so the programs start from anywhere (Qt's DLLs,
its plugins and the QML modules the windows import are copied beside them). On Linux they find Qt the
way the build did; on macOS the programs are application bundles that run against the Qt installation they
were built with, and `cmake --install` runs `macdeployqt` on each bundle to copy Qt's frameworks, plugins and
QML modules into it, which is how the disk image under [Packages](#packages) is made.

The tests that need Qt are `app.tests` and `app.qml`, `media_gen.tests` and `media_gen.qml`, and
`mock.engine`, `mock.model` and `mock.qml`; they run in the same `ctest` as the rest when
`ADL_BUILD_APP` is on, and look for the offscreen platform plugin in the kit's own `plugins`
directory. The `loopback.integration` test runs the loader's runner over real sockets on
127.0.0.1 against the mock target's engine (nothing below port 1024, a few seconds per
operation); it is labelled `integration` and needs `ADL_BUILD_APP` too.

### The demonstration

The mock target and the loader are two programs, so they can run on one machine (the target's
transfer ports start at 40000 and grow, and it honors the loader's `port` option) or on two.
[The user's guide](user-guide/demonstration.md) takes you through it step by step, with the
generator making the media set. In short, on one machine:

1. Start `adl_mock_target`, choose a storage directory and press **Start**.
2. Generate a media set with `adl_media_gen`, or write the samples with `adl_write_payloads`.
3. Start `adl_app`, press **Find** (or type the target's address `127.0.0.1` and
   `<THW_ID_POS>` `HNPFMS_L`), choose the media set directory, tick a load and **Upload**.
4. To watch the loader cope, turn on a fault injection in the mock target (stop it, set the
   fault, start it again) and run the operation again.

On Linux and macOS binding ports 59 and 1001 may need privileges; the mock target's ports are
editable, and the loader takes the target's port from Preferences (anything but 59 is sent to the
target as the `port` option).

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
| `windows-msvc-debug`, `windows-msvc-release` | MSVC | triplet `x64-windows`; the applications are on |
| `windows-msvc-analyze` | MSVC | Debug with `/analyze` (MSVC Code Analysis); tests and applications on; nightly |
| `linux-gcc-debug`, `linux-gcc-release` | GCC | triplet `x64-linux`; the applications are on |
| `linux-gcc-coverage` | GCC | Debug with `--coverage`; sanitizers off; the `coverage` target runs the tests and enforces 90% lines / 80% branches ([Testing](testing.md#coverage)) |
| `linux-gcc-tidy` | GCC + clang-tidy | clang-tidy is the compiler launcher, warnings are errors; tests and applications off; nightly |
| `linux-gcc-sonar` | GCC | Configure only, to write the `compile_commands.json` the optional SonarQube scan reads |
| `linux-clang-debug` | Clang | Debug ASan/UBSan; tests on, applications off; nightly |
| `macos-clang-debug`, `macos-clang-release` | AppleClang | triplet `arm64-osx`; the applications are on |
| `ghs508-ghs-debug`, `ghs508-ghs-release` | GHS `cxppc` | INTEGRITY 5.0.8 cross build; `ADL_ACCEPT_UNBUILT_PLATFORM=ON`, tests and the applications off (a target build); needs `INTEGRITY_ROOT`/`INTEGRITY_BSP_ROOT`/`INTEGRITY_TOOLCHAIN_ROOT` (or the compilers on PATH) |
| `ghs178b-ghs-debug`, `ghs178b-ghs-release` | GHS `cxppc` | INTEGRITY-178B cross build; as above, with `INTEGRITY178_ROOT` |

A Debug build of the applications cannot be used to open the system's own file dialogs on
Windows: under AddressSanitizer the dialog code (`ntshrui.dll`, reached from the common file
dialog) trips a heap-buffer-overflow report and the program exits with status 1 when "Choose..." is
pressed. Use a Release build (`windows-msvc-release`) to drive the windows by hand or to
[regenerate the screenshots](screenshots.md).

The Windows, Linux and macOS presets are available only on their own host. The
`ghs*` presets have no such condition, so every host lists them; they need the
INTEGRITY toolchain. The desktop Debug presets turn on AddressSanitizer (and
UndefinedBehaviorSanitizer where the compiler has it), except the analysis
presets (`windows-msvc-analyze`, `linux-gcc-coverage`, `linux-gcc-tidy`,
`linux-gcc-sonar`), which turn the sanitizers off on purpose; every first-party
target is built with warnings as errors.

## Options

| Option | Default | Does |
| --- | --- | --- |
| `ADL_BUILD_TESTS` | `ON` | Builds the unit, integration and system suites and pulls Catch2 from vcpkg |
| `ADL_BUILD_APP` | ON on a desktop when Qt is found, OFF otherwise | Builds the three Qt applications and their tests (`-DADL_BUILD_APP=OFF` on a desktop is the libraries-only build; `ON` with no kit is an error) |
| `ADL_ENABLE_SANITIZERS` | `ON` | ASan (and UBSan) in Debug |
| `ADL_ENABLE_COVERAGE` | `OFF` | gcov-style instrumentation and the `coverage` target (the coverage preset sets it) |
| `ADL_COVERAGE_LINE_THRESHOLD` | `90` | The line coverage the `coverage` target requires (percent) |
| `ADL_COVERAGE_BRANCH_THRESHOLD` | `80` | The branch coverage the `coverage` target requires (percent) |
| `ADL_ENABLE_CLANG_TIDY` | `OFF` | Runs clang-tidy as the compiler launcher for the first-party targets under `src/`, with `.clang-tidy` and warnings as errors (the tests clear the launcher; the `linux-gcc-tidy` preset sets it) |
| `ADL_ENABLE_MSVC_ANALYSIS` | `OFF` | Adds MSVC Code Analysis (`/analyze /analyze:external-`) to every first-party target; MSVC only (the `windows-msvc-analyze` preset sets it) |
| `ADL_ENABLE_TRACY` | `OFF` | Links Tracy and pulls it from vcpkg; use a Release build for numbers worth reading |
| `ADL_TARGET_PLATFORM` | empty | Names the platform for a cross build, where the toolchain does not (`windows`, `linux`, `macos`, `qnx`, `ghs508`, `ghs178b`, `vxworks`, `freertos`, `zephyr`). A desktop build leaves it empty. `freertos` and `zephyr` are accepted and refused: their images are built by projects of their own ([below](#zephyr-and-freertos)). See [platform isolation](platform-isolation.md#how-cmake-selects) |
| `ADL_ACCEPT_UNBUILT_PLATFORM` | `OFF` | Allows building a platform whose code has never been compiled (QNX and the two INTEGRITY ports). A platform with no code at all is refused whatever this says |

The Green Hills INTEGRITY platforms are [reviewed in
docs/platform-ghs.md](platform-ghs.md): what the two source trees provide, how
`cmake/FindIntegrity.cmake` and `cmake/FindIntegrity178.cmake` find them, and
what is still open before a first compile. They have never been compiled and
their code says so.

## Packages

`cmake --install` and CPack make the packages a release ships ([Releasing](releasing.md) says what is
in them and why). With a Release build of the applications:

```sh
cmake --preset linux-gcc-release            # or windows-msvc-release, macos-clang-release
cmake --build --preset linux-gcc-release
cd build/linux-gcc-release && cpack -B packages
pwsh ../../tools/release/Test-Package.ps1 -Package packages/arinc-data-loader-0.1.0-linux-x86_64.tar.gz
```

`cpack` writes the runtime package (the three applications, `adl_write_payloads`, Qt's runtime files,
`README.txt`, `LICENSE.txt`, `NOTICES.txt`) and the development package (the libraries, headers and the `adl`
CMake package) for this system; `-DADL_RELEASE_VERSION=0.2.0-rc.1` names them for a pre-release. To use the
development package, `find_package(adl 0.1 REQUIRED)` with its directory on `CMAKE_PREFIX_PATH`. The macOS
presets set `CMAKE_OSX_DEPLOYMENT_TARGET` to 14.4, the oldest macOS Qt 6.12 supports; a build for an older one
would fail Qt's own check, so it is set on purpose and not left to the SDK. A Debug build installs libraries
compiled with the sanitizers, which only a program built with them can link; package from a Release build.

## Zephyr and FreeRTOS

Both are *experimental*: built, and their tests run, on a simulator in CI, and nowhere else
([what that means](platform-abstraction.md#the-freertos-and-zephyr-ports)). An RTOS image is built around
the product's kernel configuration, so neither is a configuration of this project's own CMake; each has a
program of its own that tests it, and a product does what that program does with its own board.

### Zephyr

The platform layer is a Zephyr module: `zephyr/module.yml` names `src/platforms/zephyr` as its CMake
directory and its Kconfig. To use it, add this repository to the application's west manifest (or name it
in `ZEPHYR_EXTRA_MODULES`) and set, in `prj.conf`:

```text
CONFIG_CPP=y
CONFIG_STD_CPP23=y
CONFIG_ADL_PLATFORM=y
```

`CONFIG_ADL_PLATFORM` depends on, and does not select, the network stack (`CONFIG_NETWORKING`,
`CONFIG_NET_IPV4`, `CONFIG_NET_UDP`, `CONFIG_NET_SOCKETS`) and `CONFIG_FILE_SYSTEM`: selecting
`NETWORKING` from a module makes a dependency loop through C++ and OpenThread. It selects the full C++
library (`CONFIG_REQUIRES_FULL_LIBCPP`), dynamic threads and the socket options the port sets. The
application mounts the file system the loader writes to, as it would for anything else, before it asks for
a `Host`, and sizes the pools: a TFTP block can be 65 464 bytes, so a stack that must carry one needs
`CONFIG_NET_BUF_*` and the interface MTU to match (`tests/platform/zephyr/prj.conf` is a working set for
native_sim, with the loopback interface's MTU at 65 535). The module's own options are
`CONFIG_ADL_HEAP_BYTES`, `CONFIG_ADL_THREAD_STACK_BYTES`, `CONFIG_ADL_FS_COHERENT_HANDLES`,
`CONFIG_ADL_WARNINGS_AS_ERRORS` (the loader's sources are built with `-Werror` and the rest of the
project's warning set only where this is on; CI's configurations turn it on, and a product should leave it
off, since another compiler or optimisation level can warn about correct code) and `CONFIG_ADL_LIBRARIES`
(build the ARINC 665 and 615A libraries into the image; default on).

The test program, built as any Zephyr application is, for native_sim:

```sh
# once: a west workspace with Zephyr at the version tests/platform/zephyr/west.yml pins
#   (the repository is the manifest repository, so put it in a directory of its own)
west init -l --mf tests/platform/zephyr/west.yml <the repository directory>
west update --narrow --fetch-opt=--depth=1
pip install -r <workspace>/zephyr/scripts/requirements-base.txt

# each time
ZEPHYR_BASE=<workspace>/zephyr tools/ci/zephyr_native_sim.sh
```

That script is what CI runs. It needs `west` (pinned in `.github/requirements/zephyr.txt`), CMake, Ninja,
a C++23 `g++` (the host compiler is the toolchain: `ZEPHYR_TOOLCHAIN_VARIANT=host`), the device tree
compiler (`dtc`) and `gperf`, and is for Linux or WSL. It builds the program for `native_sim/native/64`,
runs it in a directory of its own, and exits with Catch2's status. Pass Catch2's arguments in
`ADL_CATCH_ARGS` (`ADL_CATCH_ARGS="[conformance] --durations yes"`); `zephyr.exe --rt` runs the clock in
real time instead of as fast as the host allows.

A smoke test of the platform layer on a Cortex-M3 under QEMU, built with the Zephyr SDK's own toolchain
(GCC, picolibc, no exceptions), is `tools/ci/zephyr_qemu_smoke.sh`; it needs the SDK
(`tools/ci/install_zephyr_sdk.sh <directory>`, which downloads and checks the pinned release files, then
`ZEPHYR_SDK_INSTALL_DIR` pointing at the directory) as well as
`ZEPHYR_BASE`, builds `tests/platform/zephyr_smoke` for `mps2/an385`, runs it in `qemu-system-arm` (the SDK's
or the machine's) and exits 0 only if the program reported no failure.

The whole test program on a board is `tools/ci/zephyr_qemu_suite.sh`: it builds `tests/platform/zephyr` for
`qemu_cortex_a9` with the same SDK (GCC 14.3, picolibc, the SDK's libstdc++ with exceptions and RTTI), runs
it in the SDK's `qemu-system-xilinx-aarch64` (a distribution's QEMU has no machine for this board) and exits 0
only if Catch2 reported that every test passed. It needs the same setup as the smoke test; a run takes a few
minutes to build and about five seconds in QEMU. The board's settings are in
`tests/platform/zephyr/boards/qemu_cortex_a9.conf` and `.overlay`: picolibc with a 32 MB malloc arena, a larger
kernel address space for the MMU, Zephyr's POSIX layer for what libstdc++ asks of the C library, and a
simulated flash for littlefs, since the board has none.

### FreeRTOS

`tests/platform/freertos` is a CMake project that does what a product's CMake does, on a host: it
fetches FreeRTOS-Kernel, lwIP and littlefs at pinned versions and hashes, builds them with the
configuration in `tests/platform/freertos/config.cmake` (FreeRTOSConfig.h, lwipopts.h and lwIP's
`cc.h`, written as text because this repository's source has no `#define`), builds the platform layer from
the same fragment the other platforms use and links them with a `main` that starts the scheduler and runs
Catch2 in a task:

```sh
tools/ci/freertos_posix_sim.sh          # Linux; needs CMake 4, Ninja and a C++23 g++
```

A product does the same with its own kernel port and its own configuration. What the platform layer asks of
them:

- **FreeRTOS**: `INCLUDE_vTaskDelete`, `INCLUDE_vTaskDelay`, mutexes, counting semaphores, queues, dynamic
  allocation (a heap: `pvPortMalloc` serves the memory provider) and, for a thread made on storage the
  caller supplies, static allocation. Task functions must not be `noexcept` frames that a cancellation could
  unwind through (only the POSIX simulator port does that).
- **lwIP**: the sockets API (`LWIP_SOCKET`), `LWIP_SOCKET_POLL`, UDP and IPv4, `SO_REUSE`, `LWIP_SO_RCVBUF`,
  `LWIP_TCPIP_CORE_LOCKING`, and mailboxes and pools big enough for the datagrams in flight (a 65 507 byte
  datagram is one pbuf). Its own `sys_arch` is the product's (`tests/platform/freertos/sim` has the
  simulator's).
- **littlefs**: built with `LFS_THREADSAFE` and a lock and unlock in its configuration, mounted before the
  loader asks for a `Host`.

The product tells the platform layer about the file system and the priority of the network task with
`adl::platform::freertos::set_environment` (`src/platforms/freertos/environment.hpp`), once, before
`make_host()`. The simulator's `main` is `tests/platform/freertos/sim/main.cpp`.

## The documentation site

The pages in `docs/` are built into a website with MkDocs. Install it from the hash-locked requirements and
build strictly, which is what CI does ([Documentation site](ci.md#documentation-site)):

```sh
python -m pip install --require-hashes -r .github/requirements/docs.txt
mkdocs serve                                   # open http://127.0.0.1:8000 while you edit
mkdocs build --strict                          # writes ./site
cmake --build build/<preset> --target docs     # the same, into build/<preset>/site
```

## Formatting

The repository's `.clang-format` is applied with one pinned release:

```sh
pip install -r .github/requirements/lint.txt
clang-format -i $(git ls-files 'src/*.cpp' 'src/*.hpp' 'src/*.h' 'tests/*.cpp' 'tests/*.hpp' 'tests/*.h')
```

The CI job checks every file under `src/` and `tests/` with one of the extensions `.cpp`, `.hpp`, `.h`,
`.cc`, `.hh`, `.cxx`, `.hxx`, `.inl` and `.ipp` ([CI](ci.md#the-jobs-in-ciyml)); the command above names the
three that the tracked files use.

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
