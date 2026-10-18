# The application

`src/app` is the Qt 6.12 application: a window over the libraries, and nothing else. It holds no protocol
logic and no file format. Everything it knows about ARINC 615A and 665 it asks of `adl::arinc615a`,
`adl::arinc615a_runner`, `adl::arinc665` and `adl::media_tree`, and everything it knows about the system
it asks of `adl::platform`. Qt is used here and nowhere else.

This is the first slice. What the window does today:

| What | How |
| --- | --- |
| Choose a media set | A directory with one directory per member (what `adl::media_tree` writes and `adl_write_payloads` makes). It is read with `read_media_set_tree` and `read_media_set`, and shown: the part number, the number of members, each load with its header file and the number of its data and support files, and every finding (where the set differs from what the standard says it should be). It is **shown, not yet used**: the Upload operation will load from it. |
| Find targets | FIND (Attachment 3): broadcast, three seconds. The targets that answered are listed; choosing one fills in its address and its `<THW_ID_POS>`. |
| Name a target by hand | An IPv4 address and a `<THW_ID_POS>` can be typed, for a target that does not answer FIND (or over AFDX, which has no broadcast). |
| Read information | The Information operation (5.4.2): the target's status files and its list of hardware and parts, with an abort button. |
| The activity log | What the target said (its answer, each status file) and the engine's notices (a lost or repeated status file, a refused request, a checksum that did not match, a WAIT), with the time, oldest first. |
| The outcome | How the operation ended (the engine's `OutcomeCode`), and for an abort whether the target confirmed that it had stopped. |

Upload, Download (media and operator defined), the choice of the `DNLD_DATA_<THW_ID_POS>_<number>`
directory (5.4.4.3), a progress bar with a figure, and running FIND automatically before an operation are
the next slices (the [roadmap](roadmap.md)).

## Layout

```text
src/app/
  main.cpp            creates the QGuiApplication, one LoaderModel and the QML engine
  qml/                Main.qml (the window), Panel.qml, LogView.qml
  core/               adl_app_core: everything but the window, Qt Core only
    engine_worker     the engine on its own thread, and the abort
    loader_model      what the QML talks to: properties and a few invokable functions
    models            the four list models
    media_set_summary reading a media set from a directory, as plain values
    notice_text       an engine notice as a line for the operator
    events, address   plain values, and the IPv4 parser
    host_access       where the worker gets the platform from
tests/app/            QtTest, and the window loaded offscreen
```

`core` has no QML and no GUI, so it is tested headless. The QML is a view: it binds to `LoaderModel`'s
properties and the four models and calls its functions. The window gets the model as a required initial
property (`app`), so there is no global and no singleton.

## The engine and the window: threads

The 615A runner is **blocking and single-threaded**: `Runner::run` does not return until the operation is
over, and `Runner::find` not until the window of three seconds has passed. So the window never calls it.

```text
   window's thread                           engine thread (QThread "engine")
   ---------------                           --------------------------------
   LoaderModel  --- queued call ---------->  EngineWorker::find / read_information / read_media_set
        ^                                         |  Runner::run(loader, op, {controller = &gate})
        |                                         |    the listener (OperationListener) copies what
        +------ queued signals, in order ---------+    it is handed into Qt values and emits them
        |         log_line, targets_found,                
        |         configuration_read, finished           AbortGate::on_poll (every 100 ms)
        |                                                       ^
        +------ AbortGate::request() (an atomic flag) ----------+
```

- **A job is one queued call and ends with exactly one `finished` signal**, whatever happened (the engine
  refused the request, the host could not be started, the address was not an address). While one runs
  `busy` is true and `LoaderModel` refuses another: there is one engine thread, and the operation owns the
  loader's control port.
- **Everything the engine reports is copied at once.** An `OperationListener` is handed views that are
  valid during the call only, so `InformationListener` turns each into `QString`s and emits a signal.
  Qt delivers the signals to the window's thread in the order they were emitted, so the log reads in the
  order things happened.
- **The abort cannot be a queued call**, because the worker's thread is inside the runner, not in its
  event loop. It is an atomic flag (`AbortGate`), which is also the runner's `RunController`: the runner
  polls it (every 100 ms by default) and the first poll that sees it calls `DataLoader::abort` once, with
  `aborted_by_operator`. The flag is cleared by `LoaderModel` before it queues a job, so an abort left
  over from an earlier job cannot stop the next. The target is told by the engine (an `ABORT` on every open
  transfer and in answer to the next status request); the operation ends when the target confirms or after
  the 13 second data load protocol time-out.
- **A deferred file choice** (an operator defined download) will use the same route: `RunController` is
  where `DataLoader::provide_selection` is called from, with the choice taken from a flag in the same way.
- **Closing the window** aborts what is running and waits for the engine's thread. For an operation the
  target has to confirm, that can be up to the 13 second time-out; a FIND, three seconds.
- **The platform's `Host` is made on the engine thread, once per job** (`system_host_factory`), and
  released at its end, so Winsock (on Windows) is started and stopped around each job and nothing is held
  between them. The factory is a constructor argument of `LoaderModel`, which is how the tests give it a
  host that runs in memory.

## What the Information operation needs from the application

`LoaderConfig` wants the target's endpoint, its `<THW_ID_POS>` and a workspace; the engine allocates
nothing, so the worker gives it a `std::vector<std::byte>` of `DataLoader::workspace_bytes` and keeps the
`DataLoader` (about 16 KB) on the heap. An Information operation has no `FileSource` or `FileSink`.

`<THW_ID_POS>` is, in this application, the hardware identifier from the FIND answer, an underscore and
the position (`HNPFMS` at `L` is `HNPFMS_L`), or the identifier alone for a target that gave no position.
That reading is from the way the standard's examples are written, and **has not been checked against a
real target**; the field can be edited, which is the way round it.

## Qt in this repository

- **Qt is the prebuilt kit, not vcpkg.** `cmake/FindQt6.cmake` finds it through `QT_ROOT_DIR` (set by
  `. C:\Repos\common\env.ps1`, and by CI) or the aqtinstall layout each platform fragment lists.
- **`ADL_BUILD_APP` is off by default**, so the libraries build on a machine without Qt. CI and the
  presets of anyone building the application turn it on: `cmake --preset windows-msvc-debug -DADL_BUILD_APP=ON`.
- **Qt's keyword macros are off** (`QT_NO_KEYWORDS`): `emit`, `signals`, `slots` and `foreach` are
  `Q_EMIT`, `Q_SIGNALS`, `Q_SLOTS` and `Q_FOREACH`. `forever` is one of Qt's macros and is also the name
  of the platform layer's "no time limit", so the macros would stop the platform headers compiling.
- **No `#if`, OS header or `QSysInfo`/`Q_OS_*`/`Qt.platform` in `src/app` either**
  ([platform isolation](platform-isolation.md)): what differs between desktops is in the platform
  fragments (`adl_platform_configure_app` makes the Windows program a GUI-subsystem one and the macOS
  one a bundle; `adl_platform_deploy_qt_runtime` has windeployqt copy Qt's DLLs, plugins and the QML
  modules the window imports beside the program).
- **The style is Fusion**, drawn by Qt Quick itself, so the window looks the same on the three desktops;
  `QT_QUICK_CONTROLS_STYLE` overrides it.
- **A role named `display` cannot be used in a delegate that is an `AbstractButton`** (it has a final
  property of that name), which the test that loads the window reports ("Cannot override FINAL property"); the target list's
  role is `summary`.

## What has and has not been checked

Checked: the logic through `LoaderModel` on its engine thread against the simulated target on the fake
platform (Information, FIND, an abort, a second job refused, a host that cannot start, a media set read
from a directory and one that is not there, the model destroyed while a job is queued), the models, the
address parser and the notice texts; and the window itself, loaded offscreen with every QML warning an
error, driven through a FIND and an Information operation against the simulated target.

Not checked: the window against a real target or a real network (the runner over real sockets has not been
run either, [engine](arinc615a-engine.md#what-has-been-checked-and-what-has-not)); the window on a real
screen at other scales and in a dark theme (the colours take the palette's lightness, and have been looked
at in the light one only); keyboard use and a screen reader beyond the accessible names the controls carry;
translations (the strings go through `tr`/`qsTr`, and there is no catalogue).
