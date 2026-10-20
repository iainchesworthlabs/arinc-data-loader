# Platform isolation

> **Principle.** The operating system is a CMake decision, not a source-code
> one. Everything that differs between platforms lives in
> `src/platforms/<name>/`, behind a header in `src/platforms/interfaces/`, and
> the platform's own `platform.cmake` says which of those files are compiled and
> what they link against. Shared code - everything else - does not know which
> platforms exist: no `#ifdef`, no OS header, no runtime "which OS am I on".

Adding a platform is therefore two edits: a new `src/platforms/<name>/`
directory, and one branch in the selector, `cmake/Platform.cmake`. Nothing in
`src/arinc665`, the application, the tests or any other CMake file needs to learn the
platform's name.

This is a rule for every contributor, and it is enforced by a check (see
[Enforcement](#enforcement)): a new `#ifdef` in shared code turns the
`platform.isolation` test red.

## The layout

```text
src/platforms/
  interfaces/adl/platform/   what the program needs from a platform: one header each
    paths.hpp, host.hpp, files.hpp, memory.hpp, threads.hpp, time.hpp,
    network.hpp, ipc.hpp, error.hpp, owned.hpp, ring_ipc.hpp
  windows/ linux/ macos/     the platform's own files and its platform.cmake (Linux and macOS
                             take the rest of their implementations from posix/)
  qnx/                       code that has never been compiled (status unbuilt)
  ghs508/ ghs178b/           Green Hills INTEGRITY 5.0.8 and INTEGRITY-178B (unbuilt)
  freertos/ zephyr/          experimental: built and tested on a simulator (docs/platform-abstraction.md)
  vxworks/                   a platform.cmake only: a design, no code
  posix/                     the implementations the POSIX systems share
  ghs/                       the implementations the INTEGRITY platforms share
  detail/                    portable helpers every platform links
  CMakeLists.txt             builds adl_platform from what the fragment names
tests/platform/              the fragment and selector checks, and test cases only one platform can run;
                             the programs that test freertos/ and zephyr/ (zephyr/, freertos/, rtos_support/)
tests/conformance/           the same checks against the real host and the fake
tests/fake/                  the fake platform
```

When two platforms would otherwise copy an implementation, put it in the `posix/`
directory (or `ghs/` for the INTEGRITY ones) beside them and let each platform's
`platform.cmake` name the file. Neither `posix/` nor `ghs/` is a platform, and
neither needs a `platform.cmake` of its own. The POSIX
systems differ in a few places (a condition variable's clock, thread priorities,
flushing a file to disk, a thread's name); those are functions declared in
`posix/posix_os.hpp` and defined once per system (`linux/os_linux.cpp`,
`macos/os_macos.cpp`), so the shared code has no `#ifdef`. `detail/` holds code that
has no operating system in it and that every platform compiles. Reach for a
platform's own directory when the implementations genuinely differ.

## How CMake selects

`cmake/Platform.cmake` is the **only** CMake file that tests `WIN32`, `APPLE` or
`UNIX` (and `ANDROID`, `IOS` and `EMSCRIPTEN`, which it turns into an error
saying which platforms are targets), or `CMAKE_SYSTEM_NAME`. It maps them to a
platform name, includes `src/platforms/<name>/platform.cmake`, and then checks that
the fragment filled in the whole contract (`cmake/PlatformContract.cmake`). Every
other CMake file reads what the fragment declared.

**How the selector learns the target.** A desktop build needs nothing: `WIN32`,
`APPLE` and `UNIX` say which it is, and QNX is recognised from `CMAKE_SYSTEM_NAME`.
An RTOS build is a cross build, and CMake has no system name for FreeRTOS or Zephyr
(their toolchains say "Generic") or for VxWorks, so the target is named with one
cache variable that only the selector reads:

```sh
cmake -DADL_TARGET_PLATFORM=freertos --toolchain <toolchain file> ...
```

`ADL_TARGET_PLATFORM` wins over detection when it is set. A name it does not know
is an error that lists the ones it does. The rest of the build never sees it, so
the isolation check has nothing to find. The selector then reads the fragment's
`ADL_PLATFORM_STATUS` and refuses a platform that is only a design, and one whose
code has never been compiled unless `-DADL_ACCEPT_UNBUILT_PLATFORM=ON`. A platform whose image a
project of its own builds (an RTOS: Zephyr's `find_package(Zephyr)`, a FreeRTOS kernel port and `main`)
says where in `ADL_PLATFORM_PROJECT`, and the selector refuses it with that pointer; that project
includes the platform's fragment itself for the source list, and the fragment is the one place the
sources of the platform are named.

The contract:

| What | Declared as | Read by |
| --- | --- | --- |
| Source files compiled into `adl_platform` (relative to `src/platforms`) | `ADL_PLATFORM_SOURCES` | `src/platforms/CMakeLists.txt` |
| System libraries `adl_platform` links | `ADL_PLATFORM_LINK_LIBRARIES` | `src/platforms/CMakeLists.txt` |
| Compile definitions, for `adl_platform` only (Windows: `WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00`) | `ADL_PLATFORM_COMPILE_DEFINITIONS` | `src/platforms/CMakeLists.txt` |
| The name in the version block (`Windows`, `Linux`, `macOS`) | `ADL_PLATFORM_DISPLAY_NAME` | The configure summary, the applications' version block (`cmake/AdlUi.cmake`) |
| Whether this is a desktop (installers, the Qt applications and notices exist) | `ADL_PLATFORM_DESKTOP` | `CMakeLists.txt`, which looks for Qt and builds the applications by default only on a desktop; checked by the contract |
| For a desktop only: the notice fragments the platform adds, one sentence on where the programs get Qt, and the strings the generated notices must contain | `ADL_PLATFORM_NOTICE_FRAGMENTS`, `ADL_PLATFORM_NOTICE_QT_DEPLOYMENT`, `ADL_PLATFORM_NOTICE_EXPECTED` | `cmake/AdlUi.cmake`, `tests/system/check_notices.cmake` |
| Optional: a vendor package the platform needs, whether it was found, its name and a hint | `ADL_PLATFORM_PACKAGE_FOUND`, `ADL_PLATFORM_PACKAGE_NAME`, `ADL_PLATFORM_PACKAGE_HINT` | `cmake/Platform.cmake`, which fails the configure by name when the package is missing (the INTEGRITY platforms) |
| Whether there is code to build and CI runs it (`supported`), CI runs it on a simulator only (`experimental`), the code has never been compiled (`unbuilt`), or there is only a design (`design-only`) | `ADL_PLATFORM_STATUS` | `cmake/Platform.cmake`, which refuses a build for the last, and for `unbuilt` unless `ADL_ACCEPT_UNBUILT_PLATFORM` is on |
| Optional: where the platform's image is built, for a platform the top-level project does not build | `ADL_PLATFORM_PROJECT` | `cmake/Platform.cmake`, which refuses to configure and says where to go |
| Where an installed Qt lives | `ADL_PLATFORM_QT_SEARCH_ROOTS`, `ADL_PLATFORM_QT_ARCH_DIRS` | `cmake/FindQt6.cmake` |
| What makes a Qt application a program of this desktop (no console window on Windows, a bundle on macOS) | `adl_platform_configure_app(<target>)` | `src/app`, `src/media_gen` and `src/mock_target` `CMakeLists.txt` |
| Qt's runtime beside a program, including the QML modules in `<qml_dir>` (`windeployqt` on Windows) | `adl_platform_deploy_qt_runtime(<target> <qml_dir>)` | the same three, and the `CMakeLists.txt` of `tests/app`, `tests/media_gen`, `tests/mock_target` and `tests/ui` |

The full contract is the header comment of `cmake/Platform.cmake`. A fragment
that forgets part of it fails the configure by name, not three files away. A
hook is defined by every platform, as a no-op where there is nothing to do.

Compiler concerns (`/W4` against `-Wall`, sanitizer flags) are a separate axis:
they stay in `cmake/CompilerWarnings.cmake`, `Sanitizers.cmake` and
`Coverage.cmake`, and are keyed on the compiler, not the operating system.

## Adding a platform

1. Create `src/platforms/<name>/` with one implementation of each interface
   (`paths_<name>.cpp` and the files named in
   [platform abstraction](platform-abstraction.md); use a `posix/` file where the
   implementation is the same) and a `platform.cmake` that fills in the
   contract, with `ADL_PLATFORM_STATUS` `unbuilt` until CI builds it and runs the platform suite
   (`experimental` on a simulator, `supported` on the target).
   Copy the closest existing platform's and change what differs.
2. Add a branch for it in `cmake/Platform.cmake`, and its name to the choices of
   `ADL_TARGET_PLATFORM` if it is a cross build.
3. If it has a test case only it can run, add `tests/platform/<name>/` for it (optional,
   and none exists today: the checks in `tests/platform/` are CMake scripts that apply to
   every platform).

That is the source tree. Three things outside it still carry platform names,
because their formats cannot read a CMake variable: the configure, build and
test presets in `CMakePresets.json` (names, the `hostSystemName` condition and
the vcpkg triplet), any `platform` expression in `vcpkg.json` (there are none
today) and the legs of the CI workflows. Add the platform to those as you would
a new build target.

The `platform.isolation` test reports a source file in the new directory that no
`platform.cmake` names, `platform.fragments` reports a fragment that does not
satisfy the contract, and `platform.selector` configures the project for the
platforms that have no code to check that the selector understands an explicit
target and refuses the ones it should.

## What to do instead of an `#ifdef`

Ask: could a source file per platform, or a value CMake passes in, express this?
The cases already in the tree:

| Need | Done as |
| --- | --- |
| `<windows.h>` without the `min`/`max` macros, and with the wide-character API | `WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE` (and `_WIN32_WINNT`) are in the Windows `platform.cmake` and passed to `adl_platform`; no source file defines them |
| The path of the running executable | `interfaces/adl/platform/paths.hpp`; `windows/paths_windows.cpp`, `linux/paths_linux.cpp`, `macos/paths_macos.cpp` |
| Sockets, files, threads, a clock, memory, queues | The interfaces in `interfaces/adl/platform/`, one implementation per platform ([platform abstraction](platform-abstraction.md)) |
| `MSG_NOSIGNAL` on one system, a different condition-variable wait on another | A function in `posix/posix_os.hpp`, defined once per system in `linux/os_linux.cpp` or `macos/os_macos.cpp` |
| An RTOS target that CMake has no name for | `-DADL_TARGET_PLATFORM=<name>`, read only by `cmake/Platform.cmake` |
| A program with no console window on Windows, a bundle on macOS | The `adl_platform_configure_app` hook in each fragment |
| Where to find Qt on each system | `ADL_PLATFORM_QT_SEARCH_ROOTS` in each fragment, read by `cmake/FindQt6.cmake` |
| Copying Qt's DLLs beside the program on Windows | The `adl_platform_deploy_qt_runtime` hook; Linux and macOS define it as a no-op |

## Rules

**Do**

- Declare what the program needs from the OS in `src/platforms/interfaces/`, in
  `namespace adl::platform`, with a comment saying that there is one definition
  per platform.
- Implement it once per platform, and list each file in the `platform.cmake` of
  every platform that compiles it.
- Put OS-tied defines and link libraries in the fragment, never in a shared
  `CMakeLists.txt`.
- Pass a value in with `target_compile_definitions`, or generate it into a header
  with `configure_file`, when all that differs is a constant.

**Don't**

- Add `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else` or `#endif` anywhere in
  first-party C++ (`src/`, `tests/`, `tools/`), platform directories included.
  They are already selected by CMake.
- Add `#define` or `#undef` there either. Use `constexpr`, an inline function or
  a template; a value the build has to hand the program arrives as a compile
  definition or a generated header. `#include` and `#pragma once` are fine.
- Include an OS header (`<windows.h>`, `<unistd.h>`, `<sys/...>`, the Apple
  frameworks, `<mach-o/dyld.h>`, `<emscripten.h>`) outside `src/platforms/` and
  `tests/platform/`.
- Ask the OS what it is at run time (`QSysInfo`, `QOperatingSystemVersion`,
  `Q_OS_*`, `platformName()`, QML's `Qt.platform`) outside the platform section.
- Test `WIN32`, `APPLE`, `UNIX`, `LINUX`, `ANDROID`, `IOS`, `EMSCRIPTEN`,
  `CMAKE_SYSTEM_NAME` or `$<PLATFORM_ID:...>` in any CMake file but
  `cmake/Platform.cmake` and the platform fragments.

## Enforcement

`tools/checks/check_platform_isolation.ps1` is registered as the
`platform.isolation` test (whenever `pwsh` is found), and CI runs it as a job of
its own as well, so a violation shows even while a platform's build is red. It
fails, with a `file:line` for each finding, on:

1. any preprocessor conditional in first-party C++ (`src/`, `tests/`, `tools/`),
   deliberately broader than "no OS macros": an `#if 0` or a feature `#ifdef`
   counts too. The platform directories are not exempt;
2. an OS header, or an OS-querying API (`QSysInfo`, `Q_OS_*`, `platformName()`,
   `Qt.platform`, ...), outside `src/platforms/` and `tests/platform/`. QML and
   JavaScript files are scanned for the APIs too. (The conformance tests and the
   fake use only the platform interfaces and the standard library, so they are
   outside the exemption and pass the check as any shared code does.);
3. a platform directory without a `platform.cmake`, or a translation unit
   (`.cpp`, `.cc`, `.cxx` or `.mm`) in one that no `platform.cmake` names
   outside a comment. (`detail/`, `interfaces/`, `posix/` and `ghs/` are not
   platforms, and are not required to have one; the translation units in `posix/`
   and `ghs/` must still be named by some fragment);
4. a test of the operating system (`WIN32`, `APPLE`, `UNIX`, `LINUX`,
   `ANDROID`, `IOS`, `MINGW`, `CYGWIN`, `EMSCRIPTEN`, `CMAKE_SYSTEM_NAME`,
   `$<PLATFORM_ID:...>`) in any CMake file but `cmake/Platform.cmake`, the
   platform fragments, any vcpkg triplet under `cmake/triplets/` (there are none
   yet) and the toolchain files under `cmake/toolchains/`;
5. any `#define` or `#undef` in first-party C++.

Run it from the repository root:

```sh
pwsh ./tools/checks/check_platform_isolation.ps1
```

It exits 0 when clean, 1 when it found violations, and 2 when it was started
somewhere that has no `src/platforms/`. A finding is also printed as a GitHub
Actions annotation, so on a pull request it lands on the line.

What it scans: `src`, `tests`, `tools` and `cmake`, and the top-level
`CMakeLists.txt`. It asks git which files count, so build output and anything
git ignores are skipped, while a new file that is not yet committed is still
checked; outside a git work tree it scans everything, and says how many files it
skipped. `deps/` is not scanned: vcpkg is not first-party code.

What it does not do: it is a pattern scan, not a parser. It cannot tell that a
portable-looking call behaves differently on two systems, and compiler tests in
CMake (`if(MSVC)`, `CXX_COMPILER_ID`) are a different axis and are not matched.
The rule is held by review as much as by the check.

The CMake side has its own check, the `platform.fragments` test
(`tests/platform/check_fragments.cmake`). A build includes only the
`platform.cmake` of the platform it targets, so the others would otherwise be
exercised only on their own machines. The test includes every fragment it
finds, each in a process of its own, and holds it to the contract: every
variable and hook defined, every file it compiles present, and a branch in the
selector for it (and the other way: a branch with no fragment behind it). It
runs wherever the tests run, so a typo in the macOS fragment fails a Windows
build in seconds rather than a macOS one later.
