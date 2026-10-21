# The application

`src/app` is the Qt 6.12 application: a window over the libraries, and nothing else. It holds no protocol
logic and no file format. Everything it knows about ARINC 615A and 665 it asks of `adl::arinc615a`,
`adl::arinc615a_runner`, `adl::arinc665` and `adl::media_tree`, and everything it knows about the system
it asks of `adl::platform`. Qt is used here and nowhere else.

FIND, the Information, Upload and Download operations are built. What the window does today:

| What | How |
| --- | --- |
| Choose a media set | A directory with one directory per member (what `adl::media_tree` writes and `adl_write_payloads` makes). It is read with `read_media_set_tree` and `read_media_set`, and shown: the part number, the number of members, each load with its header file and the number of its data and support files, and every finding (where the set differs from what the standard says it should be). Each load has a check box; the ticked loads are what an Upload sends. |
| Check a media set | The validation gate: reading the set **is** checking it (the read verifies every file and its check values), so a set that reads is structurally valid and may be uploaded; a set that does not read is refused with the named reason, and Upload stays disabled. The check runs when the set is chosen (a preference can turn that off) and again on demand, rebuilding the index, so files changed on disk after the first read are caught. Findings are deviations from the standard's recommendations: they are listed as warnings and do not block an upload. |
| Media set properties | A dialog with the set's identity (part number, members, batches), its loads, every file of `FILES.LUM` (name, member, pathname, CRC) and the findings. |
| Find targets | FIND (Attachment 3): broadcast, three seconds. The targets that answered are listed; choosing one fills in its address and its `<THW_ID_POS>`. Target properties shows the answer's fields (hardware identifier, type name, position, literal name, manufacturer code). |
| Name a target by hand | An IPv4 address and a `<THW_ID_POS>` can be typed, for a target that does not answer FIND (or over AFDX, which has no broadcast). |
| Read information | The Information operation (5.4.2): the target's status files and its list of hardware and parts, with an abort button. |
| Upload | The Upload operation (5.4.3): the ticked loads of the media set are loaded to the target. The target reads their header, data and support files from the media set on disk; the window shows how far the target's list has got and, beside each ticked load, its own percentage and status (and why, for one the target gave up on), from the upload status files. With the abort button. See [what the Upload operation needs](#what-the-upload-operation-needs-from-the-application). |
| Download | The Download operation (5.4.4), in both modes. Media defined: the operator names the files the target is to send. Operator defined: the target's list of files is shown to be ticked, the choice goes back through the engine's deferred-selection route, and "Choose none" cancels the operation the way the standard says. The files land in a fresh `DNLD_DATA_<THW_ID_POS>_<number>` directory under the download directory (Preferences): the number is the lowest free one. A download that receives no file (the target denies it or never answers, the operator cancels or aborts, the loader cannot run) leaves no directory behind; one that received anything keeps it. Each file's row carries its own status and progress; the abort button covers downloads. See [what the Download operation needs](#what-the-download-operation-needs-from-the-application). |
| Preferences | Persistent settings, in a two-column modal page from the header's Preferences button: the last media set directory (restored quietly at start-up), a default target address and `<THW_ID_POS>` (prefilled), the loader's control port (anything but 59 is sent to the target as the `port` option, which is what lets the loader and a target share one machine), the download directory, whether FIND runs by itself before an operation with no chosen target (one answer is taken automatically; none or several leaves the choice to the operator), whether a chosen media set is checked at once, the activity log's capacity, and the language. The dialog edits a working copy: Save writes it, Cancel and Esc leave the stored values as they were. |
| About | The `ADL` mark, what the window is for, and the build provenance: version, git describe, full commit, branch, build target, Qt version and a dirty-tree marker, generated at configure time; the licence and repository link. |
| The window's look | The shared design system in `src/ui` (taken from ICL Forge, a separate application whose code is not in this repository): `Theme.qml` is the single source of truth for colour (Signal light and dark), type, spacing and geometry; the shared controls (`AppButton`, `AppCheckBox`, `AppTextField`, `AppComboBox`, `AppSpinBox`, `SegmentedControl`, `FocusRing`), the header and footer, the chips, the log and the dialogs draw from it. The three applications compile the same source; see [the design system](design-system.md). |
| The first launch | A first-run statement before the workbench: what the window is for, one primary that opens the workbench, an optional route to choose a media set, and the defaults folded away. Passing it once is stored (its fields are the preferences), so every later launch goes straight to the workbench. |
| The language | All text goes through `qsTr`/`tr`, and the loader, the generator and the mock target offer the same eight languages (English, the source; French, Spanish, German, Yiddish, Hebrew, Simplified Chinese and Japanese) through one shared `adl::ui::LanguageManager` and its picker (the loader's is in Preferences). It installs the catalogues for the persisted language (the system locale, then English, on a first run), sets the layout direction and the typeface, and retranslates the running window, **including what the engine said earlier**: results, failures, log lines and the checks' details are `Message`s translated when they are read. Hebrew and Yiddish **mirror the whole window** (`LayoutMirroring` on the window root) and are set in the bundled Noto Sans Hebrew. Counts are real plurals (`%n`, with numerus forms). **Every message has a translation in every language, and all seven are machine-drafted first versions that no speaker has reviewed.** Not translated: the findings of the ARINC 665 library and the names of its errors, platform error codes, whatever the target sends, and the mock target's 615A engine log. [Translations](translations.md) has the details. |
| The target that was used | The address and `<THW_ID_POS>` an operation last started against are stored, and put back into the window at start-up, so a session starts where the last one left off. The Preferences defaults fill the fields on a first run, and again whenever the operator clears one. |
| The activity log | What the target said (its answer, each status file) and the engine's notices (a lost or repeated status file, a refused request, a checksum that did not match, a WAIT), with the time, oldest first. |
| The outcome | How the operation ended (the engine's `OutcomeCode`), and for an abort whether the target confirmed that it had stopped. Failures also open a dialog once, with the outcome, the detail and, where the application knows one, a hint (a host that could not start, an address that is not an address, a media set that could not be read, an abort the target did not confirm); a status bar carries the current state, the target and the media set. |

## The window

The window is one of three instruments on a shared design system (`src/ui`, described in
[the design system](design-system.md)). The workbench is a fixed context rail beside a work area:

- **The rail** is target (01) then media set (02). The target fields and the FIND results are
  always above the set, and the loads and their check boxes live inside the media set block. The
  rail scrolls itself; it is not where the action is.
- **The work area** has a fixed `Upload` / `Download` switch. `Upload` shows the plan, the three
  preflight checks (target, set, loads), the payload table of the ticked loads and the receipt of
  the last transfer. `Download` shows the mode and its consequence, the file list and the
  destination directory. The activity log and the target's configuration share a collapsible dock
  below it.
- **The action bar** is fixed at the bottom: the current state and progress on the left, then
  `Read information`, the one primary (`Upload` or `Download`) and `Abort` while a job runs.
- **The header** carries the `ADL` mark, the product and its scope, a readiness chip, Preferences
  and About. The footer carries the state, the target, the media set and the version.

A media set is read and checked when it is chosen (`Re-check` does it again); a set that reads is
valid, and its findings are warnings shown once under the set. The preflight repeats the target,
the set and the ticked count immediately above the action, and the payload table names each load
that will be sent. Closing during a job asks whether to keep working or stop and close; the second
choice asks the target to abort and waits for it.

## Layout

```text
src/app/
  main.cpp            creates the QGuiApplication, one LoaderModel, the QML engine and the language manager
  frame_profiler      the window's frame loop for a profiling build (docs/profiling.md)
  qml/                Main.qml (the shell), UploadPanel.qml, DownloadPanel.qml, ActivityDock.qml,
                      FirstRunScreen.qml, and the preferences, media set properties and target
                      properties dialogs (the About and Licences dialogs are in src/ui)
  translations/       the catalogues (adl_app_<code>.ts for the seven languages, adl_app_en.ts for the
                      English plural forms), compiled into the window's module; the shared
                      components' are in src/ui/translations. LanguageManager is src/ui's.
  core/               adl_app_core: everything but the window, Qt Core only
    engine_worker     the engine on its own thread, the abort and the deferred download choice
    loader_model      what the QML talks to: properties and a few invokable functions
    settings          the persistent preferences (QSettings), read at start-up
    models            the list models (the loads carry the operator's ticks and the progress)
    media_set_summary reading a media set from a directory, as plain values
    media_source      the files of an upload: an index of the media set's files, and a FileSource over it
    notice_text       an engine notice as a line for the operator
    events, address   plain values, and the IPv4 parser
    host_access       where the worker gets the platform from
tests/app/            QtTest, and the window loaded offscreen
tests/ui/             the window driven through Qt Quick Test (testing.md)
src/ui/               the shared design system: Theme, the controls, the header and footer, the
                      log, the dialogs, Archivo, the version and licence text and the window
                      geometry helper
```

`core` has no QML and no GUI, so it is tested headless. The QML is a view: it binds to `LoaderModel`'s
properties and the six models (`targets`, `events`, `configuration`, `loads`, `files` and
`downloads`) and calls its functions. The window gets the model as a required initial property
(`app`), so there is no global and no singleton. The only context properties are the language manager
(`languageManager`), the About text (`appVersionDetails`) and the licence notices
(`appLicenceNotices`), because they belong to the process rather than to the loader. The QML is a static library of its
own (`adl_app_qml`), linked whole into the application and into the window's test, so the test loads the
same compiled module the application does.

## The engine and the window: threads

The 615A runner is **blocking and single-threaded**: `Runner::run` does not return until the operation is
over, and `Runner::find` not until the window of three seconds has passed. So the window never calls it.

```text
   window's thread                           engine thread (QThread "engine")
   ---------------                           --------------------------------
   LoaderModel  --- queued call ---------->  EngineWorker::find / read_information / upload / download / ...
        ^                                         |  Runner::run(loader, op, {controller = &gate})
        |                                         |    the listener (JobListener) copies what
        +------ queued signals, in order ---------+    it is handed into Qt values and emits them
        |         log_line, targets_found,                
        |         configuration_read, finished           OperationGate::on_poll (every 100 ms)
        |                                                       ^
        +--- OperationGate::request_abort() (an atomic flag) --+
```

- **A job is one queued call and ends with exactly one `finished` signal**, whatever happened (the engine
  refused the request, the host could not be started, the address was not an address). While one runs
  `busy` is true and `LoaderModel` refuses another: there is one engine thread, and the operation owns the
  loader's control port. While one runs the ticks beside the loads cannot be changed.
- **Everything the engine reports is copied at once.** An `OperationListener` is handed views that are
  valid during the call only, so `JobListener` (one per job, for every operation) turns each into
  `QString`s and emits a signal.
  Qt delivers the signals to the window's thread in the order they were emitted, so the log reads in the
  order things happened.
- **The abort cannot be a queued call**, because the worker's thread is inside the runner, not in its
  event loop. It is an atomic flag (`OperationGate`), which is also the runner's `RunController`: the
  runner polls it (every 100 ms by default) and the first poll that sees it calls `DataLoader::abort` once,
  with `aborted_by_operator`. All four jobs that run an operation use it: Information, Upload and the two
  modes of Download (`LoaderModel::canAbort`); a FIND and the reading of a media set have no abort and end
  by themselves within seconds. The flag is cleared by `LoaderModel` before it queues a job, so an abort left
  over from an earlier job cannot stop the next. The target is told by the engine (an `ABORT` on every open
  transfer and in answer to the next status request); the operation ends when the target confirms or after
  the 13 second data load protocol time-out.
- **A deferred file choice** (an operator defined download) uses the same route: `OperationGate::on_poll`
  is where `DataLoader::provide_selection` is called from, with the choice posted by the window
  (`post_selection`) and taken from a flag in the same way. If an abort has been asked for, it wins.
- **Closing the window** while a job runs first asks (`An operation is running`: `Keep working` or
  `Abort and close`); the second choice aborts the job and waits for the engine's thread. For an operation
  the target has to confirm, that can be up to the 13 second time-out; a FIND, three seconds.
- **The platform's `Host` is made on the engine thread, once per job** (`system_host_factory`), and
  released at its end, so Winsock (on Windows) is started and stopped around each job and nothing is held
  between them. The factory is a constructor argument of `LoaderModel`, which is how the tests give it a
  host that runs in memory.

## What the Information operation needs from the application

`LoaderConfig` wants the target's endpoint, its `<THW_ID_POS>` and a workspace; the engine allocates
nothing, so the worker gives it a `std::vector<std::byte>` of `DataLoader::workspace_bytes` and keeps the
`DataLoader` on the heap. An Information operation has no `FileSource` or `FileSink`.

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

**During an upload the files come from the media set on disk through `adl::platform`'s file system,
not from memory.** While the media set is read (to show it), `MediaSetIndex` is made from its
`FILES.LUM`: every file's name, member, directory and 16-bit CRC, mapped to a path below the directory
the operator chose (member directory, then the file's own directory: `adl::media_tree`'s layout).
Nothing but the index is kept; during the upload the bytes are read at the offsets the target asks
for, when it asks. Choosing and checking a set is different: `read_media_set_tree` returns the bytes of
every file of the set, so the whole set is in memory while it is read, and a set has to fit in memory
to be chosen. `MediaTreeSource` is the `FileSource` over the index, and reads through the host's
`FileSystem`, which is what lets the tests run an upload against files in memory.

**Finding the file the target means.** A media set can hold several files of one name (in different
directories of a load, or on different members), and the target tells them apart by the checksum option
of its request (5.3.2.3.8.5), the CRC the load header gives that file. `MediaSetIndex::find` takes the
file of that name whose `FILES.LUM` CRC is the one asked for. Two decisions, both the application's
own choices and unchecked against a target:

- With **one** file of the name, it is served whatever CRC the request carries (and whatever kind of
  checksum). The header file's CRC as a target knows it may not be the one `FILES.LUM` records (the header
  ends in its own CRC), and the target checks what it received against what it asked for anyway.
- With **several** files of the name and a CRC none of them has, there is no such file (`not_found`, the
  target ends the operation); with several and no CRC in the request, the first one listed.

The engine's `DirectoryFileSource` (one flat directory) was not used because a media set is not one:
it has a directory per member and directories inside them.

**What was checked is what is uploaded.** The index is made when the media set is read, and kept by the
worker. At that moment every file's bytes are in memory (the read verifies them), so the index also takes a
*fingerprint* of each: its size and its SHA-1. A hash of the content, and not the file's size and
modification time, because the platform's `FileSystem` interface has no modification time, a time can be put
back or be too coarse to tell a second edit, and a changed byte always changes the hash; taking it costs
nothing extra at that point, since the bytes are there. The fingerprint is checked twice. When an upload
starts, before the target is contacted, the worker reads the whole set through the host's `FileSystem`
(file by file, in blocks of 64 KiB, so the set is not held in memory) and compares it (one more read
of the set, as long as the check's own); a difference ends the job at once
with "The media set changed after it was checked: DATA.LUP (member 1, \LOAD\)", the hint to check the media
set again, and the check no longer stands (the upload stays refused until it is run again). And each time the
target opens a file, `MediaTreeSource::open` compares that file again before a byte is served, so a file
edited while the transfer runs is refused too (the target is given an access violation, and the operator is
told the set changed, not that the target failed). A media set that cannot be read leaves no index, and
nothing to upload. An index made without the bytes (the tests that only map names to paths) carries no
fingerprints and verifies nothing.

**How the operation is judged.** `success` is the engine's `completed` with no load the target reported
as failed (`items_failed`); a completed operation with a failed load is shown as a failure ("completed,
1 load(s) failed"), and the detail says how many files the target read. A load that failed is named in
the log once, with the target's reason.

## What the Download operation needs from the application

A Download has no `FileSource`; it has a **`FileSink`** instead: `DirectoryFileSink`
(`adl::arinc615a_runner`) writes the target's files into one directory, under a temporary name
until the whole file has arrived, and never overwrites, which is exactly what 5.4.4.3 asks of
the `DNLD_DATA_<THW_ID_POS>_<number>` directory. The application makes that directory when the
download starts, under the download directory of the preferences (`download_directory.hpp`): the number is
the lowest free one from 1, and the parents are created on demand, through the platform's `FileSystem` (so
the tests' fake storage receives the bytes).

- **Nothing is made for an id that cannot be used.** The `<THW_ID_POS>` is checked before the first
  directory is made, in the model (`canDownload`) and again in the worker: the engine's own predicate
  (`is_valid_thw_id_pos`: 1 to 240 characters, no control character, slash or backslash) and, because the id
  becomes part of a directory name, that `DNLD_DATA_<id>_1` is a name every platform can make (a colon, which
  names a stream on NTFS, an asterisk or a trailing dot is refused). The model also treats an id the engine
  refuses as no target at all. When "Find the target automatically" is on and exactly one target
  answers the FIND, its id is checked the same way before the operation is started again
  (`run_pending_after_find`): one that cannot be used (for a download, one that cannot name the
  directory) stops the operation with a failure dialog and a log line, instead of another FIND or
  nothing at all.
- **A download that received nothing leaves no directory.** When the runner returns, whether the target
  denied the operation, never answered, the operator cancelled or aborted, or the loader could not run
  (the request refused, the port taken, the wait failing), the directory is removed if it is empty, and the
  summary then names no directory. A directory with anything in it, however little (a file, a part file,
  something another program put there), is kept and reported, and `remove_if_empty` never touches anything
  else. The download directory itself, which the operator chose, stays.
- **Two loaders may race for a number.** Looking for a free number and making the directory are two
  calls; if another copy takes the number in between, `make_directory` says `already_exists`, which is not
  an error here: the next number is tried (and a file system that says so for every number ends the search
  after 10 000 with a message, not a hang).
- **The names the target chooses** are checked by `DirectoryFileSink` with `adl::arinc615a::is_safe_file_name`,
  which is the predicate shared with the media tree, `adl::arinc665::is_portable_file_name`, plus two
  rules: at most 254 characters, and not ending in `.part` (the sink's own temporary name).
  `is_portable_file_name` is the same on every platform: no separator, no colon (an NTFS stream), no
  device name (`CON`, `NUL`, `COM1`, `CONIN$`... with or without an extension), no trailing dot or space,
  printable ASCII; it puts no limit on the length. The mock target adds the 254 characters itself (and
  accepts only letters, digits and `. _ -`). The media defined mode refuses such a name before the
  operation starts.

**The operator's choice is a deferred engine call.** For an operator defined download the
target writes its list of files (`<THW_ID_POS>.LNL`), and the standard leaves when the choice is
made to the loader. The listener copies the list into Qt values at once and defers; the window
shows the files all ticked; the operator ticks what to take and presses "Download selected"
(which the engine carries back as `.LNA`), or "Choose none" (which cancels the operation as
`SelectionDecision::cancelled` does: an abort with the target's confirmation). The choice
reaches the engine the same way the abort does: `OperationGate`, the one `RunController` the
runner polls, checks the abort first and then a posted selection, issues each at most once per
job, and is cleared before the next.

**In the media defined mode** the operator names the files, and each name is checked with the
same `is_safe_file_name` the sink uses before the operation starts, so a name that could reach
outside the download directory is refused before anything is sent.

**Progress and judgment.** The overall progress is the target's list ratio from the download
status files; each row's ratio is 100 once its file has arrived and 0 until then (the protocol
carries no per-file figure), with the target's status word and description. `success` is
`completed` with nothing failed and every requested file received; otherwise the failure names
the failed or missing files, with a hint. The detail says how many files arrived of how many
requested, and where.

## Where the settings are kept

The preferences, the language and each window's size and place are `QSettings` of the organisation
`iainchesworthlabs` and the program's name, so they live where the platform keeps such things (the registry
key `HKCU\Software\iainchesworthlabs\<name>` on Windows, a property list on macOS, a configuration file
on Linux). **`ADL_SETTINGS_DIR` moves them:** with the variable set to a directory, all three programs read
and write an ini file below that directory instead (`<directory>/iainchesworthlabs/<name>.ini`, whatever
the platform) and write nothing to the usual place. It is read once, at start-up, by
`adl::ui::redirect_settings_from_environment()` (`src/ui/settings_location.cpp`), which every `main` calls
straight after naming the application. The tests set it so that starting a program under test never
touches an operator's real settings or another run's, and it is also the way to keep two configurations
apart or to run a program from a stick.

The loader's keys (`src/app/core/src/settings.cpp`, `AppSettings`; each change is written at once) and
their defaults:

| Key | Default | What it holds |
| --- | --- | --- |
| `app/firstRunComplete` | false | the first-run screen has been passed |
| `mediaSet/lastDirectory` | empty | the media set folder last chosen, restored at start-up |
| `mediaSet/validateOnOpen` | true | whether a chosen set is checked at once |
| `target/defaultAddress`, `target/defaultThwIdPos` | empty | the defaults that prefill the target fields |
| `target/lastAddress`, `target/lastThwIdPos` | empty | the target the last operation started against, put back at start-up |
| `target/autoFindBeforeOperation` | false | whether FIND runs by itself before an operation with no chosen target |
| `loader/controlPort` | 59 | the loader's control port; a stored value outside 1 to 65535 is ignored |
| `download/directory` | `<Documents>/ArincDownloads` (the home folder stands in when the platform gives no Documents folder) | the download directory |
| `log/capacity` | 2000 | the most lines the activity log keeps; a stored value outside 100 to 1,000,000 is ignored |

Two keys are not the loader's own: `language/code` (written by `adl::ui::LanguageManager`; with none
stored, the system's language if it is one of the eight, otherwise English) and `window/x`,
`window/y`, `window/width`, `window/height`, `window/maximized` and `window/placed`
(`adl::ui::install_window_state`, written when the window closes and when the application quits).
The generator and the mock target store only these two groups.

`--smoke-test` makes a program draw its window's first frame, print `smoke-test: first frame drawn` and
exit with code 0 (`adl::ui::install_smoke_test`, `src/ui/smoke_test.cpp`). It is what the system tests start
the programs with ([testing](testing.md)); a program that cannot create its window exits with an error or
never gets as far as the frame.

## Qt in this repository

- **Qt is the prebuilt kit, not vcpkg.** `cmake/FindQt6.cmake` finds it through `QT_ROOT_DIR` (set it
  to the kit's directory, see [building](building.md#building-the-application); CI sets it too) or
  the aqtinstall layout each platform fragment lists.
- **`ADL_BUILD_APP` is on by default on a desktop where the Qt kit is found**, and off where it is not,
  so the libraries still build on a machine without Qt; `-DADL_BUILD_APP=ON` makes a missing kit an
  error and `-DADL_BUILD_APP=OFF` is the deliberate libraries-only build ([building](building.md#building-the-application)).
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
while a job is queued), a media defined Download whose files land byte for byte in the
`DNLD_DATA_<THW_ID_POS>_<number>` directory of the fake storage, an operator defined Download with a
ticked subset and with "Choose none" (the abort with the target's confirmation), an abort in the middle of
an Upload and of a Download, both in the model and by the window's button (the engine held inside a file
call, the target's confirmation and the error dialog asserted), a download with a failed
item, an unsafe file name refused before anything is sent, the directory handling (`download_directory_test`:
the numbering, the race for a number, each failing call of the file system, the removal of an empty
directory and only of that) and the worker's jobs called directly with a host made to fail
(`engine_worker_test`: an id no directory can be named after, a denied or silent target, the request refused,
the port taken, the wait failing, each leaving no directory), a set edited after it was checked refused at
the start of an upload and as the target opens the file, and the gate (a choice posted once, cleared per job,
the abort winning over a choice, a choice made before the loader awaits it kept for the next poll), `MediaTreeSource` and the index (two files of one name told
apart by the CRC, the fall-backs above, offsets, the limit on open files), the models (ticks, locking,
progress, and the files of `FILES.LUM`), the address parser and the notice texts; the preferences
(round-trip through `QSettings`, the control port reaching the engine as the `port` option, FIND before an
operation with none, several or one target answering, the quiet start-up restore); the validation gate (an
unreadable set — a corrupted byte, a removed file — leaves the upload refused with the reason, a set that
reads is uploadable, findings warn but do not block, a re-check catches files changed on disk, the
operator's ticks survive a re-read); the failure hints; and the window itself, loaded offscreen with every
QML warning an error, driven through a FIND, an Information operation, a media set check, an Upload and a
media defined Download (the load ticked and the buttons pressed through the window's own items), in the
light and the dark scheme. Also checked: the first-run screen hands over to the workbench only once (a
stored preference); the last operation's target comes back in a new model over the same settings; and the
language manager starts in English, then installs each of the seven catalogues in turn, retranslates the
running window and the model's phrases (an engine outcome is said in the new language), sets the layout
direction and the typeface the script asks for, and that Qt calls exactly Hebrew and Yiddish right to left;
the right-to-left window itself is `tst_right_to_left.qml` (the rail, the header, a segmented control,
a check box, a spin box and a progress bar mirror), and `tests/unit/translations_test.cpp` and the
`translations.current.*` tests hold the catalogues to the sources ([Translations](translations.md)).

Not checked: an Upload against a real target (and so which checksum a real target sends for each file,
see above), an Upload whose files are in two directories of one name end to end (the source is tested
on that, the operation is not), a set larger than memory (choosing and checking a set reads all of it into
memory; only the transfer reads by block; the test sets are a few kilobytes), the window
against a real target over a real network (the runner over real sockets is run by the loopback
integration test against the mock target, [mock target](mock-target.md), not by the window); the window
at scales other than 100% (it has been driven on a real Windows desktop in both schemes by a script, see
[the screenshots](screenshots.md), against the mock target on the same machine: FIND finds nothing there,
the operations work); keyboard use and a screen
reader beyond the accessible names the controls carry; the translations' wording (the seven catalogues are
a machine-drafted first version: the tests prove that every message has a translation of the right shape, that a catalogue
is installed and the window retranslated and mirrored, not that a speaker would accept the text); and the first-run screen's download-directory picker
(it is a Qt `FolderDialog`, the media set's own picker is the tested one).
