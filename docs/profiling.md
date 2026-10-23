# Profiling

Tracy 0.13.1, as an opt-in build. A normal build links nothing and opens no listener; the client is
selected by one switch, and the same switch decides whether the instrumentation in the libraries,
the application's engine and the window's frame loop is compiled in or expands to nothing.

```powershell
cmake --preset windows-msvc-release -DADL_BUILD_APP=ON -DADL_ENABLE_TRACY=ON
cmake --build --preset windows-msvc-release
```

That adds vcpkg's `profiling` manifest feature: the Tracy **client** (`Tracy::TracyClient`, a shared
library) and, for the host, Tracy's command-line tools (`tracy-capture`, `tracy-csvexport`, …), so a
trace can be recorded and dumped without the GUI profiler. The version is vcpkg's pinned port,
0.13.1; the GUI profiler used to read a trace should be the same release.

**Do not ship it.** The client opens a TCP listener and answers UDP discovery broadcasts, which a
loader on an aircraft network must not do. Release presets leave `ADL_ENABLE_TRACY` off.

## The instrumentation names

The repository forbids `#if` and `#define` in its own C++ (see `tools/checks/check_platform_isolation.ps1`),
so `<adl/profiling.hpp>` is **generated** by `cmake/Tracy.cmake`: one flavour calls Tracy, the other does
nothing, and both offer the same names in `namespace adl::profiling`. No instrumented file mentions Tracy.

| Name | Meaning |
| --- | --- |
| `Zone` | a scoped zone, named on construction and located by `std::source_location::current()` |
| `frame_mark()` | a frame boundary |
| `plot(name, value)` | a value plotted over time |
| `thread_name(name)` | the thread's name in the profiler |
| `app_info(text)` | the program, for the trace's header |

```cpp
#include <adl/profiling.hpp>

void LoaderModel::uploadSelected() {
    const adl::profiling::Zone zone{"app: upload selected"};
    ...
}
```

## What is instrumented

Zones are named `<area>: <action>`, so a trace can be read by area and by operation.

| Area | Zones |
| --- | --- |
| ARINC 665 | build load, verify load, check load, build media set, check value update (the CRC/MD5/SHA-1 engine, the hot loop of every verification) |
| media tree | write set, read set |
| ARINC 615A engine | start operation, status file, file received, file sent, TFTP read/write start |
| 615A runner | run operation (the whole job), find |
| Application engine | find job, information job, upload job, download job, download operator job, read media set job, check media set job |
| Application core | read and validate media set, index the media set, open a media set file, progress to the load/download model, the operator's actions (choose media set, check media set, find targets, read information, upload, download) |
| Window | `app: load QML` (compile and instantiate), frame marks, `ui.frame_ms`, `ui.fps`, `ui.render_ms` |

The window's frame loop is `apps/app/frame_profiler.cpp`: Qt emits `frameSwapped` on the GUI thread
(the frame mark and the interval) and `beforeRendering`/`afterRendering` on the render thread (the
render time, connected directly so it is measured where it happens). Threads are named `gui`,
`engine` and Qt's render thread.

Only the main application has zones of its own: `adl_media_gen` and `adl_mock_target` show only the zones
of the libraries they use.

Not instrumented, and why:


- **GPU zones.** Tracy's GPU zones need the graphics API's own instrumentation; Qt's RHI does not
  expose it. The render thread's CPU time is plotted, the GPU's is not.
- **3D.** This window draws no 3D scene; Qt Quick's 2D scene graph is what the frame plots cover.
- **QML bindings and JavaScript.** There is no QML-level instrumentation; the C++ the QML calls is
  zoned, and the frame marks show when the scene graph is not keeping up.
- **Platform sockets and files.** The zones sit at the layers above them (media tree, media source,
  TFTP), where the meaning is; a per-recv zone would swamp the trace.

## Taking a trace

### With the GUI profiler

Build with the switch, run the program, and start the Tracy profiler (0.13.1): it finds the client
by broadcast, or by address. The application is instrumented on demand (`TRACY_ON_DEMAND`), so it
does nothing until the profiler connects.

### Headlessly (the CLI tools)

The tools are installed by the host part of the feature. vcpkg installs into the build tree, so they are
under `<build tree>/vcpkg_installed/<triplet>/tools/tracy/` (`build/windows-msvc-release/` in the
commands above):

```powershell
# A workload with the window and its operations, offscreen, as the trace:
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"

# tracy-capture listens for the client, writes the trace, and stops when the client disconnects.
build\windows-msvc-release\vcpkg_installed\x64-windows\tools\tracy\tracy-capture.exe -o trace.tracy -f

# In another shell, run the workload (the window test drives the first-run screen, a FIND, an
# Information operation, a media set check, an Upload and a Download):
build\windows-msvc-release\bin\adl_app_qml_test.exe

# Then flatten the trace to CSV:
build\windows-msvc-release\vcpkg_installed\x64-windows\tools\tracy\tracy-csvexport.exe trace.tracy > trace.csv
```

A capture holds zone begin/end pairs with their times; `tracy-csvexport` emits one row per zone
with its name, thread, and timing. Because the client is built for on-demand profiling, start the
capture before the workload.

## Reading the trace

- The **frame marks** separate frames; `ui.frame_ms` above ~16.7 ms means the GUI is not holding
  60 Hz, and `ui.render_ms` splits the render thread's part from the GUI thread's.
- The **app: load QML** zone is the window's start-up cost; its children (Qt's own work) are not
  instrumented, so read it as one figure.
- An operation is the `615a: run operation` zone; inside it, the engine's status files, its file
  transfers and the media set reads appear as their own zones. The engine thread is `engine`.
- For an upload the volume is in `app: index the media set` (once, when the set is read) and in the
  transfers (`615a: file sent`); the check value update is the CPU cost of verification.
- A **progress** stall shows as a long `app: progress to the load/download model` or a gap between
  status files, not as a long transfer.

## A baseline run

The window test (Release, Tracy on, offscreen software rendering, this repository's build) is the
reference workload: a first-run screen, a FIND, an Information operation, a media set read and
check (its fixture is a few KiB), an Upload and a media Download against the simulated target. The
figures below are from one such capture on a Windows workspace; absolute numbers are not the point
(the fixture is tiny) — the *shape* is what a profile of the real thing starts from.

The headless suite (`adl_app_tests`, 43 tests over 4.9 s when it was captured; it has more tests now and
the figures were not re-measured) produced 2,292 zones — the figures below are from that capture, with the
window test's capture for the frame loop.

| Zone | Calls | Mean | Max | Reading |
| --- | ---: | ---: | ---: | --- |
| `media_tree: write set` | 17 | 15.7 ms | 17.4 ms | A media set written to disk. The cost is the OS: one directory per member, a handful of files, each created and closed (Windows file-system and AV cost dominates; the code itself is a few `ofstream` writes). This is `adl_media_gen`'s and the tests' path, not the loader's. |
| `media_tree: read set` | 36 | 1.8 ms | 7.6 ms | The loader's path, once per chosen media set: enumerate the members and read the three index files. Dominated by opening files, not parsing. |
| `app: read and validate media set` | 36 | 1.9 ms | 7.7 ms | The read plus ARINC 665 verification, on the **engine** thread; the difference from the row above is the verification. |
| `615a: run operation` | 24 | 0.21 ms | 0.65 ms | One whole 615A operation against the simulated target (no network latency). |
| `arinc665: check value update` | 1,297 | 2.0 µs | 23.5 µs | The CRC/MD5/SHA-1 engine: verification is cheap per block; a large set scales with bytes, not with file count. |
| `arinc665: build load` / `build media set` | 35 / 19 | 37 µs / 68 µs | 65 µs / 81 µs | The formats themselves are negligible next to the file system. |
| `615a: file sent` / `file received` | 180 / 41 | 6 µs / 8 µs | 157 µs / 205 µs | Per-file handling in the engine. |
| `app: load progress to model` / `download progress to model` | 16 / 97 | 1.5 µs / 0.4 µs | 7 µs / 3 µs | Progress updates into the Qt models are not a cost. |
| `ui.frame_ms` (window test) | 20 frames | ~16 ms | — | 60 Hz held while the window ran offscreen with the software renderer; `ui.render_ms` is plotted alongside it per frame. |

### What the profile says

- **In this capture the application is not the hotspot.** The capture is small: a fixture of a few KiB,
  20 frames, offscreen with the software renderer. The engine thread does its whole job in fractions of a
  millisecond against the simulated target; the GUI thread's actions (upload, download, read
  information, check media set) are queued calls that return in microseconds (the outliers are below).
  A long operation has not been profiled, so whether the window keeps drawing at 60 Hz during one is
  not shown here; it is what the design (engine on its own thread, signals back) is meant to give.
- **The file system is.** Writing a set costs ~16 ms for a tiny one; reading one costs ~2 ms.
  Nothing in the parsing or the checksums is worth attention at this scale. For a real media set
  the read and the verification scale with bytes and files, and that is where the next trace
  should look (a many-hundred-MB set, a real target).
- **Two GUI-slot outliers exist and are not code hotspots.** `chooseMediaSet` includes the
  settings write-through (`QSettings::sync`, deliberate: a preference has to survive a crash) at
  ~4 ms; `uploadSelected`/`downloadSelected`/`readInformation` show minima near 0 and maxima of
  9–22 ms, which is wall-clock sampling of the GUI thread while the engine thread starts and runs
  the job, not work inside the slot. If it ever matters, the settings writes are the only ones a
  code change could remove, at the cost of the crash-safety the class documents.

### What to profile next

- An upload of a real (hundreds of MB) media set to a target over a real network: the engine's
  transfer path and the platform's sockets, not the formats, will dominate; the zones are there.
- The generator writing a large set: the write path's per-file cost is the number to watch, and
  it is OS-bound.
- GPU zones, if the renderer ever becomes the limit: they need the RHI's own instrumentation,
  which this build does not add.
