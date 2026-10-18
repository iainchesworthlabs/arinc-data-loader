# The application

`src/app` is the Qt 6.12 application: a window over the libraries, and nothing else. It holds no protocol
logic and no file format. Everything it knows about ARINC 615A and 665 it asks of `adl::arinc615a`,
`adl::arinc615a_runner`, `adl::arinc665` and `adl::media_tree`, and everything it knows about the system
it asks of `adl::platform`. Qt is used here and nowhere else.

Two slices are built: the skeleton with Information, and Upload. What the window does today:

| What | How |
| --- | --- |
| Choose a media set | A directory with one directory per member (what `adl::media_tree` writes and `adl_write_payloads` makes). It is read with `read_media_set_tree` and `read_media_set`, and shown: the part number, the number of members, each load with its header file and the number of its data and support files, and every finding (where the set differs from what the standard says it should be). Each load has a check box; the ticked loads are what an Upload sends. |
| Check a media set | The validation gate: reading the set **is** checking it (the read verifies every file and its check values), so a set that reads is structurally valid and may be uploaded; a set that does not read is refused with the named reason, and Upload stays disabled. The check runs when the set is chosen (a preference can turn that off) and again on demand, rebuilding the index, so files changed on disk after the first read are caught. Findings are deviations from the standard's recommendations: they are listed as warnings and do not block an upload. |
| Media set properties | A dialog with the set's identity (part number, members, batches), its loads, every file of `FILES.LUM` (name, member, pathname, CRC) and the findings. |
| Find targets | FIND (Attachment 3): broadcast, three seconds. The targets that answered are listed; choosing one fills in its address and its `<THW_ID_POS>`. Target properties shows the answer's fields (hardware identifier, type name, position, literal name, manufacturer code). |
| Name a target by hand | An IPv4 address and a `<THW_ID_POS>` can be typed, for a target that does not answer FIND (or over AFDX, which has no broadcast). |
| Read information | The Information operation (5.4.2): the target's status files and its list of hardware and parts, with an abort button. |
| Upload | The Upload operation (5.4.3): the ticked loads of the media set are loaded to the target. The target reads their header, data and support files from the media set on disk; the window shows how far the target's list has got and, beside each ticked load, its own percentage and status (and why, for one the target gave up on), from the upload status files. With the abort button. See [what the Upload operation needs](#what-the-upload-operation-needs-from-the-application). |
| Preferences | Persistent settings, in a dialog from the File menu: the last media set directory (restored quietly at start-up), a default target address and `<THW_ID_POS>` (prefilled), the loader's control port (anything but 59 is sent to the target as the `port` option, which is what lets the loader and a target share one machine), the download directory, whether FIND runs by itself before an operation with no chosen target (one answer is taken automatically; none or several leaves the choice to the operator), whether a chosen media set is checked at once, and the activity log's capacity. |
| The activity log | What the target said (its answer, each status file) and the engine's notices (a lost or repeated status file, a refused request, a checksum that did not match, a WAIT), with the time, oldest first. |
| The outcome | How the operation ended (the engine's `OutcomeCode`), and for an abort whether the target confirmed that it had stopped. Failures also open a dialog once, with the outcome, the detail and, where the application knows one, a hint (a host that could not start, an address that is not an address, a media set that could not be read, an abort the target did not confirm); a status bar carries the current state, the target and the media set. |

Download (media and operator defined), the choice of the `DNLD_DATA_<THW_ID_POS>_<number>` directory
(5.4.4.3) and running FIND automatically before an operation are the next slices (the
[roadmap](roadmap.md)); of those, FIND-before-an-operation is now a preference (above), and the
rest remain.

## Layout

```text
src/app/
  main.cpp            creates the QGuiApplication, one LoaderModel and the QML engine
  qml/                Main.qml (the window), Panel.qml, LogView.qml, the preferences,
                      properties and about dialogs
  core/               adl_app_core: everything but the window, Qt Core only
    engine_worker     the engine on its own thread, and the abort
    loader_model      what the QML talks to: properties and a few invokable functions
    settings          the persistent preferences (QSettings), read at start-up
    models            the five list models (the loads carry the operator's ticks and the progress)
    media_set_summary reading a media set from a directory, as plain values
    media_source      the files of an upload: an index of the media set's files, and a FileSource over it
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
  loader's control port. While one runs the ticks beside the loads cannot be changed.
- **Everything the engine reports is copied at once.** An `OperationListener` is handed views that are
  valid during the call only, so `InformationListener` turns each into `QString`s and emits a signal.
  Qt delivers the signals to the window's thread in the order they were emitted, so the log reads in the
  order things happened.
- **The abort cannot be a queued call**, because the worker's thread is inside the runner, not in its
  event loop. It is an atomic flag (`AbortGate`), which is also the runner's `RunController`: the runner
  polls it (every 100 ms by default) and the first poll that sees it calls `DataLoader::abort` once, with
  `aborted_by_operator`. Both operations (Information and Upload) use it. The flag is cleared by `LoaderModel` before it queues a job, so an abort left
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

## What the Upload operation needs from the application

The Upload operation (`UploadOperation`) names the loads to send by `LoadReference`: the header file's
name and the load's part number, which is what `LoadSummary` carries for each load of the set. The
operator ticks loads in the window (`LoadModel`'s writable `checked` role); `LoaderModel::uploadSelected`
hands the ticked ones to the worker, which builds the request, runs it through the same runner as
Information, and reports through the same `JobListener`: each upload status file becomes a line in the
log (the counter, the status, the percentage of the list, the time left if the target says) and a
progress report (the list's percentage, and per load its percentage, status and description), which
`LoaderModel` turns into the `progress` property and the loads' `ratio`, `loadStatus`, `failed` and
`problem` roles. Each file the target reads is a line in the log (`on_file_sent`). The operation is
refused before it starts, and the button disabled, unless there is a media set that was read, a load
ticked, and a valid address and `<THW_ID_POS>` (`canUpload`).

**The files come from the media set on disk through `adl::platform`'s file system, not from memory.**
While the media set is read (to show it), `MediaSetIndex` is made from its `FILES.LUM`: every file's name,
member, directory and 16-bit CRC, mapped to a path below the directory the operator chose (member
directory, then the file's own directory: `adl::media_tree`'s layout). Nothing but the index is kept;
the bytes are read at the offsets the target asks for, when it asks (a media set can be larger than
memory). `MediaTreeSource` is the `FileSource` over it, and reads through the host's `FileSystem`, which
is what lets the tests run an upload against files in memory.

**Finding the file the target means.** A media set can hold several files of one name (in different
directories of a load, or on different members), and the target tells them apart by the checksum option
of its request (5.3.2.3.8.5), the CRC the load header gives that file. `MediaSetIndex::find` takes the
file of that name whose `FILES.LUM` CRC is the one asked for. Two decisions, both mine and unchecked
against a target:

- With **one** file of the name, it is served whatever CRC the request carries (and whatever kind of
  checksum). The header file's CRC as a target knows it may not be the one `FILES.LUM` records (the header
  ends in its own CRC), and the target checks what it received against what it asked for anyway.
- With **several** files of the name and a CRC none of them has, there is no such file (`not_found`, the
  target ends the operation); with several and no CRC in the request, the first one listed.

The engine's `DirectoryFileSource` (one flat directory) was not used because a media set is not one:
it has a directory per member and directories inside them.

**What was read is what is uploaded, until the operator reads again.** The index is made when the media
set is read, and kept by the worker; the files themselves are opened when the target asks. A file
changed or removed on disk after the set was read is not noticed by the application: a changed one
arrives as it now is and the target's checksum check says so; a removed one is `not_found` for the
target, which ends the operation. A media set that cannot be read leaves no index, and nothing to upload.

**How the operation is judged.** `success` is the engine's `completed` with no load the target reported
as failed (`items_failed`); a completed operation with a failed load is shown as a failure ("completed,
1 load(s) failed"), and the detail says how many files the target read. A load that failed is named in
the log once, with the target's reason.

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
platform (Information, FIND, an Upload of a media set written to a directory and read back through the
index, its progress and its end, a load the target cannot read, an abort of an Upload, a second job refused,
a host that cannot start, a media set read from a directory and one that is not there, the model destroyed
while a job is queued), `MediaTreeSource` and the index (two files of one name told apart by the CRC, the
fall-backs above, offsets, the limit on open files), the models (ticks, locking, progress, and the files of
`FILES.LUM`), the address parser and the notice texts; the preferences (round-trip through `QSettings`, the
control port reaching the engine as the `port` option, FIND before an operation with none, several or one
target answering, the quiet start-up restore); the validation gate (an unreadable set — a corrupted byte, a
removed file — leaves the upload refused with the reason, a set that reads is uploadable, findings warn but
do not block, a re-check catches files changed on disk, the operator's ticks survive a re-read); the failure
hints; and the window itself, loaded offscreen with every QML warning an error, driven through a FIND, an
Information operation, a media set check and an Upload (the load ticked and the button pressed through the
window's own items).

Not checked: an Upload against a real target (and so which checksum a real target sends for each file,
see above), an Upload whose files are in two directories of one name end to end (the source is tested
on that, the operation is not), a media set of many gigabytes (files are read by block, not held, and the
test sets are a few kilobytes), an abort in the middle of a transfer through the window (the engine's
tests cover it; the application's abort test holds the engine at the start of the job); the window against a real target or a real network (the runner over real sockets has not been
run either, [engine](arinc615a-engine.md#what-has-been-checked-and-what-has-not)); the window on a real
screen at other scales and in a dark theme (the colours take the palette's lightness, and have been looked
at in the light one only); keyboard use and a screen reader beyond the accessible names the controls carry;
translations (the strings go through `tr`/`qsTr`, and there is no catalogue).
