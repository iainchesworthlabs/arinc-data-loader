# Green Hills INTEGRITY platforms

Two platforms for Green Hills Software's INTEGRITY RTOS: `ghs508` for the
commercial INTEGRITY 5.0.8 and `ghs178b` for INTEGRITY-178B. This page is the
review that decided their shape: what the two source trees provide, what the
platform interfaces need, how the CMake side finds a distribution, and what is
still unknown.

Green Hills is the vendor, INTEGRITY the operating system, MULTI (with `ccppc`,
`cxppc`, `gbuild`) the toolchain. VxWorks is Wind River's product and its
`src/platforms/vxworks/` design fragment is untouched by this work.

## Status

| Platform | Status | What that means |
| --- | --- | --- |
| `ghs508` (INTEGRITY 5.0.8) | **unbuilt** | The port is written on the shared `posix/` sources plus a small INTEGRITY delta. No GHS toolchain or INTEGRITY 5.0.8 headers were available when it was written, so it has never been compiled |
| `ghs178b` (INTEGRITY-178B) | **unbuilt** | A native port written against the supplied tree's application API, following the timing pattern of the vendor's own ARINC 615A module. Two interfaces say `unsupported` on purpose: `Threading::spawn` and the whole `FileSystem` (see below). Never compiled |

Neither is a desktop: no Qt, no installers, and the tests are not built for
them (`ADL_BUILD_TESTS=OFF`). The selector refuses both until
`-DADL_ACCEPT_UNBUILT_PLATFORM=ON`.

## The two source trees

Both live outside the repository, in the licensed installations of the vendor's products, and are found at configure time (see
[Finding the distributions](#finding-the-distributions)). Nothing from them is
vendored into the repository.

### `int508` — INTEGRITY 5.0.8, commercial

What the folder is: a **board support package and prebuilt runtime** for a
PowerPC target, not a complete SDK.

| What | Where | Notes |
| --- | --- | --- |
| BSP sources | `bsp/` (131 files) | `mv64x6x` timers/enet/DMA/I2C, `ppc5554`, `rsimp2a`, `riu5554`, serial, flash, PCI, RTC. Kernel-side code |
| Target build files | `target/rsimp2a.bld`, `target/riu5554.bld` | `:ppc_cputype=ppc7455`, `:defines=__rsimp2a__`, `target_os=INTEGRITY`, includes `intppc.bld` |
| Prebuilt runtime | `rsimp2a/` | `libINTEGRITY.so`, `libc.so`, `libposix.a`, `libsocket.a`, `libnet.a`, `libtcpip.a`, `libip4.a`, `libbsp.a`, `libivfs.a`, FFS/MSDOS/NFS, TFTP client/server, `libecxx*.so`, `libscxx*.so` |
| Linker scripts | `rsimp2a/default.ld`, `default_256MBFlash.ld` | GHS linker syntax (`CONSTANTS`/`SECTIONS`) |
| Build driver | `rsimp2a/system_libs.gpj` | `-bsp rsimp2a`, `-os_dir c:/ghs/int508` |
| OS headers | **not here** | They come from the MULTI/INTEGRITY installation the `.gpj` names (`C:/ghs/int508`) |

So the folder gives the board layer and the libraries; the headers (and the
compiler) are the installation. `FindIntegrity` therefore needs both
`INTEGRITY_ROOT` (the installation) and `INTEGRITY_BSP_ROOT` (this folder), and
says which one is missing.

### `int178B` — INTEGRITY-178B

What the folder is: a **kernel/BSP source distribution with a partial
application include set** (`rtos-i40-178B-bae-04252008`), plus prebuilt
libraries for two boards.

| What | Where | Notes |
| --- | --- | --- |
| Application headers | `INTEGRITY-include/` | `INTEGRITY.h`, `fcntl.h`, `sys/time.h`, `sys/types.h`, `ipflite/`, `arinc615a/`, `auditlog.h`, `boottable.h` |
| C library subset | `ansi/` | ctype, errno, float, limits, math, stdbool, stddef, stdint, stdlib, string. **No `stdio.h`, no `unistd.h`, no `sys/stat.h`** |
| C++ headers | `eecxx/` | Green Hills *Embedded* C++ (`new`, `cstddef`, ..., `yvals.h`) |
| BSP and kernel | `bsp/`, `kernel/`, `asp/`, `intlib/`, `shared/` | Sources and kernel-side headers (`kernel/task.h`, `kernel/clock.h`, `kernel/semaphore.h` are kernel internals) |
| IP stack | `tcpip/ipflite/`, `INTEGRITY-include/ipflite/` | BSD-style socket calls over `ipflite` |
| Prebuilt libraries | `rsimp2a-bae/`, `sim800/`, `ippc*/` | `libINTEGRITY.a`, `libkernel.a`, `libbsp.a`, `libipflite.a`, `libipfliteclient.a`, `libansi_178b.a`, per-CPU `ippc` archives |
| Linker | `INTEGRITY.ld` | GHS `-sec` map with `.picbase`, `.textchecksum`, `.datachecksum` |
| Build file | `target/rsimp2a-bae.bld` | `:toolchain=ghs`, `:processor=ppc`, `:ppc_cputype=ppc7400`, `:defines=INT_178B CLIBS_178B NO_TRACEPOINTS`, `:nostdinc=true`, `:staticlink=true`, `-Xnosmallprologue` |

The **application-facing** API in `INTEGRITY.h` is the set marked `/*c*/`:
`CreateSemaphore`, `CreateHighestLockerSemaphore`, `WaitForSemaphore`,
`ReleaseSemaphore`, `SynchronousSend`/`SynchronousReceive`, the `CreateConnection`
family, `GetTimeOfCurrentPartitionWindow`, `CurrentTask`, `Yield`, `Exit`, and
the `Time` arithmetic helpers. Tasks, clocks and memory regions are created by
the kernel image's configuration (`kernel/global_table.c`, the `.gpj`/`.bld`
files), not by the application.

`ipflite` provides `socket`, `bind`, `connect`, `sendto`, `recvfrom`, `select`,
`setsockopt` and `socket_close` (the close is not `close`). It has no `poll`.
The header defines only `SO_SNDBUF`, `SO_RCVBUF`, `SO_SNDLOWAT`, `SO_RCVLOWAT`,
`SO_ERROR` and `IP_TTL`; `SO_BROADCAST` and `SO_REUSEADDR` are not there.

### What the 178B tree does not provide

Read as a list of *headers*, the tree declares none of the following for an
application. Read as the vendor's own ARINC 615A module (application code for
this OS, in `modules/ghs/arinc615a/`), most of them exist after all, in a
shape that is not POSIX. That module is the evidence the port builds on:

- **No sleep** — but `SetClockAlarm` on a virtual clock plus `WaitForActivity`
  is the module's sleep and time-out, and the port's.
- **No timed wait** — `WaitForSemaphore` has no timeout, but registering the
  semaphore and the clock as activities and dispatching on the id the
  `WaitForActivity` returns is the module's timed wait, and the port's.
- **No readable monotonic clock** — `GetTimeOfCurrentPartitionWindow` returns
  the *window's* start and end, not the current instant; the module's clocks
  come from the primary clock (`FindClock` lives in a kernel-internal header)
  and `CreateVirtualClock`, and the port reads the primary with `GetClockTime`.
- **No application task creation** — there is no `TaskCreate`/`TaskSpawn`; tasks
  are configured into the kernel image, so `Threading::spawn` returns
  `unsupported`.
- **No file-system headers** — `fcntl.h` declares `open`, `creat` and `fcntl`;
  `read`, `write`, `lseek`, `close`, `mkdir`, `opendir`/`readdir`, `stat`,
  `rename` and `unlink` are not declared anywhere in the tree, so the port's
  `FileSystem` returns `unsupported` until the product names its storage.

The two deliberate `unsupported`s (spawning a thread, and files) are the parts
a product has to supply: the tree declares no API for them, so the port says `unsupported` and does not pretend.

## The shape

```text
src/platforms/
  ghs/            the INTEGRITY-178B native helpers (error mapping, Time
                  conversion, priority map); not a platform, listed like posix/
                  and exempt from the fragment rule
  ghs/std/        the C++20/23 standard-header shims a GHS toolchain can be
                  short of (either INTEGRITY toolchain can use them), one directory per header so that a toolchain which
                  lacks ONE header sees ONE shim (expected/, span/, bit/,
                  compare/, filesystem/)
  ghs508/         INTEGRITY 5.0.8: platform.cmake, os_ghs508.cpp,
                  paths_ghs508.cpp, and the shared posix/ sources
  ghs178b/        INTEGRITY-178B: platform.cmake and a native port
                  (memory, clock, threads, files, network, paths, host)
cmake/
  FindIntegrity.cmake        the 5.x installation, BSP package and GHS toolchain
  FindIntegrity178.cmake     the 178B kernel/BSP tree
  GhsStdShims.cmake          detects which standard headers the toolchain lacks
  toolchains/ghs-integrity508.cmake
  toolchains/ghs-integrity178b.cmake
```

Two platform names rather than one `ghs` with a profile: the fragments differ in
status, sources and libraries, the contract is built around one directory per
platform, and `platform.fragments` checks each one on every host. `ADL_TARGET_PLATFORM`
is `ghs508` or `ghs178b`; CMake has no system name for either, so it is always
named explicitly.

## The 5.0.8 port

INTEGRITY 5.0.8 is POSIX, so the port is the shared POSIX sources plus the
per-system functions `posix/posix_os.hpp` already abstracts:

| Interface | Implementation |
| --- | --- |
| `FileSystem` | `posix/files_posix.cpp` (`open`, `pread`, `write`, `fstatat`, `mkdir`, `opendir`/`readdir`, `rename`, `unlink`) |
| `MemoryProvider` | `posix/memory_posix.cpp` (`posix_memalign`, `mlock`) |
| `Threading` | `posix/threads_posix.cpp` (`pthread_create`, `pthread_mutex_t`, a mutex + condition variable as the semaphore) |
| `Clock` | `posix/clock_posix.cpp` (`clock_gettime(CLOCK_MONOTONIC)`, `nanosleep`) |
| `Network` | `posix/network_posix.cpp` (`socket`, `bind`, `sendto`, `recvmsg`, `poll`, non-blocking sockets) |
| `Ipc` | `detail::RingIpc` over the threading above |
| `paths` | `ghs508/paths_ghs508.cpp`: INTEGRITY has no `/proc/self/exe`, so it returns empty (the interface allows that) |
| per-system | `ghs508/os_ghs508.cpp`: `host_name`, `is_real_time`, `init_condition`, `wait_condition`, `apply_priority`, `name_current_thread`, `flush_to_storage` |

Libraries, from the BSP package: `INTEGRITY`, `posix`, `socket`, `net`, `c`.
The BSP itself (`libbsp.a`, the `bsp/` sources) belongs to the kernel image, not
to the loader task; the port links the runtime, not the BSP.

**To verify against the installed headers** (each is a place INTEGRITY's POSIX
may differ from Linux's): `pread`, `fstatat`, `dirfd`, `poll`, `mlock`,
`pthread_condattr_setclock`, `fsync`, `SO_REUSEPORT`. A missing one becomes a
function in `posix_os.hpp` with an implementation for each POSIX system, the way
the condition-variable clock already is; it is not an `#ifdef`.

## The 178B port

The blocker that made this a design at first — no clock, sleep or timed wait in
the client headers — was settled by reading the vendor's own ARINC 615A module
(`modules/ghs/arinc615a/arinc615a_targethardware.c`), which is application code
for this OS. Its timing model is the one the port copies:

1. `CreateVirtualClock(PrimaryClock, CLOCK_ALARM, &clock)` makes a per-task
   alarm clock over the `PrimaryClock` (the extern `INTEGRITY.h` declares;
   `FindClock` lives in a kernel-internal header, so the port uses the externs
   the vendor's module uses).
2. `CreateActivity(task, prio, false, id, &activity)` and
   `AsynchronousReceive(activity, (Object)object, NULL)` register an object — a
   semaphore, a clock — as a source of activity for the task.
3. `SetClockAlarm(clock, false, NULL, &deadline)` arms a one-shot alarm at an
   *absolute* time, and `WaitForActivity(&id)` blocks until any registered
   source fires; the id says which. A timed wait is "register the semaphore and
   the clock, arm the alarm, wait, and decide from the id whether the semaphore
   was posted or the time ran out". A virtual clock made with `CLOCK_ALARM`
   alone cannot be read (reading demands `CLOCK_READTIME`), so the port reads
   the time from the primary clock and arms alarms on the virtual one; the two
   share the same timer driver.

The port:

| Interface | Implementation |
| --- | --- |
| `MemoryProvider` | A static 512 KiB block (`.bss`), first-fit with coalescing; no OS call, nothing allocated after start-up. `resident` is the same memory: an application partition does not page |
| `Clock` | `GetClockTime` on the primary clock for `now()` (a virtual alarm clock cannot be read); `SetClockAlarm` + `WaitForActivity` for `sleep_for`; the RTC clock (the `RealTimeClock` extern) for wall time, `unsupported` when absent |
| `Threading` | `Mutex` from `CreateHighestLockerSemaphore`; `Semaphore` from `CreateSemaphore`/`ReleaseSemaphore` with the ceiling counted in the object, the poll from `TryToObtainSemaphore`, and the timed wait from the pattern above. `spawn` returns `unsupported`: application tasks are configured into the kernel image, and the tree has no client task-creation call |
| `FileSystem` | Every method returns `unsupported`. The tree declares no application file API (`fcntl.h` has `open`/`creat`/`fcntl` and nothing else); a product supplies the storage adapter the design doc describes for targets without one |
| `Network` | `ipflite`: non-blocking `recvfrom` + `select` for the waits, `socket_close`, `getsockname`; `wait_readable` is one `select` over the sockets. Broadcast and port reuse cannot be asked of the stack (the header declares no `SO_BROADCAST`/`SO_REUSEADDR`), which is commented at each place it matters |
| `Ipc` | `detail::RingIpc` over the native semaphores |
| `paths` | Empty, as on 5.0.8 |

The port is a working skeleton of everything the interfaces need except those
two deliberate `unsupported`s; each uncertain spelling — the meaning of a
`WaitForActivity` id, whether a completed semaphore activity means the
semaphore was taken, the `CreateHighestLockerSemaphore` argument — is marked in
the source as to verify against the module and the User's Guide at the first
compile.

## C++ standard and shims

The decision, stated once so it is never re-argued file by file:

- **The interfaces are C++23 and stay C++23.** Nothing in
  `src/platforms/interfaces/adl/platform/` is weakened for a toolchain, and no
  platform code is written to an older standard.
- **The GHS toolchain is assumed to support the C++23 language.** A 2008-era
  MULTI would not; the toolchain this work targets does. The standard
  *library* is where the gap can appear, and that is the only gap the shims
  cover.
- **A missing standard header gets a shim on the GHS side**, never a change to
  an interface. The five headers that the interfaces and the platform sources include and that are C++20 or newer — `<expected>`, `<span>`, `<bit>`, `<compare>`, `<filesystem>` — have
  shims in `src/platforms/ghs/std/` (GHS's extensionless-header convention),
  each providing exactly what this code base uses and no more, so a use that
  appears fails to compile against the shim, which is the point.
- **Selection is automatic, and per header.** `cmake/GhsStdShims.cmake`
  compiles a probe per header with the located toolchain; for each header it
  lacks, the matching shim's own directory
  (`src/platforms/ghs/std/<header>/`) is prepended to the platform's include
  path, ahead of the toolchain's own headers, which is what makes the shim
  shadow the missing one. One directory per shim, so a toolchain that is short
  `std::expected` and nothing else sees exactly that one shim and never
  shadows a header it does have. A gap with no shim is a configure error that
  names the file to add. Where there is no compiler (the `platform.fragments`
  check) nothing is probed, because a fragment declares and only a real build
  compiles.
- **Anything older than C++20 is assumed present** (`<string_view>`,
  `<optional>`, `<variant>`, `<chrono>`, ...): a toolchain that lacks those
  while claiming C++23 needs a new set of shims, not a changed interface.

Two consequences follow. `paths.hpp` returns `std::filesystem::path` and is
left alone: a toolchain without `<filesystem>` gets the minimal shim (a `path`
that constructs from anything, compares, and is always empty — which is the
truth on INTEGRITY, where `executable_path()` returns `{}`). And the platform
code keeps its C++23 spelling throughout; the shims define names in
`namespace std`, which is undefined behaviour by the letter of the standard and
the accepted way to backfill one header for one toolchain, and they are only
ever compiled in GHS builds.

## Finding the distributions

`cmake/FindIntegrity.cmake` and `cmake/FindIntegrity178.cmake` are module-mode
packages (`find_package(Integrity)` / `find_package(Integrity178)`). Each
accepts a root on the command line or in the environment and searches the usual
places; neither vendors anything.

| Input | Meaning |
| --- | --- |
| `INTEGRITY_ROOT` | The INTEGRITY 5.x installation: the directory holding `include/INTEGRITY.h` and the GHS runtime libraries |
| `INTEGRITY_BSP_ROOT` | The board package (the `int508` directory of the installation): `bsp/`, `target/`, the prebuilt `rsimp2a/` |
| `INTEGRITY_BSP` | Board name, default `rsimp2a` |
| `INTEGRITY_TARGET_CPU` | Default `ppc7455` |
| `INTEGRITY_VERSION` | Default `5.0.8` (also derived from an `intNNN` root) |
| `INTEGRITY_TOOLCHAIN_ROOT` | The MULTI installation holding `ccppc`/`cxppc` |
| `INTEGRITY178_ROOT` | The 178B tree (the `int178B` directory of the installation) |
| `INTEGRITY178_BSP` | Board, default `rsimp2a-bae` |
| `INTEGRITY178_TARGET_CPU` | Default `ppc7400` |

Results, for both: `_FOUND`, `_VERSION`, `_INCLUDE_DIRS`, `_LIBRARIES`,
`_LIBRARY_DIRS`, `_LINKER_SCRIPT`, `_BSP_INCLUDE_DIRS`, `_BSP_SOURCES`,
`_COMPILE_DEFINITIONS`, `_COMPILE_OPTIONS`, `_C_COMPILER`, `_CXX_COMPILER`,
`_ASSEMBLER`, `_LINKER`, `_BUILD_TOOL`, and an imported `_::Runtime` target that
carries the include directories, definitions, options and libraries. A platform
fragment adds that one target to `ADL_PLATFORM_LINK_LIBRARIES`; the contract
does not grow a variable per toolchain.

The find modules fail with an explanation only when the platform is actually
being built: `platform.fragments` includes every fragment in a bare `cmake -P`
process, where a missing installation must not fail the check.

## Building

```sh
cmake --preset ghs508-ghs-debug
cmake --build --preset ghs508-ghs-debug
```

The toolchain file sets `CMAKE_SYSTEM_NAME` to `Generic` (CMake has no INTEGRITY
platform module; it does have the GHS compiler modules), the compilers to
`ccppc`/`cxppc`, `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` (a GHS
executable link needs the INTEGRITY linker directives and is done by `gbuild`),
and the CPU flag. The CMake build produces the static libraries and objects; the
final INTEGRITY image is linked by the product's `.gpj`/`.bld`, as the vendor
intends, and `FindIntegrity` exposes the linker script and BSP sources for it.

## Open questions

These need answers before either platform can be compiled:

1. **The 178B file system and task table.** The port's `FileSystem` is
   `unsupported` and `spawn` is `unsupported` because the tree has no
   application file API and no client task-creation call. The product decides
   which storage adapter stands in for `FileSystem`, and whether the loader's
   runner thread is a configured task.
2. **Which board.** 5.0.8 has `rsimp2a` and `riu5554`; 178B has `rsimp2a-bae`
   and `sim800`. The defaults above are the ones the build files in the trees
   name; a product may use another.

The C++-level question that used to head this list is settled by
[C++ standard and shims](#c-standard-and-shims): the interfaces stay C++23,
and a GHS standard library that is short a header gets a shim.

## What is done, and what is next

Done: the two find modules, both toolchain files, the selector
branches, the `ghs508` port on the POSIX sources, the `ghs178b` native port,
the standard-header shims and their detection, the presets, and the tests that
hold them to the contract.

Next: compile `ghs508` against a real installation and fix what INTEGRITY's
POSIX differs in; compile `ghs178b` and verify the clock/activity spellings
against the vendor's module; and settle the two questions above with the
product.
