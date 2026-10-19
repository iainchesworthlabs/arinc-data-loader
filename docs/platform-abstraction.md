# Platform abstraction

What the loader needs from the system it runs on, as interfaces in
`src/platforms/interfaces/adl/platform/`, with one implementation per
operating system beside them. This page is the design: the four areas
(files, memory, threads and time, networking and message passing), why each is
shaped as it is, how each of the operating systems provides it, and what is
and is not known to work. The two Green Hills INTEGRITY ports have their own
review, [the GHS review](platform-ghs.md), because they were built against the
vendor's source trees rather than its documentation.

[Platform isolation](platform-isolation.md) is the rule that keeps the operating
system out of the rest of the source, and how to add a platform. This page is
what a platform has to provide once you do.

## What is verified, and what is a design

Be clear about this before reading further.

| Platform | Status | What that means |
| --- | --- | --- |
| Windows | **supported** | Built and tested by CI (MSVC, Debug under AddressSanitizer and Release), and by the author |
| Linux | **supported** | Built and tested by CI (GCC), and in a container by the author (GCC 15, Debug under AddressSanitizer and UBSan, and Release) |
| macOS | **supported, not yet seen to run** | Written to the same pattern and built by CI (AppleClang); nobody had run it when this was written. The first CI run is its first test. The places most likely to need a fix are listed under [Risks](#gaps-and-risks) |
| QNX Neutrino | **unbuilt** | Code exists (`src/platforms/qnx`, on top of the shared `posix/` sources) and has never been compiled against the QNX headers or run. The selector refuses it unless asked (`-DADL_ACCEPT_UNBUILT_PLATFORM=ON`) |
| INTEGRITY 5.0.8 (GHS) | **unbuilt** | Code exists (`src/platforms/ghs508`, on top of `posix/`) and has never been compiled against the INTEGRITY headers or run; no GHS toolchain exists on this project's machines. [The GHS review](platform-ghs.md) lists the places INTEGRITY's POSIX may differ |
| INTEGRITY-178B (GHS) | **unbuilt** | Code exists (`src/platforms/ghs178b`, a native port: semaphores, the virtual-clock time-out pattern, ipflite) and has never been compiled. `Threading::spawn` and the `FileSystem` are `unsupported` on purpose; [the GHS review](platform-ghs.md) says why |
| VxWorks | **design only** | No code. The fragment exists so the name is known and a build is refused with an explanation |
| FreeRTOS | **experimental** | Built, and the conformance suite and the 615A engine tests run, in CI on the kernel's POSIX simulator port: the real kernel in a Linux process, lwIP's UDP sockets over its loopback interface, littlefs on a RAM block device (`src/platforms/freertos`, `tests/platform/freertos`). No target hardware, real network interface or flash has run it. [What was and was not verified](#the-freertos-and-zephyr-ports) |
| Zephyr | **experimental** | Built, and the conformance suite and the 615A engine tests run, in CI on Zephyr's `native_sim` board: Zephyr's own kernel, IPv4/UDP stack (over its loopback interface) and littlefs on simulated flash, as a host process (`src/platforms/zephyr`, `tests/platform/zephyr`). No board has run it. [What was and was not verified](#the-freertos-and-zephyr-ports) |

*Experimental* is a status of its own: more than QNX's unbuilt code (it compiles, and a test suite passes
on it, in CI) and less than the desktops' supported (what ran is a simulator, which has no priorities
against real interrupts, no real link, no real flash). The fragment says `experimental`, and the selector
builds nothing for it from the top-level project: the image of an RTOS is built by a project of its own
([Selecting a platform](#selecting-a-platform)).

Only the three desktops are built and tested on their own hardware. FreeRTOS and Zephyr run on
simulators. For QNX, VxWorks and the INTEGRITY ports this page says what the operating system offers as
the author understands it from its documentation, without having built against it (the two INTEGRITY
ports are reviewed against the supplied source trees in [the GHS review](platform-ghs.md)). Every such
statement is a claim to check against the vendor's headers when a port is compiled, and the tables mark
where the author is unsure. The FreeRTOS and Zephyr columns below were claims until those ports were
written; [the section on them](#the-freertos-and-zephyr-ports) says where the code differs from what
the tables proposed.

The choice to ship QNX as unbuilt code and the other three as designs was the
author's, not the owner's: QNX is POSIX, so a port is a few hundred lines on
top of what the desktops already share, while a VxWorks, FreeRTOS or Zephyr
port is written against a particular configuration (VxWorks 178C's permitted
API subset, FreeRTOS's choice of network stack and file system, Zephyr's
Kconfig) that nobody here has. The Green Hills INTEGRITY ports (5.0.8 on the
POSIX sources, 178B native) followed when their source trees became available;
[the GHS review](platform-ghs.md) is where they are described. FreeRTOS and
Zephyr were then written, at the owner's request, against the one configuration
each that a simulator provides, and are experimental. VxWorks is still a design.

## What 615A asks of a platform

Read from `docs/specs/615A-3.pdf` (section numbers are its own); nothing here
is quoted.

- **UDP over IPv4 and nothing else** (5.3.1). TFTP (RFC 1350) rides on it. All
  TFTP uses **port 59** (5.3.2.3.2); FIND uses **port 1001**, a broadcast
  request and unicast answers (5.3.3, Attachment 3). A "port" option can move
  the *loader's* control port to 49152 or above for one operation (5.3.2.3.8.6);
  the target's stays 59. An AFDX network has no broadcast (Appendix E), so FIND
  there is a series of unicast sends.
- **The loader is both TFTP client and server** (5.4, 6.3). It reads the
  target's `.LCI`, `.LUI` or `.LND` file as a client, then *serves* the target's
  reads of header and data files, and receives the target's *writes* of status
  files (`.LCS`, `.LUS`, `.LNS`) and downloaded data, all on its own control
  port. So one process listens on a port and runs several transfers at once
  (6.3.2 allows the upload thread to be duplicated for a parallel load). A
  socket is bound to a chosen port, each transfer gets a socket of its own on an
  ephemeral port, and one thread must wait on all of them.
- **Time-outs are the protocol's reliability** (Attachment 4): a TFTP time-out of
  2 s with one retry, a data-load protocol time-out of 13 s between a transfer's
  end and the next one's start, a WAIT of up to 65535 s (5.3.2.3.4), a target
  "exception timer" of up to 65535 s of silence (6.4.3). The platform has to
  give a monotonic clock that does not step, and to sleep.
- **Blocks up to 65464 bytes** (RFC 2348, adopted by 5.3.2.3.8.1), so a datagram
  of up to 65468 bytes, with IP fragmentation left to the system.
- **Files are read at the offsets TFTP asks for and written as blocks arrive.**
  A download makes a directory `DNLD_DATA_<THW_ID_POS>_<n>` and may write a
  `DNLD_INFO_...` file (5.4.4.3), so the platform must make directories and
  refuse to overwrite.
- **The standard says nothing about threads, memory or priorities.** Those
  decisions are this project's, driven by the targets it wants to reach.

## The shape

```text
  application / runner              engine (sans-I/O core)            platform
  ────────────────────              ──────────────────────            ────────
  owns a Host&               ──►   protocol state machines:
  waits on sockets, a timer         (now, bytes received) in,
  and a queue                       (bytes to send, events, the next
                                     time it wants to be woken) out
        │                                    ▲
        │ clock, sockets, files              │ no thread, no socket,
        ▼                                    │ no clock inside
   adl::platform::Host ───────────────────────┘
   clock · memory · files · threads · network · ipc
```

- **A sans-I/O core.** The 615A and TFTP state machines (stages 2 and 3) take the
  time and the bytes that arrived and return the bytes to send, the events for the
  application and the next instant they want to be woken. They own no thread,
  socket or clock. That is what makes them portable (the core compiles
  anywhere), testable (feed it a script of packets and times, with no network
  and no waiting) and certifiable (no hidden concurrency).
- **A thin runner** is the only code that calls the platform: wait on the sockets
  until the core's next deadline, hand it what arrived, send what it returned,
  read and write the file blocks it asked for. One thread runs it.
- **A `Host`** (`host.hpp`) gathers the six services behind one reference, so
  that the runner is handed a single thing and a test hands it another.
  `make_host()` is defined once per platform and returns the real one; the fake
  (`tests/fake`) is the other.

### Why abstract classes, not link-time functions

`paths.hpp` (the executable's path) is a free function with one definition per
platform, chosen at link time. That is right for something nothing replaces.
The services here are replaced: the fake must stand in for the real ones in the
same program. So they are abstract classes and the one choice made at link time
is `make_host()`. The cost is a virtual call per operation and a heap object
per handle, which is nothing next to a system call and a network round trip.
Templates over the platform would avoid both and put all the engine's code in
headers, compiled once per platform and per test; not worth it.

### Failure is a value; nothing throws

Every operation returns `Result<T>` (`std::expected<T, adl::platform::Error>`),
and every interface function is `noexcept`. An `Error` is a code from a small
fixed set (`timeout`, `not_found`, `no_space`, `address_in_use` ...) and the
system's own number, kept for the log and never branched on. Each platform maps
errno, `GetLastError`, `WSAGetLastError` or its RTOS's status onto the codes
(`posix/posix_common.cpp`, `windows/windows_common.cpp`).

Allocation in the implementations is `new (std::nothrow)`, and a failure is
`resource_exhausted`; a plain `new` would call `std::terminate` from inside a
`noexcept` function. There is no RTTI: the one place an implementation needs the
concrete type of an interface pointer (`wait_readable` takes sockets) uses
`static_cast`, which is why the sockets must be ones that this `Network` opened.
There is no iostream.

### Handles: `Owned<T>`

A socket, an open file, a thread, a lock or a queue is returned as an
`Owned<T>` (`owned.hpp`): a `std::unique_ptr` whose deleter can hand the object
back to the service that made it. On a desktop the deleter is empty and the
handle simply deletes the object. A platform with no heap implements
`Recycler<T>` over fixed slots and returns handles that point at it, so the
interface does not force a heap on the platforms that cannot have one. This was
decided now, with the interfaces, because it is the one choice that would change
every signature later.

## Area 1: files and storage (`files.hpp`)

**Interface.** `FileSystem` opens a `ReadFile` (size, `read_at(offset, buffer)`),
creates a `WriteFile` (append, size, sync, close), and `stat`s, makes a
directory, removes, renames and lists. Listing takes a function pointer and a
context, with a template wrapper, so that nothing is allocated for it.

**Rationale.**

- It is the loader's use and no more. A TFTP server reads a block at the offset
  the request names, and a client appends blocks as they arrive. There is no
  seek, no read-write handle, no memory mapping.
- `read_at` returns a short count only at the end of the file, so a caller fills
  a block with one call; `write` writes all of its data or fails, so a caller
  never resumes a partial write. Both match what RTOS file systems can promise
  and what a TFTP block needs.
- `create` is `exclusive` or `replace`. Exclusive is the default for downloads:
  the loader never overwrites (5.4.4.3 numbers each download directory, and the
  tree writer in `adl::media_tree` is as careful).
- `rename` with replacement makes a download atomic: write to a temporary name,
  check the checksum option's value (5.3.2.3.8.5), rename.
- **Paths** are UTF-8, `/` separated, never empty and never containing NUL or a
  backslash; every platform refuses those with `invalid_argument`
  (`detail/path_rules.hpp`). A backslash is a separator on Windows and a letter on
  Linux, and file names reach the loader from a target on the network, so the
  interface makes a name mean one thing everywhere. The engine goes further and
  never builds a path from a name a target asked for: it serves only names it finds
  in the load's header file and media set lists, so there is nothing to traverse.
- It does **not** use `std::filesystem`: no path objects, iterators, error codes or
  allocation, and nothing that an RTOS's library lacks. (`adl::media_tree`, which
  writes a media set to a directory, does use it, and is not part of the
  embedded story; the platform layer is the part that must be portable to it.)
- A target with **no file system** holds its loads some other way, such as flash
  blobs in a table. It serves the engine through a small adapter that implements
  `FileSystem` over that table; `tests/fake/fake_storage.hpp` (an in-memory file
  system) is the shape of one.

| | Windows | Linux, macOS | QNX | VxWorks | FreeRTOS | Zephyr |
| --- | --- | --- | --- | --- | --- | --- |
| Open, read | `CreateFileW`, `ReadFile` with an `OVERLAPPED` offset | `open`, `pread` | as POSIX | `open`, `lseek` + `read` (no `pread` assumed) | FreeRTOS+FAT `ff_fopen`/`ff_fseek`/`ff_fread`, or FatFs `f_open`/`f_lseek`/`f_read`, or littlefs `lfs_file_*` | `fs_open`, `fs_seek`, `fs_read` |
| Write | `WriteFile` | `write` loop | as POSIX | `write` | `ff_fwrite` / `f_write` / `lfs_file_write` | `fs_write` |
| Durable | `FlushFileBuffers` | `fsync` (macOS: `F_FULLFSYNC`, then `fsync`) | `fsync` | `ioctl(FIOSYNC)` or `fsync` on dosFs/HRFS | `ff_fflush`/`f_sync`/`lfs_file_sync` | `fs_sync` |
| Directories | `CreateDirectoryW`, `FindFirstFileExW`, `RemoveDirectoryW` | `mkdir`, `opendir`/`readdir`, `rmdir` | as POSIX | `mkdir`, `opendir`/`readdir` | `ff_mkdir`, `ff_findfirst`; `f_mkdir`, `f_readdir`; `lfs_mkdir`, `lfs_dir_read` | `fs_mkdir`, `fs_opendir`/`fs_readdir` |
| Rename with replace | `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` | `rename` | `rename` | `rename` | `ff_rename`; FatFs `f_rename` **fails if the target exists** (remove first, not atomic); littlefs rename replaces | `fs_rename` (target behaviour depends on the file system) |
| Mount points | drive letters, UNC | one tree | one tree | device names | volume prefixes | every path begins at a mount point (`/NAND:/...`) |

**Gaps and risks.**

- Windows: a path longer than about 1000 UTF-16 units is refused with
  `name_too_long`; the `\\?\` prefix is not used. Removing a file that is open
  keeps its name occupied until the last handle closes (the conformance test
  checks that the reader keeps working, not that the name is free).
- FAT cannot replace atomically, and a power loss mid-download leaves a partial
  file: the engine writes to a temporary name and renames, and reports a
  download as complete only after that.
- Case and Unicode: the interface assumes nothing about two names that differ
  only in case. macOS and Windows file systems are usually case-insensitive and
  Linux's is not. 665 file names are case-sensitive in a load (2.2.2); a media set
  that holds two names differing only in case does not survive being written to a
  case-insensitive disk, which `adl::media_tree` already refuses.
- RTOS file systems differ in what `rename` and directory iteration while
  modifying do. The conformance suite is where a port finds out.

## Area 2: memory (`memory.hpp`)

**Goal.** The engine and codec work from caller-supplied or bounded buffers, and
nothing is allocated once the program has started. What the platform layer offers:

- `StaticStorage<N>`: `N` bytes in the object, so memory can live in `.bss`.
- `Arena`: bump allocation inside a block, with marks and `reset`. For the state of
  one operation, or the scratch of one packet. Reports exhaustion as null, never
  by throwing; `high_water()` says how much a run needed, so a program can be given
  exactly that.
- `BlockPool`: equal blocks handed out and taken back in any order. For the packet
  buffers of the transfers in flight.
- `MemoryProvider` (the one part that differs per platform): where the blocks come
  from at start-up, with `general` or `resident` (never paged out) memory.

### Decision: an arena and a pool, not `std::pmr`

`std::pmr` is the standard answer and was considered first. It is not used because:

1. **It reports exhaustion by throwing.** `polymorphic_allocator::allocate` calls
   `memory_resource::allocate`, and a resource that is full throws `bad_alloc`
   (`null_memory_resource` exists for exactly that). This project returns errors
   as values and the embedded target probably builds without exceptions.
2. **It needs a hosted standard library.** `<memory_resource>` with its
   `unsynchronized_pool_resource` and `monotonic_buffer_resource` is not in every
   embedded toolchain's library, or arrives later than the language level the rest
   of the code uses.
3. **It buys nothing where the allocation actually is.** The ARINC 665 model types
   hold `std::string` and `std::vector`, which use `std::allocator`, not a
   polymorphic one; making them allocator-aware is a rewrite of the library for
   a gain only on the platforms that need it.
4. **What the engine needs is smaller.** Fixed-size packet buffers (a pool), and
   per-operation state that is freed all at once (an arena). Both are fifty lines,
   have no virtual calls, and are tested.

An arena or pool can still be given to a `pmr` container through a thin adapter if
a port wants one; nothing here stops it.

**Where allocation still happens.** Stated plainly, because "no allocation after
start-up" is a design goal and not yet a fact:

- **`adl::arinc665` allocates.** `PartNumber`, `LoadSpec`, `HeaderFile`,
  `BatchFile`, `LoadsList`, `FilesList`, `BatchesList`, `MediaLabel`, the file lists
  and the check values hold `std::string` and `std::vector`, and the `encode_*`
  functions return a `std::vector<std::byte>`. Reading a media set into memory
  allocates a vector per file. Decoding takes a `std::span` of the bytes, so the
  *input* side needs no copy, but the decoded model is heap-built.
- **The platform layer allocates a handle per object**: a socket, an open file, a
  thread, a lock, a queue. A desktop `new`s it (without throwing). The data path
  allocates nothing: a read, a write, a send, a receive, a queue operation touch only
  the caller's buffers. An RTOS port avoids the handle allocation with `Owned<T>`'s
  recycler (above) and by creating what it can at start-up.
- **The fake allocates freely**: it is for tests.

The route to none, for the embedded target, is a TFTP engine that works only on
the protocol-file formats of 615A (which are small and fixed in structure) and on
the *bytes* of the load's files, reading a block at a time through `ReadFile`,
and never builds a `adl::arinc665` model on the device. The loader serves a header
file as bytes; it does not need to decode it to do so. That decision belongs to
stage 2, which owns the codec, and is recorded here as the intention.

| | Windows | Linux, macOS | QNX | VxWorks | FreeRTOS | Zephyr |
| --- | --- | --- | --- | --- | --- | --- |
| `general` | `VirtualAlloc` (zeroed pages) | `posix_memalign` + `memset` | as POSIX | `memPartAlloc` from a partition made at start-up on a static array; in an ARINC 653 partition, the partition's memory | `pvPortMalloc` (heap_4/5) or a static array | `k_heap_aligned_alloc` on a `K_HEAP_DEFINE` heap, or a static array |
| `resident` | `VirtualLock` (needs working-set quota) | `mlock` (limited by `RLIMIT_MEMLOCK`) | `mlock` | no paging in kernel mode: same as `general` (an RTP may differ; check) | no paging: same as `general` | no paging: same as `general` |
| Fixed blocks | `BlockPool` over the above | same | same | same, or `memPartLib`/`msgQ` | same, or `heap` per block size | `k_mem_slab` (native fixed-block allocator) |

**Gaps and risks.**

- The desktop providers are not exercised on an impossible request under the
  sanitizers (the sanitizer's allocator aborts the process instead of returning
  null, as intended); the mapping to `resource_exhausted` is by inspection there
  and tested on the fake.
- VxWorks 178C and ARINC 653 partitions forbid dynamic allocation after
  initialisation; the data path already allocates nothing, but the *handle*
  allocation above means a 178C port needs the recycler-backed handles from the
  start.
- `resident` on a desktop can fail for quota reasons even for small sizes
  (Windows' default minimum working set is about 200 KB), so callers must treat it
  as best effort and carry on, which the interface says.

## Area 3: threads, priority and time

(`threads.hpp`, `time.hpp`)

**Interface.** `Threading` spawns a `Thread` (entry function pointer and a
`void*`, a name, an abstract `Priority`, an optional stack size or caller-supplied
stack storage) and makes a `Mutex` and a counting `Semaphore` (with a ceiling; a
ceiling of 1 is an event). `Clock` gives a monotonic `Instant`, sleeps, and
optionally reports wall time. `Duration` is `std::chrono::nanoseconds`, `forever`
is `Duration::max()`, and every blocking call takes a `Duration`.

**Rationale.**

- **The core owns no thread**, so the platform's threading is small: a runner
  thread and an application thread, and the locks between them. A thread spec is
  C-style (function pointer and `void*`) because `std::function` allocates and
  `std::thread` is not on every target.
- **Time-outs are the protocol, so every wait takes a deadline.** `Semaphore::wait`,
  `MessageQueue::send`/`receive` and `UdpSocket::receive_from` all take a
  `Duration`; none can wait for ever by accident. Each desktop implementation
  re-checks the platform's monotonic clock after a wait returns, because the OS's
  wait can end a little early by that clock (the conformance suite found this on
  Windows) and "at least the time asked" is what a protocol time-out needs.
- **A monotonic clock, 64-bit nanoseconds.** Never the wall clock for an
  interval. 292 years of range, so no wraparound handling in the protocol code
  (an RTOS's 32-bit tick counter wraps in 49.7 days at 1 kHz and the port widens
  it).
- **Mutexes are not recursive; semaphores have ceilings** and say so when posted
  past them (`resource_exhausted`), so a lost signal is a visible fault.
- **Destroying a running `Thread` waits for it.** An RTOS cannot stop a task from
  outside, so the interface does not pretend to.

### The priority scheme

Five ordered levels: `lowest`, `low`, `normal`, `high`, `highest`. Each platform
maps them onto its own range in whichever direction that runs; no native number
leaves `src/platforms`. A runner wants `high`: it must answer a TFTP packet inside
2 seconds even while the application is busy. Everything else is `normal`; a log
writer is `low`.

`Thread::priority_outcome()` says what happened: `applied`, `not_permitted` (the
platform has priorities and refused this process them) or `unsupported`. A desktop
carries on at the default when it is refused; the loader is correct either way,
because its guarantees come from time-outs and not from scheduling.

| Level | Windows | Linux | macOS | QNX (unbuilt) | VxWorks (design) | FreeRTOS (experimental) | Zephyr (experimental) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Direction of the native scale | higher = more urgent | higher = more urgent (real-time classes) | higher = more urgent | higher = more urgent, 1 to 255 | **lower = more urgent**, 0 to 255 | higher = more urgent, 0 (idle) to `configMAX_PRIORITIES - 1` | **lower = more urgent**; negative = cooperative |
| `lowest` | `THREAD_PRIORITY_LOWEST` | `SCHED_IDLE` | `SCHED_OTHER` at the range's minimum | 5 | 200 | 1 | the lowest application priority (`K_LOWEST_APPLICATION_THREAD_PRIO`) |
| `low` | `BELOW_NORMAL` | `SCHED_BATCH` | a quarter up the range | 8 | 150 | a quarter of the way below the network task | three quarters of the way down |
| `normal` | `NORMAL` | `SCHED_OTHER` | the middle of the range | 10 (QNX's default) | 100 (the usual application default) | half of the way below the network task | half way down the pre-emptible range |
| `high` | `ABOVE_NORMAL` | `SCHED_FIFO`, a quarter of the way up the FIFO range | three quarters up the range | 30 | 60 (numerically above tNet0's 50: less urgent than the stack's task) | three quarters of the way below the network task (lwIP's `tcpip_thread`, FreeRTOS+TCP's `ipconfigIP_TASK_PRIORITY`: usually `configMAX_PRIORITIES - 2`) | a quarter of the way down |
| `highest` | `HIGHEST` | `SCHED_FIFO`, half way | the top of the range | 50 (an unprivileged process may be held to 63) | 55 | one below the network task, never the stack's | 1 (0 left to the system) |
| Refused when | never (within the normal class) | `SCHED_FIFO` needs `CAP_SYS_NICE` or `RLIMIT_RTPRIO`: `not_permitted` | never | above the process's allowed maximum | n/a | n/a | n/a |

The VxWorks column is a proposal to be settled against the product's priority plan; it is
not code. The FreeRTOS and Zephyr columns are what the code does, with the numbers in
[the section on those ports](#the-freertos-and-zephyr-ports), and a product still has to settle them
against its own plan. What is fixed is the rule that makes them safe:

- **Never produce a cooperative Zephyr priority.** A cooperative thread is never
  preempted by another thread, so a runner mapped to one starves everything below
  it. The map stays in the preemptible range.
- **Stay below the network stack's task.** FreeRTOS+TCP's IP task, VxWorks's
  `tNet0` and lwIP's `tcpip_thread` must run before the runner handling their
  packets, or a packet waits for the thread that waits for it.
- **Do not map to the idle priority**, and leave the very top for the system.

### The clock

| | Windows | Linux, macOS, QNX | VxWorks | FreeRTOS | Zephyr |
| --- | --- | --- | --- | --- | --- |
| Monotonic | `QueryPerformanceCounter` | `clock_gettime(CLOCK_MONOTONIC)` | `clock_gettime(CLOCK_MONOTONIC)` (POSIX clocks), or the tick counter `tickGet()` at `sysClkRateGet()` | `xTaskGetTickCount()`, widened to 64 bits (tick rate `configTICK_RATE_HZ`) | `k_uptime_get()` (64-bit ms) or `k_cycle_get_64()` |
| Sleep | `Sleep`, looped on the clock | `nanosleep`, looped on EINTR | `nanosleep` or `taskDelay` (ticks) | `vTaskDelay` (ticks) | `k_sleep(K_NSEC/K_MSEC)` |
| Resolution | 0.5 to 16 ms per wait (system timer tick); the clock itself is finer | ns | the system clock rate (often 1 ms, 16.7 ms at the 60 Hz default) | one tick | a tick (`CONFIG_SYS_CLOCK_TICKS_PER_SEC`) |
| Wall time | `GetSystemTimePreciseAsFileTime` | `CLOCK_REALTIME` | `CLOCK_REALTIME` if set | none unless the product has an RTC: `unsupported` | `CLOCK_REALTIME` with the POSIX clock option and an RTC, else `unsupported` |

### Locks, semaphores and threads

| | Windows | Linux, macOS | QNX | VxWorks | FreeRTOS | Zephyr |
| --- | --- | --- | --- | --- | --- | --- |
| Thread | `_beginthreadex` (suspended, so priority and name are set first) | `pthread_create`; the thread sets its own priority before the entry function runs | as POSIX | `taskSpawn` / `taskCreate` or `pthread_create` | `xTaskCreate`; **`xTaskCreateStatic`** with the spec's `stack_storage` and a static TCB | `k_thread_create` on a `K_THREAD_STACK_DEFINE` stack (`stack_storage`); never dynamic in a certified build |
| Join | `WaitForSingleObject` | `pthread_join` | as POSIX | `pthread_join` if pthreads are used; a native task has none, so it signals a semaphore as its last act | no `join`: a task signals as its last act and deletes itself | `k_thread_join` |
| Mutex | SRW lock | `pthread_mutex_t` | `pthread_mutex_t` (priority inheritance available) | `semMCreate(SEM_INVERSION_SAFE)` | `xSemaphoreCreateMutex` (priority inheritance) | `k_mutex` (priority inheritance) |
| Semaphore | `CreateSemaphoreW` | mutex + condition variable (macOS has no `sem_init`) | `sem_init`/`sem_timedwait` or as POSIX | `semCCreate`, `semTake(ticks)` | `xSemaphoreCreateCounting` | `k_sem` |
| Timed wait | `WaitForSingleObject(ms)` | `pthread_cond_timedwait` on a monotonic clock (Linux: `pthread_condattr_setclock`; macOS: `pthread_cond_timedwait_relative_np`) | as Linux | ticks | ticks | `k_timeout_t` |

**Gaps and risks.**

- Thread priority on a desktop is advisory. Linux real-time classes need
  privilege; a CI runner is not given it; the tests check that every level starts a
  thread and that `normal` is always `applied`.
- The `Thread` entry function must not exit with an exception: it is
  `noexcept`, and a throw terminates.
- No thread cancellation, no thread-local storage, no thread-safety annotations.
- Stack size: `stack_bytes` is a request. A platform raises it to its minimum.
  Under AddressSanitizer a thread needs more stack than without; the tests use the
  default.

## Area 4: networking and message passing

(`network.hpp`, `ipc.hpp`)

**Interface.** `Network::open_udp({local endpoint, allow_broadcast, reuse_address,
receive_buffer_bytes})` returns a `UdpSocket`: `send_to`, `receive_from(buffer,
timeout)` and `local_endpoint()`. `Network::wait_readable(sockets, timeout, ready)`
waits on up to 64 at once. `Ipc::create_queue` makes a bounded `MessageQueue` of
whole messages with timed `send` and `receive`.

**Rationale.**

- **UDP datagrams, IPv4, and nothing more** (the list above). No TCP, IPv6,
  multicast membership, interface enumeration or name resolution: nothing in 615A
  uses them, and the first four are where RTOS stacks differ most.
- **A socket is created already bound.** There is no unbound, half-open state
  to get wrong. Port 0 asks for an unused port and `local_endpoint()` reports it.
- **A time-out is a result, not an exception.** `receive_from` returns `timeout`; the
  datagram can be any size up to 65507 bytes and says if it was cut (`truncated`).
- **`wait_readable` is how one thread serves the control port and every transfer.**
  It is `poll`, `WSAPoll`, `zsock_poll`, `FreeRTOS_select` and `select` in disguise
  and returns the count of ready sockets, 0 being a time-out.
- **ICMP "port unreachable" never surfaces.** A send to a port nobody listens on
  must not make a later receive on that socket fail; Windows does exactly that
  (`WSAECONNRESET`) unless told not to (`SIO_UDP_CONNRESET`), so the Windows
  implementation says so and the conformance suite checks it on every platform.
- **Binding is exclusive by default** (`SO_EXCLUSIVEADDRUSE` on Windows, the
  default elsewhere): a second socket on the same endpoint fails with
  `address_in_use`, and no other process can take the port from under the loader.
  `reuse_address` lets two sockets share, which FIND needs when a loader and a
  target meet on one machine in a test.
- **Messages are bounded.** A queue of `capacity` messages of at most
  `message_bytes`, whole and in order, with the storage either allocated once at
  creation or supplied by the caller (`QueueSpec::storage`).
- **A queue is built from the platform's own mutex and semaphores** on every platform
  that does not have a better one (`RingIpc`, portable code in `ring_ipc.hpp`). An
  RTOS port replaces it with the system's queue, which can be used from an interrupt
  and inherits priority: a FreeRTOS `QueueHandle_t`, a VxWorks `MSG_Q_ID`, a Zephyr
  `k_msgq` (whose buffer is caller-supplied, matching `QueueSpec::storage`), a QNX
  channel or POSIX message queue.

| | Windows | Linux, macOS | QNX | VxWorks | FreeRTOS | Zephyr |
| --- | --- | --- | --- | --- | --- | --- |
| Stack | Winsock 2 (`WSAStartup` in `make_host`) | BSD sockets | BSD sockets (`io-sock`; older `io-pkt`), `-lsocket` | IPNET stack, BSD sockets | **FreeRTOS+TCP** (`FreeRTOS_socket`, its own API) **or lwIP** (BSD API or `netconn`) | native stack, BSD sockets (`zsock_*`, or POSIX names with `CONFIG_POSIX_API`) |
| Receive with time-out | `WSAPoll` + `recvfrom` | `poll` + `recvmsg` (non-blocking socket) | as POSIX | `select` (confirm `poll`) | `FREERTOS_SO_RCVTIMEO` on FreeRTOS+TCP; `SO_RCVTIMEO` or `select` on lwIP | `zsock_poll` + `zsock_recvfrom` |
| Wait on many | `WSAPoll` | `poll` | `poll` | `select` | `FreeRTOS_select` with a socket set (needs `ipconfigSUPPORT_SELECT_FUNCTION`); lwIP `select`/`poll` | `zsock_poll` (bounded by `CONFIG_NET_SOCKETS_POLL_MAX`) |
| Broadcast send | `SO_BROADCAST` | `SO_BROADCAST` | `SO_BROADCAST` | `SO_BROADCAST` | FreeRTOS+TCP: send to the broadcast address (and receiving a broadcast depends on its configuration: **check**); lwIP: `SO_BROADCAST` | `SO_BROADCAST` support depends on the Zephyr version and its IPv4 options: **check** |
| Port reuse | `SO_REUSEADDR` | `SO_REUSEADDR` + `SO_REUSEPORT` | `SO_REUSEPORT` may be absent: **check** | **check** | not supported by FreeRTOS+TCP: a second bind of a bound port fails | `SO_REUSEADDR` (and `SO_REUSEPORT` from some version): **check** |
| 65 KB datagram | yes | Linux yes; **macOS limits a datagram to its send buffer, 9216 bytes, unless raised** (done: `SO_SNDBUF`) | yes | depends on the buffer pool | **no** unless buffers are sized for it (`ipconfigNETWORK_MTU`, buffer count); a port sizes the block it negotiates (RFC 2348) to what the stack can carry | limited by the net buffer pool |
| Queue | `RingIpc` | `RingIpc` | `RingIpc`, or POSIX `mq_*` / a channel | `msgQCreate`/`msgQSend`/`msgQReceive` | `xQueueCreate`/`xQueueSend`/`xQueueReceive` (items are fixed size: length-prefix as `RingIpc` does) | `k_msgq` (fixed size, length-prefixed likewise) |

**Gaps and risks.**

- **Port 59 is privileged on Linux** (below `net.ipv4.ip_unprivileged_port_start`,
  1024 by default) and a process that is not root or without
  `CAP_NET_BIND_SERVICE` cannot bind it. macOS allows a non-root process to bind a
  privileged port on the wildcard address on recent releases; this has not been
  checked here. Windows has no such rule, but its firewall asks. The standard's own
  answer is the "port" option, which moves the loader's control port to 49152 or
  above for an operation (5.3.2.3.8.6): the loader should offer it, and the
  conformance tests bind high ports only.
- **FIND on a host with several interfaces**: a limited broadcast
  (255.255.255.255) leaves by the one interface the system picks. Binding the
  socket to an interface's address chooses it; directed broadcast addresses and
  enumerating interfaces are not offered. The runner takes the local address from
  its configuration.
- **A loader and a target on one machine** can share FIND's port 1001 only with
  `reuse_address` on both; which socket gets a given datagram is then up to the
  system.
- **The 2 second TFTP time-out on a loaded Windows machine**: waits are in whole
  milliseconds on a timer that ticks every 1 to 16 ms, so a wait can be late by a
  tick (never early; the loop re-checks the clock). A time-out of 2 seconds is
  unaffected; sub-10 ms accuracy is not offered.
- IPv4 only. 615A says IP version 4 (5.3.1), so this is the standard's, not a
  limit of this design, but AFDX gateways and aircraft networks may bring
  addressing that does not fit it.
- A socket is used by one thread at a time. `wait_readable` on one thread and
  `send_to` on another socket is fine; two threads on one socket is not specified.
- The message queue is not safe to use from an interrupt handler; an RTOS port
  that needs that provides the native one.

## The fake platform

`tests/fake` implements every interface in memory: no socket, no file, no real
second.

- **`FakeWorld`** is the clock and an event queue. Time moves only when told to
  (`sleep_for`, a wait with a time-out, `advance_by`), and events due on the way
  run in time order on the calling thread. A test of a 13 second time-out takes
  microseconds and is the same every time.
- **`FakeNetwork`** delivers datagrams after a latency, and can lose, duplicate,
  corrupt (flip one bit) or reorder them (jitter), by profile from a seed (the same
  seed is the same history on every compiler: the generator is SplitMix64) or by a
  filter the test writes ("drop the third DATA packet"). Every datagram sent is
  captured for assertions. An `Agent` at an address stands in for a target: it is
  handed each datagram sent to it and replies through the network. Hosts at
  different addresses share a network; broadcast reaches the others and not the
  sender.
- **`MemoryFileSystem`** is a complete file system in RAM, with a capacity (a full
  disk is `no_space`) and a one-shot write failure.
- **`FakeThreading`** does not run a thread when it is spawned but when it is
  joined, destroyed, or `run_pending()` is called: deterministic, and so unable to
  show two threads racing. A semaphore that is waited on while empty lets the world
  run up to the time-out, so a simulated target can act while a loader waits.
- **`FakeMemory`** has a budget.

### Conformance

`tests/conformance` runs the same checks against two hosts: the real one this
build selected, and the fake. A check that passes on the fake and fails on the real
host means the fake lies; the reverse means it is too strict. Each CI leg runs the
suite on its own platform, so "every real platform implementation" is Windows,
Linux and macOS today and a port's first job tomorrow. The checks that need real
concurrency (threads contending on a lock, a queue between threads, an echo server
over real UDP, one thread serving several sockets) run on the real host only.

## The FreeRTOS and Zephyr ports

Written on the owner's request, each against the one configuration a simulator provides. Read the tables
above for what the operating systems offer; this section is what the code does, what ran, and what did
not. Nothing here has run on a board.

| | FreeRTOS | Zephyr |
| --- | --- | --- |
| Operating system | FreeRTOS-Kernel V11.2.0 | Zephyr 4.4.2 |
| Network stack | lwIP 2.2.1, its sockets API (`lwip_socket`, `lwip_poll`, ...), IPv4 and UDP | Zephyr's own stack, `zsock_*`, IPv4 and UDP |
| Files | littlefs 2.11.3 (`lfs_*`) on a mounted `lfs_t` the product hands over (`freertos/environment.hpp`) | Zephyr's `fs_*` API on whatever the product mounts; tested on littlefs over flash |
| How it is built | A CMake project on a host that builds the kernel, lwIP and littlefs and links the platform layer to them (`tests/platform/freertos`; a product does the same with its own kernel port and configuration) | A Zephyr module (`zephyr/module.yml`, `src/platforms/zephyr/Kconfig`) that an application turns on with `CONFIG_ADL_PLATFORM=y` |
| Where it ran | The kernel's POSIX simulator port on Linux (GCC 15) | `native_sim/native/64` on Linux (GCC 15, the host's libstdc++) |
| Network in the run | lwIP's loopback interface: 127.0.0.1 | Zephyr's loopback interface: 127.0.0.1 |
| Storage in the run | littlefs on a RAM block device | littlefs on the simulated flash |
| CI job | `FreeRTOS (POSIX simulator)` | `Zephyr (native_sim)` |

**What was verified.** Both test programs build with the project's warning set (`-Wall -Wextra -Wpedantic
-Werror` and the rest, on the platform layer's own sources), and run the whole platform layer suite and
the 615A engine tests: the same Catch2 sources the desktops run (`tests/conformance`, the fake and the
engine tests; [testing](testing.md)) and a test of a thread on a stack the caller made, 222 test cases and about
36 200 assertions, all passing. The
conformance cases run against the real host of the port and against the fake, so the fake and the port
were held to each other, as on the desktops. Each program was run ten times in a row without a failure,
and built with CMake 4.2 and 4.4.

**Where the code differs from the proposals in the tables above.** Each of these is something the first
run of the suite, or reading the headers, found:

- **Files.** littlefs gives a file handle the file as it was when it was opened, and holds what a writer has
  written in the writer's cache until a sync, so the interface's promise that a reader sees what a writer
  wrote by the time `write` returned needed work on both ports: a new or emptied file is synced when it is
  created, every write is synced, and a reader looks at the file's size before each read and reopens it when
  it has grown. A reader whose file has been removed keeps the handle it has. On Zephyr this is
  `CONFIG_ADL_FS_COHERENT_HANDLES` (default on; off, another handle sees data after `sync()` or `close()`,
  at the file system's speed). `rename` replaces atomically on littlefs; where a Zephyr file system's
  rename will not replace (FAT), the port removes the destination first, which is not atomic. Exclusive
  creation is `LFS_O_EXCL` on FreeRTOS and a look first on Zephyr, whose `fs_open` has no such flag.
- **Mutex.** A Zephyr `k_mutex` is recursive and the interface's is not, so `try_lock` by the holder is
  refused (the lock count says it is the holder). A FreeRTOS mutex is not recursive and refuses it by
  itself.
- **Semaphore.** FreeRTOS: a counting semaphore, whose give at the ceiling fails, which is
  `resource_exhausted`. Zephyr: a `k_sem` stops at its limit without saying so, so the semaphore is a count
  under a `k_mutex` with a `k_condvar`, as in the POSIX port.
- **Queue.** A kernel queue copies fixed-size items, and a message here is anything up to `message_bytes`
  long. Both ports keep the messages in slots and carry the slot numbers on two native queues, one of free
  slots and one of full ones (`k_msgq`, `xQueueCreate`), so the kernel does the blocking, the ordering and the
  time-outs. The two queues of numbers are the one allocation a queue makes that a caller's `storage` does
  not cover.
- **Threads.** Zephyr: a `k_thread` on a stack from `k_thread_stack_alloc` (`CONFIG_DYNAMIC_THREAD`) or the
  caller's own storage, joined with `k_thread_join`. FreeRTOS: `xTaskCreate`, or `xTaskCreateStatic` on the
  caller's storage. A task cannot be joined, so it signals a semaphore as its last act and waits to be
  deleted, and the joiner deletes it (the kernel then has nothing left of it, which a task that deleted
  itself would not guarantee until the idle task had run). The task function must not be `noexcept`: on the
  POSIX simulator port deleting a task cancels its thread, which unwinds the stack with an exception a
  `noexcept` frame turns into `std::terminate`. A real port does not unwind, and the function is harmless
  there.
- **Memory.** Zephyr: a `k_heap` over an array in `.bss` of `CONFIG_ADL_HEAP_BYTES`. FreeRTOS:
  `pvPortMalloc` with an alignment header (the heap hands out `portBYTE_ALIGNMENT`, a caller can ask for
  4096).
- **Clock.** Zephyr: `k_uptime_ticks`, 64 bits. FreeRTOS: `xTaskGetTickCount` widened to 64 bits by
  counting wraps (it must be read at least once in half a wrap; the runner reads it every time it wakes).
  Neither waits longer than a day (Zephyr) or an hour (FreeRTOS) in one kernel call. Wall time:
  FreeRTOS has none (`unsupported`); Zephyr reports it only once something has set the real-time clock.
- **Network.** `receive_from` reads without blocking and waits with `poll`, as on POSIX. A datagram that did
  not fit is told by `ZSOCK_MSG_TRUNC` on Zephyr and by `MSG_TRUNC` of `recvmsg` on lwIP (which returns the
  datagram's length, so the port keeps the smaller). Zephyr answers a bind to an address no interface has
  with `ENOENT` (and so does an exhausted `CONFIG_NET_MAX_CONN`, which is worth knowing when it appears),
  mapped to `address_unavailable`; lwIP binds anything, so the port looks the address up among the
  interfaces. Whether a socket may send a broadcast is enforced by the port, not left to the stack.

**Priorities, as built.** With `N` the priority of the network stack's task (`Environment::
network_task_priority`, default `configMAX_PRIORITIES - 2`) on FreeRTOS, and `L =
K_LOWEST_APPLICATION_THREAD_PRIO` (`CONFIG_NUM_PREEMPT_PRIORITIES - 1`) on Zephyr:

| Level | FreeRTOS (higher is more urgent) | Zephyr (lower is more urgent) |
| --- | --- | --- |
| `highest` | `N - 1` | 1 |
| `high` | three quarters of `N - 1` | `1 + (L - 1) / 4` |
| `normal` | half of `N - 1` | `1 + (L - 1) / 2` |
| `low` | a quarter of `N - 1` | `1 + 3 (L - 1) / 4` |
| `lowest` | 1 | `L` |

Each is at least 1 on FreeRTOS (0 is the idle task's) and is never negative on Zephyr (a cooperative thread
is never pre-empted). With the simulator's configuration (`configMAX_PRIORITIES` 8, `N` 6) the levels are
5, 3, 2, 1 and 1; with `native_sim`'s 15 pre-emptible priorities they are 1, 4, 7, 10 and 14. The
outcome is always `applied`. **A product still has to check the plan against its own threads**: on
Zephyr the stack's receive and transmit threads have priorities of their own
(`CONFIG_NET_RX_THREAD_PRIORITY` and the traffic classes), and no level here is chosen against them.

**What the simulators cannot show, and nobody has checked.**

- Any board: interrupt latency, the MPU or userspace (`stack_storage` is used as the caller made it and
  must be aligned for the architecture), the cooperative or tickless configurations, SMP.
- A real link. Both runs use the loopback interface only, so FIND's broadcast has not left a machine,
  ARP and a link-layer MTU have not been in the way, and a 65 KB datagram has never been fragmented on
  the wire. The loopback is given a 65535 byte MTU (Zephyr) and the stack's pools are sized for one
  such datagram in flight twice over (both); a product's `blksize` has to be what its stack can carry
  (RFC 2348), and reassembly (`IP_REASSEMBLY`, `CONFIG_NET_IPV4_FRAGMENT`) has not been exercised.
- Real flash: littlefs on a NOR device with its wear and its power losses, and the cost of a sync per
  write (the reason `CONFIG_ADL_FS_COHERENT_HANDLES` exists).
- A toolchain that is not GCC 15 with the host's libstdc++. A target needs a C++23 standard library with
  `<expected>` and `<span>` (the Zephyr SDK's GCC and its libstdc++ have them from GCC 13), and the test
  programs enable exceptions and RTTI for Catch2; the platform layer and the libraries do not need
  them, and nobody has built them with `-fno-exceptions` on an embedded libstdc++.
- No allocation after start-up. The platform layer allocates a handle per object, as the desktops do
  ([Area 2](#area-2-memory-memoryhpp)), and the two ports do not yet draw them from fixed slots.
- A loader against a target through the real stacks: the loopback integration test needs the Qt-based
  mock target, which does not run on either.
- The other network stacks and file systems of the design (FreeRTOS+TCP, FAT and FatFs): a port to
  one is a new `network_` or `files_` source and not a change to the rest.

## Selecting a platform

A desktop build needs nothing. A cross build names its target:

```sh
cmake -DADL_TARGET_PLATFORM=freertos --toolchain <toolchain file> ...
```

`ADL_TARGET_PLATFORM` is a cache variable read in one place, `cmake/Platform.cmake`,
which is exempt from the rule against testing the operating system; every other
file reads what the platform's fragment declared. It accepts `windows`, `linux`,
`macos`, `qnx`, `ghs508`, `ghs178b`, `vxworks`, `freertos` and `zephyr`. QNX is
also recognised from `CMAKE_SYSTEM_NAME`, the one RTOS that CMake names (the two
INTEGRITY targets are named through the GHS toolchain files, which set it as
`Generic`). A platform's fragment declares
`ADL_PLATFORM_STATUS` (`supported`, `experimental`, `unbuilt` or `design-only`); the selector
refuses a design-only platform outright and an unbuilt one unless
`-DADL_ACCEPT_UNBUILT_PLATFORM=ON`. A platform whose image a project of its own builds (FreeRTOS and
Zephyr: an RTOS image is built around the product's kernel configuration, port and main, and Zephyr's
around `find_package(Zephyr)`) declares where, in `ADL_PLATFORM_PROJECT`, and this project refuses it with
that pointer; [building](building.md) says how each is built. Two tests hold this:
`platform.fragments` (every fragment meets the contract) and `platform.selector`
(an explicit target is understood, and the refusals say why).

## Adding a port

1. Read the platform's section above and check every claim against its headers.
2. Implement each header in `interfaces/adl/platform/` in
   `src/platforms/<name>/`, reusing `posix/` where the system is POSIX (QNX does:
   it adds a handful of functions, declared in `posix/posix_os.hpp`).
3. Fill in the fragment, set its status to `unbuilt`, and list the files.
4. Build it. Run the conformance suite on the target (the suite needs only the
   interfaces, a Catch2 that runs there or a small harness that calls the same
   functions). `tests/platform/rtos_support/test_sources.cmake` lists the sources the two RTOS
   programs run, and `tests/platform/rtos_support/temp_dir.hpp` stands in for the host-file-system
   scratch directory of the desktop tests.
5. Change the status to `experimental` when CI builds it and runs the suite on a simulator, and to
   `supported` only when it runs on the target.

## Gaps and risks

The ones that matter most, in the order they are likely to bite:

1. **macOS has not run.** It shares the POSIX sources with Linux and differs in
   `macos/os_macos.cpp`: the condition-variable wait, `F_FULLFSYNC`, the priority
   range, thread naming. The UDP datagram limit (9216 bytes by default) is worked
   around with `SO_SNDBUF`, which is not yet seen to work.
2. **The embedded targets are unproven.** FreeRTOS and Zephyr have run on simulators and on nothing
   else ([what that leaves open](#the-freertos-and-zephyr-ports)). VxWorks 178C is a
   certifiable profile whose available API subset (sockets? a file system?
   `pthread`?) is the first thing to establish; nothing here assumes more than the
   standard VxWorks APIs named above, and none of it has been checked against the
   178C profile.
3. **`adl::arinc665` allocates**, so "no allocation after start-up" is not true of
   any program that decodes a header file. The route is in Area 2.
4. **Priorities on a desktop are advisory**, and a runner may be late.
5. **Port 59 is privileged on Linux.**
6. **No wall clock on most RTOS targets**; `DNLD_INFO` dates and log timestamps
   need a source from the product.
7. **The standard is silent on threads, memory and priorities**, so the whole
   threading model here is this project's choice, and the owner may want a different
   one for a certified target.
