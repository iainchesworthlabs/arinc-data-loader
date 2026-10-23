# Testing

The suite has three tiers, every one of them labelled in CTest:

| Tier | Label | What it is |
| --- | --- | --- |
| Unit | `unit` | One piece of code against its contract: the ARINC 665 library, the 615A codec, the media tree, the samples, the platform types and the fake, the application's models, settings and plain functions, and the media set generator's and mock target's cores |
| Integration | `integration` | Pieces together: the 615A engine and runner against the simulated target on the fake platform and over real loopback sockets, the platform conformance suite against the real host, and the mock target's engine against the loader's engine |
| System | `system` | What a user runs: the three application programs started as processes (each must draw its window and exit 0), the windows loaded offscreen and driven through Qt Quick Test, the payload writer, and the build checks. The whole-operation tests carry both `integration` and `system` |

The tests are Catch2 and QtTest/Qt Quick Test programs, plus checks about the
build itself and runs of the `adl_write_payloads` program. They run in CI on
Windows (MSVC), Linux (GCC) and macOS (AppleClang); locally they have been run on
Windows, and on Linux in a container.

## Running them

After a build (see [Building](building.md)):

```sh
ctest --preset windows-msvc-debug              # or linux-gcc-debug, macos-clang-debug
ctest --preset windows-msvc-debug -R platform  # names containing "platform": the build checks and a few platform tests
ctest --preset windows-msvc-debug -R tools     # just the payload writer
ctest --preset windows-msvc-debug -R docs      # the link and path checks of the Markdown files
ctest --preset windows-msvc-debug -L unit      # one tier
ctest --preset windows-msvc-debug -L integration
ctest --preset windows-msvc-debug -L system
ctest --preset windows-msvc-debug -L ui        # the tests that load or drive a window
```

The `ui` label is a second label, beside the tier: the `ui.*` tests, `app.qml`, `media_gen.qml` and
`mock.qml` carry it. They need the Qt kit.

The same three tiers have build targets:

```sh
cmake --build --preset windows-msvc-debug --target check-unit
cmake --build --preset windows-msvc-debug --target check-integration
cmake --build --preset windows-msvc-debug --target check-system
```

Every Catch2 `TEST_CASE` is registered with CTest by name
(`catch_discover_tests`), so a failing case is named in the CTest output, and
`ctest --preset <preset>` shows the output of any that fail. To run the
unit-test program directly, with Catch2's own filters:

```sh
build/windows-msvc-debug/bin/adl_unit_tests --list-tests
build/windows-msvc-debug/bin/adl_unit_tests "[golden]"
build/windows-msvc-debug/bin/adl_platform_tests "[conformance]"
build/windows-msvc-debug/bin/adl_engine_tests "[operation]"
```

The main test programs are `adl_unit_tests` (the ARINC 665 library, the 615A codec, the media
tree and the samples), `adl_platform_tests` (the platform layer: its portable types, the fake
platform and the conformance suite) and `adl_engine_tests` (the 615A engine, runner and the whole
operations against a simulated target on the fake platform). With `ADL_BUILD_APP=ON` they are
joined by `adl_app_tests` and `adl_ui_tests` (the application's logic and its window), `adl_window_state_tests`,
`adl_mock_tests`, `adl_mock_model_test`, `adl_gen_tests`, the three offscreen QML programs, `adl_loopback_test`
and the system smoke tests in `tests/system`.

Two things a confined shell (a container where only the source tree is writable, or Windows'
sandbox) trips over, neither of them a test failure:

- Catch2 discovers every test by running the test program as it links, writing the listing to
  `%TEMP%` (or `$TMPDIR`). If writes outside the tree are refused, point TMP and TEMP at a
  directory inside the build tree before configuring; otherwise the build stops with "Error
  listing tests from executable" and no output.
- `platform.selector` configures and builds a probe with the MSVC toolchain, which needs
  `rc.exe` on PATH: run `ctest` from a Visual Studio developer shell.

The window's tests run offscreen (no desktop needed): the test's environment sets
`QT_QPA_PLATFORM=offscreen` and points at the Qt kit's plugin directory.

The tags are `[crc]`, `[digest]`, `[integrity]`, `[check-value]`, `[part-number]`,
`[naming]`, `[header]`, `[load]`, `[batch]`, `[media-files]`, `[media-set]`,
`[label]`, `[media-tree]`, `[portable-name]`, `[golden]`, `[compatibility]`, `[limits]`, `[robustness]` and
`[platform]`; `public_api_test.cpp` adds `[to-string]`, `[generic]` and `[listener]`. The 615A codec adds `[tftp]` (with `[options]`, `[wait]` and `[abort]`),
`[find]`, `[protocol-files]` (with `[status]`, `[text]` and `[names]`), `[golden615a]` and
`[robustness615a]`. The platform layer adds `[memory]`, `[error]`, `[time]`, `[network]`,
`[fake]`, `[conformance]` (with `[clock]`, `[files]`, `[threads]`, `[ipc]`,
`[network]`), `[integration]` (real UDP over the loopback interface) and `[real]` (runs
on the real host only). The 615A engine adds `[engine]`, with `[tftp-transfer]` (and
`[wrap]`, `[sas]`, `[faults]`, `[timeout]`, `[wait]`, `[abort]`, `[options]`, `[tsize]`, `[checksum]`,
`[receive]`, `[send]`, `[io]`), `[loader]` (the data loader driven by hand, with `[status]`,
`[version]`), `[operation]` (the whole operations, with `[information]`, `[upload]`, `[download]`,
`[operator]`, `[init]`, `[port]`, `[status]`, `[timeout]`, `[abort]`, `[version]`, `[faults]`),
`[runner]`, `[find]` and `[files]`.

## Coverage

The project minimum is **90% of lines and 80% of branches of the portable code**. The
`linux-gcc-coverage` preset instruments the code, and its `coverage` target runs
the suite and then enforces both figures with gcovr:

```sh
pip install -r .github/requirements/coverage.txt
cmake --preset linux-gcc-coverage
cmake --build --preset linux-gcc-coverage --target coverage
```

It needs gcovr (without it the configure warns and there is no `coverage` target) and, for the second
pass below, the Qt kit: that pass fails when the applications were not built. CI configures with
`-DADL_BUILD_APP=ON`, which turns a kit that cannot be found into an error at configure time.

The report is written to `build/linux-gcc-coverage/coverage`: `index.html` (one
page per source file), `summary.json` and `cobertura.xml`. The CI coverage job
runs the same script (`cmake/CoverageReport.cmake`) and fails the job when either
figure is below the minimum. `--exclude-throw-branches` and
`--exclude-unreachable-branches` leave out branches no test can take: a throwing
branch and a line the compiler proves unreachable.

The gate measures the portable code:
`libs/arinc665`, `libs/arinc615a`, `libs/arinc615a_mini`, `libs/arinc615a_runner`, `libs/media_tree`,
`libs/samples`, `apps/write_payloads` and `libs/platform/interfaces` with `libs/platform/detail`.
The OS fragments under `libs/platform` are left out because each host builds a different set (the
same gate has to mean the same thing on all three) and the conformance suite is what holds them to
the platform contract. QML cannot be measured by gcovr, which is why the gate is the libraries' and
not a blended figure.

**A second pass measures the applications' window-free logic**: `apps/app/core`,
`apps/mock_target/core` and `apps/media_gen/core` (the models, the engine's worker, the settings, the
host access, and the generator's and the mock target's cores), with Qt's generated files (`moc_*`,
`mocs_compilation`, `*_autogen`) left out. These are plain C++ with Qt types, driven by `app.tests`,
the UI suite and the generator and mock target tests, and they were outside the gate for no better
reason than that they need the Qt kit. The floor is lower than the libraries' (80% of lines, 66% of branches) and sits a little under what the suite
measured in CI when it was set (83.9% of 4199 lines, 70.3% of 4127 branches); raise it whenever the measure rises. The same script runs
the pass (`cmake/CoverageReport.cmake`, `-DAPP_LINE_THRESHOLD=` and `-DAPP_BRANCH_THRESHOLD=`), its
report is in `build/linux-gcc-coverage/coverage/apps`, and the pass **fails when it measures too
little** (fewer than 1000 lines or 300 branches: a coverage build configured without the
applications, tests that did not run, a filter that matched nothing), because gcovr reports 0 of 0
as 100%. The CI coverage job installs the Qt kit and configures with `-DADL_BUILD_APP=ON` for that
reason; the first pass is unaffected by it.

## What there is

| Test file | What it holds down |
| --- | --- |
| `crc_test.cpp` | The CRCs against the check values of ARINC 665-3 section 4.3 (also as `static_assert`), the Appendix L reference files whose content the standard describes, the usual `"123456789"` results, feeding data in pieces, and that a CRC appended to its data makes the CRC of the whole zero |
| `digest_test.cpp` | MD5 against RFC 1321's test suite and SHA-1 against FIPS 180's, a million `a`, every padding boundary, reading a digest mid-stream |
| `integrity_checker_test.cpp`, `check_value_type_test.cpp` | The five check values in the form section 5.1 stores them, Table 5.1's values, titles and lengths, types that cannot be calculated |
| `part_number_test.cpp` | Check characters (Appendix E's example), building a part number, the checks of sections 2.1.1 and 3.1 |
| `naming_test.cpp` | File names, extensions and reserved extensions (Table 3.2.2-1), pathnames, target hardware IDs |
| `header_file_test.cpp` | The layout of Figure C-1 (the fixed part, the pointers, padding, relative pointers), round trips, each way a header file can be damaged, reading a later format version through its pointers, the recommendations |
| `load_test.cpp` | Building a load, every check value type for the Load Check Value, verifying a load and each way it can fail |
| `batch_file_test.cpp` | The batch file layout, round trips, damage, the recommendations |
| `media_files_test.cpp` | The three lists: layout, round trips with and without user data, sequence numbers, damage, the recommendations |
| `media_set_test.cpp` | Building and reading media sets across members, the lists on each member, FILES.LUM's entries, partial reads, damage of every file, members of different sets, files of one name told apart by directory and CRC (section 3.2.4.2), findings |
| `portable_name_test.cpp` | The one file name predicate the sink, the media tree, the application and the mock target share: a table of hostile names (separators, colons and streams, devices in any case and with extensions, trailing dots and spaces, wildcards, control characters, non-ASCII, a name with a NUL in it) refused the same on every platform |
| `media_tree_test.cpp` | Every sample media set through a directory tree and back (and the set read from the disk verifies like the one in memory), the backslash pathnames as native paths, empty files and deep directories, binary content, names that cannot be files on every platform (device names, reserved characters, trailing dots, non-ASCII), sequence numbers outside 1 to 255, duplicate names whatever their case, nothing written when anything is invalid, never writing over a non-empty member, a stray entry or a link in a tree being refused, and damage on the disk being found when the set is read. The temporary directories come from `tests/support/temp_dir.hpp`. The link case makes a symbolic link, and where the account may not (an ordinary Windows account) a directory junction with `mklink /J`, which any account may make, so it runs on all three desktops and is skipped only when neither can be made |
| `media_label_test.cpp` | The label text, its date format, the required items |
| `golden_test.cpp` | The library's header file, batch file and list files against files written independently ([below](#golden-payloads)) |
| `compatibility_test.cpp` | The ARINC 615 and Boeing 777 layouts of Appendix F (built by `adl::samples`, which `adl_write_payloads` writes out) |
| `limits_test.cpp` | Each side of the 16-bit field limits, a load of a thousand files, a 3 MB file, a media set of many loads |
| `robustness_test.cpp` | Every decoder over every truncation, every single-bit change, damage with the CRC put right, and random bytes, under the sanitizers |
| `tftp_test.cpp` | The TFTP packets of ARINC 615A-3 section 5.3.2: the layout of each, octet mode in any case and nothing else, block numbers that wrap 65535 to 1, a WRQ acknowledged with block 0, the largest block (65464), error and option-acknowledgement packets, an encoder that never writes past a buffer too small for it |
| `tftp_options_test.cpp` | `blksize`, `tsize`, `timeout`, `port` and `checksum_<n>` with each side of their limits and every malformed value, names in any case, unknown options ignored, repeats refused, the option builder, and the WAIT and ABORT messages: the standard's own `WAIT:50`, all 65536 wait times and all 65536 status codes round-tripped |
| `find_test.cpp` | The FIND request and answer of Attachment 3 (the example's layout, 0x10 and NUL terminators, each field at its limit and one over, empty fields, the manufacturer code of three) |
| `protocol_files_test.cpp` | The twelve protocol files of section 6.4: exact layouts, round trips, the file length and version, the status codes each field may hold, the text length rule and its padding, the ratios, counts, user defined data, nested lists, and a buffer too small for every file |
| `arinc615a_golden_test.cpp` | The codec's packets and files against the ones written independently ([below](#golden-payloads)) |
| `arinc615a_robustness_test.cpp` | Every 615A decoder over every truncation, bit and byte change, several changes at once, inserted and removed bytes, random bytes and random bytes behind a valid beginning, under the sanitizers; whatever is accepted must encode back to bytes that decode to the same thing |
| `public_api_test.cpp` | The small public vocabulary both libraries share: every `to_string` for the 615A and 665 errors, status codes, notices, transfer outcomes, checksum verdicts and check value types, the Table 5.1 lengths, which status fields may carry each code, the engine clock's saturating arithmetic, the generic CRC fed whole and in pieces, and the default `OperationListener` (every callback is harmless and a download choice with no override is cancelled) |
| `tests/engine/tftp_transfer_test.cpp` | `TftpTransfer` on its own: two of them over a link in memory (a file read and written at 12 sizes and several block sizes, the largest block, block numbers across the 65535 to 1 wrap in both directions with 65 541 blocks, every datagram duplicated and the sender's block counted (the Sorcerer's Apprentice fix), 120 runs with loss, duplication and reordering from fixed seeds that either deliver the file exactly or time out, reordering by up to 900 ms, the two second time-out and one retry to the datagram, lost ACKs made up for by the retry and three losses not), and one transfer at a time with every datagram written out (WAIT as an answer to a request and as an ordinary error in the middle, ABORT from the peer with each code and our own, error packets, the options of our request and of the OACK we send and accept, an OACK that breaks the rules refused with error 8, a size that is not what arrives, the checksum verdict for each algorithm and a wrong one that does not fail the transfer, repeated and stray and garbage datagrams, a block too long, the dally, repeated requests, a full disk, a file that cannot be read, a request that does not fit its buffer) |
| `tests/engine/operations_test.cpp` | `DataLoader` with every datagram written out: the request for each operation's initialization file and its options, requests that cannot be made, the time-outs to the nanosecond (2 s and one retry, the DLP retry, 13 s from the end of the initialization file and from the end of each status file, an exception timer longer and shorter than the time-out), requests refused with the right error (before the operation is under way, another operation's file, no slot, from another host), a repeated request, a newer request replacing an old one, status counters (repeats, out of order, gaps, the wrap), an invalid status file, the abort and its confirmation, a protocol version that is not A4 |
| `tests/engine/data_loader_test.cpp` | Whole operations through the runner on the fake platform against the simulated target: an Information with and without status files, an Upload of a media set built with `build_media_set` (also 150 000 bytes in 64 byte blocks, a short load, two files of one name told apart by the checksum option, a file not yet available, a file refused), media defined and operator defined Downloads (a wrong checksum, no checksum, a file nobody asked for, a missing file, a sink that fails, the choice made later or not at all), a denied or unsupported operation, WAIT, a silent target, the port option, status files lost and repeated, 13 s of silence, the exception timer, the operator's abort in an upload and a download, a target's abort, a protocol version that is not A4, 24 seeds each of an upload and a download over a network that loses, repeats and reorders, a network that loses 45% (it must end, not hang), and that one seed is one history |
| `tests/engine/find_client_test.cpp` | `FindClient` (the request bytes, the three second window to the nanosecond, what is ignored, repeats, a concentrator's second identity, 64 targets) and `Runner::find` on the fake network (several targets, one that answers twice, one too late, addresses given for AFDX, a lossy network, a port that is taken) |
| `tests/engine/directory_files_test.cpp` | `DirectoryFileSource` and `DirectoryFileSink` on the fake file system: names that are safe, reading in blocks, names refused (paths, `..`, separators), the limit on open files, a file that appears only when complete, nothing left for a failed transfer, never overwriting, a full disk |
| `tests/engine/sim_target.cpp`, `sim_target.hpp`, `rig.hpp`, `transfer_pair.hpp` | Not tests: the simulated target, which uses `adl::arinc615a_mini` (`libs/arinc615a_mini/src/mini_tftp.cpp`), its own TFTP that shares no code with the engine, and the helpers the tests above use ([below](#the-simulated-target)) |
| `paths_test.cpp` | `adl::platform::executable_path` names the running test program |
| `memory_test.cpp` | `Arena`, `BlockPool` and `StaticStorage`: alignment, exhaustion as a null result and never a write past the end, marks and rewinding, blocks that do not overlap and come back in any order, a block that is not the pool's being refused |
| `platform_types_test.cpp` | Error codes and their names, `Instant` arithmetic that saturates instead of wrapping, the protocol's largest figures fitting a `Duration`, IPv4 addresses read and written (and the bad ones refused) |
| `fake_platform_test.cpp` | The fake itself: the clock moves only when told, events run in time order, a latency, a loss profile that is the same for the same seed, a filter that drops, duplicates and corrupts chosen datagrams, jitter that reorders and loses none, an agent standing in for a target, two hosts on one network, broadcast, a receive buffer that overflows, the in-memory file system, a memory budget, threads that run when joined |
| `tests/conformance/*` | The platform contract, each check run against the fake **and** the real host this build selected. Clock: never backwards, sleeps at least what was asked and not wildly more, a wall clock that is plausible or `unsupported`. Memory: zeroed, aligned, writable, independent, refused with the right reason, `resident` granted or refused usefully. Files: a file written in pieces is read back at any offset (a short count only at the end), a file across the largest TFTP block size (65464), exclusive creation, directories, listing (and changing the directory while listing), rename over a file, a file that outlives its removal, paths the interface refuses (empty, backslash, NUL), non-ASCII names, a full disk, a closed file. Threads: join, destroying a thread waits for it, every priority level starts a thread and `normal` is always applied, a mutex, a semaphore's count, ceiling and time-out. Queues: whole messages in order, a full queue, a ring that wraps, bad sizes, caller-supplied storage. Network: a datagram of each TFTP-relevant size arrives whole with its source, order, time-out, a second bind refused, a port shared on request, an address that is not the host's, waiting on several sockets, truncation, the maximum datagram, a send to a closed port not upsetting the sender, broadcast needing permission. Real host only: threads contending on a mutex, a queue between threads under backpressure, a thousand datagrams echoed over real UDP, one thread serving five sockets with `wait_readable`, a datagram arriving from another thread while a receive waits |
| `apps/app/tests/address_test.cpp`, `notice_text_test.cpp`, `models_test.cpp` | The application's plain parts (QtTest, `app.tests`): the IPv4 parser (four decimal octets and nothing else: no blanks, signs, hexadecimal, short forms), the target's `<THW_ID_POS>`, each engine notice said with its unit (every enumerator of `Notice`, each with a sentence of its own), and the four list models (roles, the log keeping the newest 2000 lines, the log's role numbers that the shared `LogView` reads, the loads' ticks and their locking while a job runs, and an upload's progress matched to the loads) |
| `apps/app/tests/loader_model_test.cpp` | Whole jobs through `LoaderModel` on its engine thread, against the simulated target on the fake platform: an Information operation (the result, the hardware and parts, the log in order, one request seen by the target), FIND (a target listed, then none), FIND run by itself before an operation (one answer taken, none or several starting nothing, and one answer whose `THW_ID_POS` cannot be used stopping with a message, not with another FIND), choosing a target, an abort before the target has accepted (made certain by holding the engine thread at the start of the job) and the next job running, a second job refused while one runs, a host that cannot start, a media set read from a directory and one that is not there, and the model destroyed while a job is queued; a target changed after a FIND and an Information (`reconcile_target`: the selection, the configuration and the receipt go, the log stays, a media set result is not a receipt and survives), `resultIsTransfer` and `resultWasDownload` for every kind of job (including a failed download); the application's own `system_host_factory` on the real host, with a download directory whose name is not ASCII (nothing answers on the loopback, the operation is stopped once the numbered directory exists on the real disk); the Upload operation: a media set written as a tree and read back, a load ticked and uploaded (the files the target read, the progress only going forward to 100, the log, the ticks locked meanwhile), what an upload needs before it can start (a media set, a tick, a valid target), a file the media set does not have (the target ends the operation), an abort before the target has accepted, and **an abort in the middle of an Upload and of a Download**: the target is slow (`work_time`), the engine's thread is held inside the file call that begins the second file ([below](#holding-a-transfer-half-way)), the model is aborted, and the target's confirmation (`The target confirmed that it stopped.`), the partial `files_read` / `files_written` and the progress already reported are asserted |
| `apps/app/tests/loader_model_test.cpp` (`MessageTest`, `a_language_switch_reaches_...`) | `Message`: verbatim text, placeholders replaced in one pass, a sequence, a count, and that the text follows the installed translator; and that a language switch (the French catalogue installed while a result, the log and a failure are on show) is read again by the model after `retranslate()`. |
| `apps/app/tests/media_source_test.cpp` | The index of a media set's files and the `FileSource` over it, on files in the fake platform's memory: paths below the member and directory, a pathname a tree cannot hold, two files of one name told apart by the CRC in the request, the fall-backs (one file of a name is served whatever the CRC; several and no fitting CRC is not found; several and no CRC is the first), a file in the lists and not on disk, reads at offsets and at the end, and the limit of open files |
| `apps/app/tests/controls_test.cpp`, `download_directory_test.cpp`, `engine_worker_test.cpp`, `settings_test.cpp` | More of the application's logic (QtTest, `app.tests`). The runner's controller: the operator's abort and the deferred file choice of an operator defined download are posted on the window's thread and read by the runner's, one `clear` forgets both, and a choice the loader is not yet waiting for is kept for the next poll. The download directory `DNLD_DATA_<THW_ID_POS>_<number>`: its name, the ids a directory cannot be named after, a chain of directories made and an existing one left alone, the lowest free number, a number taken between looking and making, why a directory could not be made, and that only an empty directory is removed. The engine worker's jobs called on one thread: a download the target denies, a silent target, a request the loader refuses, a port that cannot be opened and a network wait that fails each leave no download directory, and one that received a file keeps its directory. The settings: the defaults, what is written read back, numbers out of range kept as they were, the first run complete only once, and the last target remembered apart from the default one |
| `apps/app/tests/qml_test.cpp` (`app.qml`) | The window: `Main.qml` loaded offscreen with the real model, a FIND and an Information operation run through it against the simulated target, then a media set read, its load ticked through the window's own check box and an Upload started from the window's button, and any QML warning (a binding to a name that is not there, a property that cannot be overridden) failing the test. It can save pictures of the window: `adl_app_qml_test <workbench.png> <first-run.png> <right-to-left.png> <preferences.png> <about.png>` (each optional, in that order; `ADL_DARK=1` selects the dark scheme); the offscreen platform on Windows has no fonts unless `QT_QPA_FONTDIR` points at some |
| `tests/ui/qml/tst_main_window.qml` (`ui.tst_main_window`) | The loader window through Qt Quick Test: the real `Main.qml` and `LoaderModel`, offscreen, with Qt Quick Test's own mouse and key events on the window's items. Typing a target, FIND and clicking the target it found, reading the information into the configuration list, choosing and re-checking a media set, ticking its loads through the check box and the All/None buttons, an upload started from the Upload button and checked against what the target read, both download modes (a media defined name, and the operator defined choice with its confirm button, unticked file by file through `downloadChoiceCheck`, and "Choose none" cancelling the download), a refused operation opening the error dialog and its OK dismissing it, **Abort in the middle of an upload and of a download** (the engine held half way through the transfer; the progress bar is shown, Abort cannot be pressed twice, the target confirms the stop, part of the load is across, and the error dialog is open - asserted, not "if it is visible"), and Clear emptying the activity log. `tst_dialogs.qml` (`ui.tst_dialogs`) opens Preferences, the media set and target properties and About, types into their fields and closes them through their standard buttons. **Preferences is transactional, and the tests say so**: every field is changed and, before Save, the model and the settings file still hold the old values; Save then writes all of them to both (read back through a second `LoaderModel` over the same ini); Cancel and Esc leave the model and the file as they were, and reopening the dialog shows the stored values, not the abandoned edit. `test_05` opens About and its Licences page. `tst_first_run.qml` (`ui.tst_first_run`) starts on a settings file with nothing in it: the first-run screen stands in front of the workbench, the defaults are typed into its own fields (address, `THW_ID_POS`, download directory), Start loading hands over, the workbench starts from them, and a new model over the same file finds the first run done and the same defaults. `tst_window_close.qml` (`ui.tst_window_close`) closes the window: idle, it closes without asking; with a job running it asks, "Keep working" leaves job and window alone, "Abort and close" aborts, waits for the job to end and then closes, and - when the job ended while the question was open, so no change of state is left to close the window - closes at once without aborting anything |
| `tests/ui/qml/tst_right_to_left.qml` (`ui.tst_right_to_left`) | The right-to-left languages, switched through the real `LanguageManager`: in Hebrew and Yiddish the rail is on the right, the header actions and the segments swap, a check box keeps its box on the reading side of its label with its detail indented from the right, a spin box has its steppers on the left, a progress bar fills from the right, and English puts the window back; and which of the eight languages are right to left is the manager's answer. |
| `tests/ui/qml/tst_keyboard.qml` (`ui.tst_keyboard`) | Principle 9 for the dialogs, on the real window: where the focus starts in Preferences (its first field, then Tab through the fields), About, the stop-and-close question (on "Keep working", with Return not stopping anything) and the error dialog, Enter saving and Esc leaving the stored values, Esc acknowledging a failure as OK does, and the focus going back to the control that opened the dialog |
| `tests/ui/qml/tst_navigation.qml` (`ui.tst_navigation`) | The loader's lists and groups by keyboard: three FIND answers (shown without a FIND) as one tab stop with Up/Down/Home/End/Enter and a 2 px marked choice, the dock tabs as one control with arrow keys that wrap and cannot be unchecked by a click, the activity log scrolling by Page/Home/End and copying with Ctrl+C (and its description naming the platform's own copy key), the configuration list as a tab stop, the header chip and the status bar agreeing after a failure, and the next-step banner's title matching its button |
| `tests/ui/qml/tst_accessibility.qml` (`ui.tst_accessibility`) | The 2 px ring on a field and a spin box, the 24 px steppers and check box rows, accessible names that contain the visible text (and the spin box editor's name), and a field set from outside (UI Automation, voice control) reaching the model |
| `tests/ui/qml/tst_layout.qml` (`ui.tst_layout`) | The window at its 1000×620 minimum: the key controls inside the window, the dock folded and the plan at least 340 px, an open dock 150-260 px, a visible scroll bar on the overflowing upload and download pages, and check tile details that wrap rather than elide |
| `tests/ui/qml/tst_tones.qml` (`ui.tst_tones`) | The status vocabulary: one glyph per tone, warn and fail not sharing an ink or surface, the log's four severity words, a chip's glyph, fill and border per tone, chip text at 11 px or more, the contrast of every tone's ink, the selection border, the progress fill, the disabled label and the focus ring in both schemes, reduced motion, and `Theme.fileUrl` |
| `tests/system/smoke_app.cmake` (`system.*.starts`) | The built programs as processes: `adl_app`, `adl_media_gen` and `adl_mock_target` each start offscreen with `--smoke-test` and **must draw their window's first frame, print `smoke-test: first frame drawn` and exit 0** (an application that cannot load its QML or find a Qt plugin exits with an error, and one that never shows its window runs into the time-out - both fail). Their settings are redirected with `ADL_SETTINGS_DIR` to a directory under the build tree, and the test requires that the program wrote its ini file there, which proves the redirection took effect and the real registry key or configuration file was not used |
| `tests/ui/window_state_test.cpp` (`ui.window_state`) | The window geometry the three programs share (`libs/ui/window_state.cpp`), on a bare window on the offscreen platform: the default size and place when nothing is stored, a stored size and position coming back, a size below the minimum raised, a size larger than the screen allows brought inside it (less 48 and 80 pixels), a position on no screen ignored (and one that was never saved not used), a maximised window opening maximised, and what is written when the window closes (the maximised rectangle is not remembered as its size) |
| `tests/unit/translations_test.cpp` and `translations.current.*` | The Qt Linguist catalogues of the three programs and the shared interface, read as files (so on every leg, Qt or not): no message unfinished or vanished, none empty, placeholders, plural forms, markup and identifiers kept, no sentence left in English, the same messages in all seven translations (the eight languages are English and these), the English plural catalogues complete. The `translations.current.*` tests (`cmake/CheckTranslations.cmake`) run lupdate over a copy of the catalogues and fail when they no longer match the sources, ignoring `<location>` lines. They check shape, not whether the translations are good ([translations](translations.md)). |
| `apps/app/tests/window_state_test.cpp` (`app.tests`) | The placement rule of `libs/ui/window_state.cpp` as plain functions: a stored coordinate read as signed even when the Windows registry kept it as an unsigned DWORD (a window on a monitor left of the primary one), and a title bar that must be reachable (96 by 24 px) on some screen's available area rather than one pixel of overlap |
| `apps/media_gen/tests/generator_test.cpp` (`media_gen.tests`) | The generator's model and worker (QtTest): the header file name following the load part number, a generated media set that verifies and that the loader's own reader (`read_media_set_summary`) accepts, a form that is refused (an invalid part number, a file name the naming rules refuse, a file, header or batch name that the standard allows and the folder tree's portable name rule refuses, duplicate names, no data files), a change of language keeping the outcome of the run, the Check value column counted by the `NO CHECK VALUES` chip, data files placed round-robin over the members, the batch file when asked for, a cancel before anything is written (nothing is written), after the tree is written and after the set verified (what was written is removed, and the operator is told), a write that fails leaving no partial set, an allocation that fails ending the job cleanly, and the set appearing under its name in one step and never over one that exists |
| `apps/media_gen/tests/qml_test.cpp` (`media_gen.qml`) | The generator's window, `GeneratorWindow.qml`, loaded offscreen with the real model: the form is filled in and a generation is started from the window's own Generate button, which must end with a verified media set, and any QML warning fails the test. It also checks that a name the folder tree refuses is marked on its row, that the `NO CHECK VALUES` chip follows the Check value column, the footer's state mark, and that a change of language keeps the outcome of the run. Given a second argument it saves a picture of the window |
| `apps/mock_target/tests/engine_test.cpp` (`mock.engine`) | The mock target's engine against the loader's real engine (`DataLoader` and `Runner`) on the fake network, to show that it is a target the loader can use: FIND answered with the configured identity, Information, an Upload that delivers every file byte for byte, both Download modes, an operator abort in the middle of an Upload and at the first status file, each confirmed, and the injections: a refused Upload, a silent target, a garbage initialization file, a wrong checksum option, a lost status file seen as a gap, the target's own abort after a number of status files, and a WAIT answer |
| `apps/mock_target/tests/model_test.cpp` (`mock.model`) | `MockTargetModel` on its worker thread over the fake platform: start and stop, an Upload through the model storing its files, a link that fails reporting the error, binding to the loopback address only when asked, a name the loader sends stored only if it is plain and portable (a load directory is never a device or a dotted name), starting and stopping in quick succession leaving the worker free, and the activity log's role numbers that the shared `LogView` reads |
| `apps/mock_target/tests/qml_test.cpp` (`mock.qml`) | The mock target's window, `TargetWindow.qml`, loaded offscreen with the real model and started and stopped through its own buttons (the footer's state mark follows); the activity log's Ctrl+C text carries the time and the severity word of every line; any QML warning fails the test. Given a second argument it saves a picture of the window |
| `tests/loopback/loopback_test.cpp` (`loopback.integration`) | The loader's real engine (`DataLoader` driven by `Runner`) against the mock target's engine over real UDP sockets on 127.0.0.1, the mock target's engine on a thread of its own: Information, an Upload byte for byte, a media defined and an operator defined Download, and an operator abort in the middle of an Upload. Every port is above 1024 and below the dynamic range, the mock target's status heartbeat is one second, and each scenario builds its own target, host and directories. FIND is not run: the standard fixes it to port 1001 on both ends |
| `tools.write_payloads*` (CTest) | `adl_write_payloads` writes every scenario to a fresh directory and verifies it (the setup test `tools.write_payloads.clean` empties the directory first, since the program never overwrites), `--list` works, and an unknown scenario or a missing output directory is refused |
| `tests/platform/check_fragments.cmake` | `platform.fragments`: each fragment satisfies the platform contract, on whichever host this runs |
| `tests/platform/check_selector.cmake` | `platform.selector`: configures the project with `ADL_TARGET_PLATFORM` set, and checks that a platform with no code is refused with its reason, that QNX is refused until `ADL_ACCEPT_UNBUILT_PLATFORM` is on and then configures, and that a name that is not a platform lists the ones that are |
| `tools/checks/check_platform_isolation.ps1` | `platform.isolation`: no preprocessor conditionals or defines, no OS headers outside the platform section, no OS tests in CMake outside the selector |
| `tools/checks/check_doc_links.ps1` | `docs.links`: every relative link and `#anchor` in every Markdown file resolves, and every picture a page shows exists (needs `pwsh`; no network, other sites' links are not followed) |
| `tools/checks/check_doc_paths.ps1` | `docs.paths`: every repository path that a Markdown file names in a code span (such as `apps/app/core` or `tools/ci/changed_code.ps1`) is a file or folder the repository has, spelled with the same case; fenced blocks are not read. Placeholders (`<name>`, `*`, `$HOME`), a path that does not start at the repository root, `deps/` and a short allow-list in the script (a standard that is not in the repository, a planned folder, a file inside a package; each with its reason) are not checked. A stale path is fixed in the page; the allow-list is for a path that is meant not to exist (needs `pwsh`) |
| `tools/ci/test_changed_code.ps1` | `ci.changed_code`: which changed files `tools/ci/changed_code.ps1` calls documentation, so that a pull request of documentation alone builds nothing ([CI](ci.md#documentation-only-changes)). It holds the files that must count as code (`LICENSE`, `docs/release-signing-key.asc`, a Markdown file below the root, the workflows), a rename into `docs/`, and a queue entry compared with its base (needs `pwsh` and `git`) |
| `tests/system/check_notices.cmake` | `system.notices`: `NOTICES.txt` and `LICENSE.txt` are beside the executables; `NOTICES.txt` names what the build ships (Qt at the version the build used, the typefaces, the standards' publisher), carries the text of the terms (the LGPL and the OFL) and what the platform adds, and has no `{{placeholder}}` left; `LICENSE.txt` is the GPL |
| `tests/packaging/check_install.cmake` | `system.install_consumer`: the development package ([Releasing](releasing.md#the-development-package)). Its component is installed into a scratch prefix; the headers, the CMake package and `LICENSE.txt` are there; the exported targets name neither the source tree, the build tree nor a build-only target (`adl_warnings`, `adl_sanitizers`, `adl_coverage`, `adl_tracy`); and a small program built against it with `find_package(adl)` runs. It is registered only for an optimised build, because a Debug build's libraries carry the sanitizers, which a program using them must be built with too; so it runs in `release.yml` and not on the Debug legs |

## The UI suite

`tests/ui` is the window's own suite, written with Qt Quick Test. Each
`tests/ui/qml/tst_*.qml` file is a `TestCase`, and CTest runs one process per
file (`ui.tst_main_window`, `ui.tst_dialogs`, `ui.tst_first_run`, `ui.tst_window_close`, `ui.tst_right_to_left`,
`ui.tst_keyboard`, `ui.tst_navigation`, `ui.tst_accessibility`, `ui.tst_layout` and `ui.tst_tones`) so that a media
set read in one cannot colour the next. `ui.window_state` is a QtTest program of its own, beside them. `UiSetup` (`tests/ui/ui_setup.hpp`) creates the same rig
the application's other tests use — the real `LoaderModel` on its engine thread,
the simulated target on the fake platform, a media set written to disk and
mirrored into the fake host — and hands the model and a small `Harness` to QML as
context properties. The application's QML is compiled into a module and its controls
come from `libs/ui/qml`, so the suite's build copies the shared and window QML into
one directory with a `qmldir` that declares the `Theme` singleton, and the tests
load that `Main.qml`. It is the window's own source, copied rather than reimplemented;
the copy is what makes a direct component load see the same components the module does.

The events are Qt Quick Test's: `mouseClick` on the button or check box,
`keyClick` for each character typed into a field, and `tryCompare`/`tryVerify`
to wait for the model on its worker thread. The assertions read both what the
window shows (`statusState`, the configuration list, the result label and the
error dialog's text) and what the operation did (`Harness` reports the files the
target read, the downloads in the fake file system and the rows of each list).
The window is made tall enough that the whole left column is on screen: a click
on an item scrolled out of view is not a click the window receives.

The error dialog is a Qt Quick `Dialog`, like the other dialogs, rather than a
native message box: a native modal runs its own event loop and cannot be driven
by Qt Quick Test. That also keeps the dialog's look the same on every desktop. The
suite uses the dialogs' own footer buttons: `AppDialog.standardButton()` maps
`Dialog.Ok`, `Dialog.Save` and `Dialog.Close` to the primary button and
`Dialog.Cancel` to the secondary, so the test clicks the same Save/Close the
operator does.

## Settings in the tests

A test never reads or writes the settings of the person running it, nor those another test (or the
run before) left. Three mechanisms, by what is being run:

- **A test program that uses `QSettings`** (`app.tests`, `app.qml`, `ui.*`, `ui.window_state`) moves
  the default format and place into a scratch directory (`tests/support/scratch_settings.hpp`:
  `adl::test::ScratchSettings`, an ini file in a `QTemporaryDir`). `LoaderModelTest::init()` and
  `WindowStateTest::init()` move it on to a fresh empty directory before every test, and the old one
  is deleted, so `lastAddress`, `downloadDirectory` and the last media set of one test are not there
  for the next. `LoaderModel` tests are given their own file with `settings_in()`, and the UI suite's
  `Harness` has an ini of its own.
- **A program started as a process** (`system.*.starts`) is given `ADL_SETTINGS_DIR` pointing below the
  build tree, which makes the program use an ini file there ([the application](app.md#where-the-settings-are-kept)).
  The variable is read by every `main()` through one helper, `adl::ui::redirect_settings_from_environment()`
  (`libs/ui/settings_location.cpp`).
- **The scratch space** of the whole run is under the build tree (`ctest --preset` and the `check-*`
  targets set `TEMP`, `TMP` and `TMPDIR` to it), and a test removes the directories it makes.

## Holding a transfer half way

The fake platform runs in virtual time, so a whole transfer takes a few milliseconds of real time and a
test on another thread cannot be sure it is still under way when it presses Abort (the earlier abort
tests held the host factory, so the abort landed before the target was even contacted).
`tests/support/gated_host.hpp` is a `Host` that forwards to another and can hold the thread using it at a
chosen file call: `arm(Event::open_read, 2)` holds the engine's thread when the target asks for its second
file (an upload: the first has been read; the application opens each file of the set once before an upload starts, to see that none changed since the check, so the count starts after those), `arm(Event::create, 2)` when the second file begins to arrive (a
download). The test waits until the gate is reached, asks for the abort, and releases it; the target is made
slow (`SimPlan::work_time`) so it cannot finish on its own. The same gate is behind `Harness::holdUploadAtSecondFile()`
and `holdDownloadAtSecondFile()` in the UI suite. The run is deterministic: the engine is single-threaded and
the clock virtual, so what the target has done when the gate is reached is the same every time.

## The RTOS test programs

FreeRTOS and Zephyr are tested by programs of their own, because an RTOS image is a project of its own
(`tests/platform/zephyr`, `tests/platform/freertos`; [building](building.md#zephyr-and-freertos)). Each
builds the same Catch2 sources as the desktop programs, from one list
(`tests/platform/rtos_support/test_sources.cmake`): everything `adl_platform_tests` runs (the portable
types, the memory types, the fake platform, the conformance suite against the real host of the port and
against the fake) and everything `adl_engine_tests` runs (the 615A engine, the runner and whole operations
against a simulated target on the fake platform), and each has two test cases of its own
(`stack_storage_test.cpp`: a thread on a stack the caller made, which only an RTOS port has). The Zephyr
program's last line is `All tests passed (36288 assertions in 227 test cases)`, on `native_sim` and in QEMU. What differs is
the host under test: the port, on Zephyr's `native_sim` or the FreeRTOS POSIX simulator. A test added to the lists of
`tests/CMakeLists.txt` belongs in that file too, or the RTOS programs will not run it.

`tests/platform/rtos_support/temp_dir.hpp` shadows `tests/support/temp_dir.hpp` for them: the scratch
directory is made, and removed with everything in it, through the platform's own `FileSystem` under the
mount point (`/lfs` on Zephyr, the root of littlefs on FreeRTOS), because the host's file system is not
the one the port sees. The `[conformance]` tag works as on the desktops:

```sh
ADL_CATCH_ARGS="[conformance][network]" tools/ci/zephyr_native_sim.sh   # Zephyr: Catch2's arguments in an environment variable
build/freertos-posix-sim/bin/adl_freertos_tests "[conformance]"           # FreeRTOS: a program with a command line
```

`tests/platform/zephyr_smoke` is not part of this suite: a few checks (clock, memory, threads, a mutex, a
queue, UDP) that run on a Cortex-M3 in QEMU, built with the Zephyr SDK's toolchain and no exceptions
(`tools/ci/zephyr_qemu_smoke.sh`). The Zephyr test program itself also runs on a Cortex-A9 in QEMU with that
toolchain (`tools/ci/zephyr_qemu_suite.sh`): the same 227 test cases.

Not run on the RTOS programs: the codec and ARINC 665 unit tests (`adl_unit_tests`; they test code the
desktops already run), the application's tests, and the loopback integration test, which needs the Qt mock
target. [Platform abstraction](platform-abstraction.md#the-freertos-and-zephyr-ports) lists what the
simulators leave open.

## Golden payloads

`tests/golden/make_golden.py` writes a header file, a batch file and the three
lists of a two-member media set. It is a second implementation of the formats,
written from the tables and figures of the standard and sharing no code with the
library, with the CRC from the sample program in Appendix K and the digests from
Python's `hashlib`. Its output is `tests/golden/golden_payloads.hpp`, which is
committed, so building the tests needs no Python. `golden_test.cpp` builds the
same scenario with the library (`tests/support/fixtures.hpp` forwards to the
scenario in `adl::samples`, which mirrors the script, input for input) and compares the bytes.

If the format changes on purpose, change both and regenerate:

```sh
python tests/golden/make_golden.py
```

This catches a mistake in the C++. It does not catch a misreading of the standard
that the script shares; [the coverage page](arinc665-coverage.md#places-the-standard-is-unclear)
lists where the standard is unclear.

`tests/golden/make_golden_615a.py` does the same for ARINC 615A-3: the TFTP requests,
option acknowledgement, ACK, DATA and error packets (including `WAIT:50` and
`ABORT:1004`), the FIND request and answer, and every protocol file, written from the
tables of section 6.4, Attachment 3 and the layouts of RFCs 1350 and 2347, sharing no code
with the library. Its output is `tests/golden/golden_615a.hpp`, committed, and
`arinc615a_golden_test.cpp` builds the same packets and files with the library (through
`tests/support/sample_615a.hpp`, which mirrors the script input for input) and compares
bytes, and reads the independent bytes back. If the format changes on purpose, change both
and run `python tests/golden/make_golden_615a.py`. It shares the choices
[the codec page](arinc615a-codec.md#places-the-standard-is-unclear) lists, so it cannot show
that they match another implementation. Appendix G's packet examples are figures with no
bytes in them, so there is nothing from the standard to check the TFTP layouts against beyond
the `WAIT` and FIND examples and the RFCs.

## Breaking the code on purpose

To see whether the suite fails when the code is wrong, the code was broken on purpose and the tests run.
The ARINC 665 library was broken in these ways: a wrong pointer in the header, file resolution that
ignores the CRC, check characters added where they are exclusive-ored, a Load CRC over the wrong part of
the header, a Load Check Value that starts early, and text fields not padded to a word. Each was caught,
the last by 53 test cases.

The 615A codec was checked the same way, with 25 deliberate breaks, one at a time: a block
number that wraps to 0, another transfer mode, a WAIT text without its colon, lower-case
ABORT digits, block size limits one off, a FIND terminator, the FIND field limits, a
different version, a text length without its terminator, a left-adjusted ratio, a file
length that leaves out its own four bytes, the fields of `.LUS`, `.LCS` and `.LNR` swapped
or off by one, a status code allowed in the wrong field, a decoder that ignores the file
length or the version, an empty required text, a checksum of any length, case-sensitive
option names, an oversized DATA block and an empty OACK. Each was caught. (One was not at
first: the FIND hardware identifier limit, because the test took its figure from the same
constant the library did. The test now writes the limits out.)

The engine was checked the same way, with 38 deliberate breaks, one at a time, each rebuilt and run
(`adl_engine_tests` takes 15 seconds): a sender that answers a duplicate ACK with the block again, a block
number that wraps to 0, two retries and not one, no dally, a size mismatch ignored, a checksum that always
matches or that fails the transfer, a repeated block not acknowledged, any block size accepted, a stray
transfer ID answered, a WAIT in the middle taken as a WAIT, an abort that sends nothing, an OACK with a
larger block size accepted, the time-outs of the standard changed by a second (TFTP, DLP, FIND), a second DLP
retry, an exception timer ignored, a repeated status counter shown again, lost status files not counted, a
first counter of 1 called a gap, the list written before the accepted status, an abort that leaves transfers
open or does not answer the first status file request, the protocol version not checked, a file nobody asked
for taken, a checksum mismatch failing the operation, the Information fast path lost, WAIT not waited for, no
fallback when options are refused, `WAIT` answered with an error, completion that does not wait for a transfer
still open, a FIND answer accepted late, repeated, or with no identifier, a list transfer that does not
suspend the time-out, and the port option always sent. Six survived the first run (the block size a server
accepts, the time-out running while a status file or the list is arriving, a first counter of 1 reported, an
abort that waits for the status file instead of ending the open transfers, completion that does not wait, the
list transfer's suspension); each got a test, and each is now caught. One break still survives and is
equivalent: the guard in `check_watchdog` that skips the check while a transfer suspends the time-out is
redundant with the deadline being set to `never` at that moment. (The Sorcerer's Apprentice break made the
test program loop for ever, which is how the bug looks; it counts as caught.) The run did not catch a bug
in the runner, but the sanitizer did: the first version used a socket that an earlier command of the same
round had closed.

## Adding a test

Put a `TEST_CASE` in the file for the thing it tests, or add a file under
`tests/unit/` and list it in `tests/CMakeLists.txt`. Name the case as a
sentence about behaviour, tag it, and when the standard says what the answer is,
use the standard's value and say where it comes from. A test of one platform's
code goes in `tests/platform/` (the platform section is the one place the
isolation check allows OS headers). Helpers for building byte strings, damaging a
file and putting its CRC right again are in `tests/support`.

A bug fix comes with the test that failed before it.

## The fake and the conformance suite

The platform layer is tested by comparing two implementations of one contract. The
fake (`tests/fake`, [described here](platform-abstraction.md#the-fake-platform))
runs in memory on a clock that moves only when told to, so the engine's tests
need no socket and no real second; the conformance suite then checks that the fake
and the real host of each platform behave the same way, so that those tests mean
something about the real one. A conformance check is written with
`GENERATE(HostKind::fake, HostKind::real)` and `HostUnderTest`
(`tests/conformance/host_under_test.hpp`), which hands it a host and a scratch
directory; a check that needs real concurrency is written for the real host only
and tagged `[real]`. A port runs the suite on its target as its first test.

## The simulated target

The engine's operations are tested against a simulated 615A target (`tests/engine/sim_target.hpp`), an
`adl::fake::Agent` attached to the fake network at an address of its own, with the loader on another. It
behaves as sections 5.4 and 6.3 say a target does: it serves the initialization file, writes status files
as a heartbeat (counters and all), reads the load's files from the loader (an upload), writes the files and
the lists of a download, writes `.LCL`, and answers FIND. A `SimPlan` makes it slow, silent, denying, aborting,
waiting, refusing options, speaking another protocol version, losing or repeating a status file, sending a wrong
checksum, announcing an exception timer, or asking for a file that was not offered. Its TFTP is
`MiniTransfer` (`libs/arinc615a_mini/src/mini_tftp.cpp`): lock-step blocks, a two second time-out and one retry, written
plainly and sharing no code with the engine, so that the engine is run against a second reading of RFC 1350
and section 5.3.2 and not against itself. It does use the codec for the packets, which the golden tests
check against a writer of their own.

Everything runs on the fake clock, which moves only when told to, so a transfer of thirteen seconds of
silence takes a few milliseconds, and the same seed is the same history on every platform (the generator is
SplitMix64, not a standard library's distribution). The network's `LinkProfile` (latency, jitter, loss,
duplication, corruption) is how loss, repeats and reordering are made; the target's own faults are in the plan.
After the runner returns the rig advances the clock a second more, so that the target receives the loader's
last datagram, and `Rig::ended` is the time the runner stopped.

The media set the Upload loads is built with `adl::arinc665::build_media_set` from the sample load
(`adl::samples`), and served by a `FileSource` over its members that tells two files of one name apart by
the CRC in the request, as a header file's entries would. Because the target is told the files to read and
what to expect of each, it checks every byte it reads.

## Known gaps

- Nothing in the 615A codec or engine has been checked against another implementation of the
  protocol, a target or a loader; [the choices the codec made](arinc615a-codec.md#places-the-standard-is-unclear)
  and [the engine's](arinc615a-coverage.md#places-the-standard-is-unclear) are the first things to confirm.
  The simulated target has a TFTP of its own, but it is the same reading of the standard.
- The application has been run against the simulated target and the mock target, never a real one. The automated tests drive its window offscreen with Qt Quick Test, at one size and in the light theme. The window has also been driven on a real Windows desktop, in both colour schemes, by the script that takes the screenshots ([Screenshots](screenshots.md)); that is not part of the automated tests.
  `app.tests`, `app.qml` and the `ui.*` tests (ten `ui.tst_*` suites and `ui.window_state`) run on the three desktop legs.
- No test runs a 615A operation against a real target. The loopback integration test
  (`loopback.integration`) does run whole operations over real sockets, but against the mock
  target's engine on the same machine; the runner otherwise runs on the fake platform, and the
  platforms' sockets are covered by the conformance suite. macOS has not run any of the engine's
  tests except in CI.
- The codec and the engine are meant for a build without exceptions and RTTI. The `Zephyr (QEMU, SDK toolchain)`
  job builds them that way for a Cortex-M3 (the smoke program's `prj.conf` turns both off), but that program
  does not run them, and no job builds them that way with the desktop compilers. (They compile that way under
  g++ 15 with `-fno-exceptions -fno-rtti`, checked once by hand.)
- The golden payloads come from the same reading of the standard as the library.
  Nothing has yet been checked against a load or a media set written by another
  tool.
- Appendix L's random reference files and its Ethernet frame are not in the
  standard.
- No tool other than this one has read the trees `adl_write_payloads` writes.
- The macOS platform sources have not yet been run on a Mac: the first CI run of
  the platform layer is their first test. (The Linux ones have, in a container
  with GCC 15 under the sanitizers.)
- QNX's sources have never been compiled and VxWorks has none; the conformance suite is how a
  port will find out where it differs. FreeRTOS and Zephyr have run the suite on simulators only
  ([below](#the-rtos-test-programs)); no board has run it.
- The conformance suite cannot show a data race: the fake runs a thread when it is
  joined, and the real-host checks are a handful of contended runs, not a proof.
- A real allocator is not asked for an impossible amount of memory under the
  sanitizers, which abort the process instead of returning null; `resource_exhausted`
  is tested on the fake.
