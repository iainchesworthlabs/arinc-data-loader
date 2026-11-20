# The mock target

`apps/mock_target` is the second Qt application: a mock ARINC 615A target that runs on Windows,
macOS and Linux over real UDP sockets, so that the data loader's dialogues can be demonstrated
end to end against something that behaves like a target without an embedded box. It is a mock
for demonstrations, not a real target, and it is not what the loader's tests run against (those
use the simulated target of `tests/engine`, which runs in memory on the fake platform).

The window shows what the target is doing while it does it: it answers FIND with the identity
the operator configured, serves the initialization files, writes its status files, reads the
load's files (an upload), writes the files of a download, and stores what an upload left in it
on disk. The operator can also make it misbehave, to watch the loader cope: answer WAIT, refuse
an operation, drop status files, send a wrong checksum, abort on its own, or go silent.

## What it does

| What | How |
| --- | --- |
| FIND | Listens on UDP port 1001 (configurable) and answers a FIND request with the configured identity: target hardware ID, type name, position, literal name and manufacturer code (`adl::arinc615a`'s FIND codec). |
| Initialization files | A TFTP server on port 59 (configurable) serves `<THW_ID_POS>.LCI`, `.LUI`, `.LND` and `.LNO` with an "accepted" status file and protocol version A4. It honors the loader's options (block size, and the `port` option that lets the loader use a control port of its own). With an injection it answers WAIT or refuses the operation with 0x1000 ("operation denied"), so the loader's error paths can be shown. The engine can also refuse with 0x1002 and serve an initialization file of an invalid structure (`Config::refusal`, `Config::garbage_init`); the window has no control for either. |
| Information | Writes the configuration status files (`.LCS`), then the configuration list (`.LCL`, hardware with parts), then the final status 0x0003, on the DLP status cadence. |
| Upload | Reads the upload request (`.LUR`), then each load's header, data and support files from the loader, checking each against the check value the header file records; writes upload status files (`.LUS`) with per-load progress; stores what it read to `<storage folder>/<load part number>/<file name>`; ends 0x0003, or 0x0002 naming the failed loads. |
| Download | Both modes: for a media defined download it serves the files the loader's `.LNR` asks for; for an operator defined one it writes the list of files it holds (`.LNL`) and serves only the ones the loader's `.LNA` chooses. The files it can serve are read **once, when Start is pressed** (the session), from `<storage folder>/holds` (a folder you make; the mock target does not create it), so a file put there while the target is running is not served until it is stopped and started again; with no storage folder, no `holds` folder or nothing in `holds` it holds one demonstration file, `DEMO.TXT`. Download status files (`.LNS`) carry the progress; it ends 0x0003. |
| Aborts | When the loader sends ABORT, the mock target stops its transfers and ends the operation with a final status file carrying the loader's code, which is what makes the loader report the abort as confirmed. |
| Injections | Answer the first request with WAIT for N seconds, answer every request with WAIT, refuse one chosen operation (Information, Upload, Download media defined, Download operator defined), drop every Nth status file, a bit flipped in every served file (the protocol files included, so a download ends at once as "unsupported protocol version"), a wrong checksum option (reported as a notice; the file is kept and the operation completes), an abort (status 0x1003) after N status files, and silence after the first status file (the loader then runs into its 13 second data load protocol time-out). Think time (0 to 60 000 ms, default 1000) and the status file period (0 to 3600 s, default 4) shape the pace of a dialogue. Defaults: behave. |
| The log | Every status file, file read or written, WAIT and abort as a line with a severity, shown in the window like the loader's activity log. |
| The language | A language box in the header, beside About, offers the eight languages of the family; the choice applies at once and is kept. Hebrew and Yiddish mirror the window ([Hebrew picture](images/mock-hebrew.png)). The window, its model's messages and the shared components are translated (machine drafts no speaker has read, [translations](translations.md)); the activity lines the 615A engine writes about its own work (`listening: FIND on port ...`, `served ...`) are English in every language. |

## The command

`adl_mock` is the same mock target as a command, for test rigs and CI jobs: no window, no Qt, one target in
one process. A job starts it, points a loader at it by address, and reads what happened. It is a mock, not a
real target, and it has not been run against real equipment. It answers FIND, serves the initialization
files, and runs the Information, Upload and Download operations exactly as the window's target does (the
engine is the same, [How it is put together](#how-it-is-put-together)). Its source is `apps/mock`.

```sh
adl_mock --storage out/kept --operations 1 --json
```

It runs until it has served `--operations` operations, or for `--seconds`, or until it is killed. It handles
no signal: killing it abandons what it was doing, and the loader that was in an operation times out by itself.

### Options

Options are written `--name value`. A value is never a list with commas: an option that takes several values
(`--part`) is given several times. The same options and exit codes, and each message, are in the user's guide:
[Command line and exit codes](user-guide/command-line.md#adl_mock). A folder (`--holds`, `--storage`) is given as the system
writes it, in any characters it allows: `main` takes its arguments from `adl::platform::arguments` as UTF-8, and makes each
folder with `path_from_utf8` ([Command-line programs](architecture.md#command-line-programs)).

| Option | Meaning | Default |
| --- | --- | --- |
| `--hardware-id <text>`, `--position <text>` | The target's identity. `THW_ID_POS` is the two joined by `_`, and it names the protocol files. Letters, digits, `.`, `_` and `-` only; at most 15 and 8 characters. | `HNPFMS`, `L` |
| `--type-name <text>`, `--literal-name <text>`, `--manufacturer <text>` | The rest of the FIND answer: at most 8, 20 and 3 characters | `FMS`, `FMS LEFT`, `HNP` |
| `--part <hardware>:<part number>[:<amendment>[:<designation>]]`, repeatable | The hardware and parts the Information operation reports. The parts of one piece of hardware share its name; the serial numbers are `SN-0001`, `SN-0002` and so on in the order the hardware is first named; a part with no designation is designated by its hardware | The engine's own list |
| `--holds <folder>` | The files it holds for a download: the files directly in the folder, read once when the program starts | One demonstration file, `DEMO.TXT` |
| `--storage <folder>` | Where uploads are stored, as `<folder>/<load part number>/<file name>`. The folder is made if it is not there | None: uploads are served and not kept |
| `--address <ipv4>` | The local address it listens on. `0.0.0.0` is every interface | `127.0.0.1` |
| `--tftp-port <n>`, `--find-port <n>` | The ports | 59, 1001 |
| `--think-time-ms <n>`, `--status-period <seconds>` | The pace: 0 to 60000 ms before it starts work, and 0 to 3600 s between status files | 1000, 4 |
| `--wait-first <seconds>`, `--wait-every <seconds>` | Answer the first request, or every request, with WAIT for that many seconds (1 to 65535) | Off |
| `--refuse <operation>` | Refuse that operation: `information`, `upload`, `download-media` or `download-operator` | Off |
| `--refuse-code <code>` | Refuse with `0x1000` (denied) or `0x1002` (not supported). Needs `--refuse` | `0x1000` |
| `--invalid-init-file` | Serve an initialization file of an invalid structure | Off |
| `--drop-status-every <n>` | Lose every nth status file (never the first or the last) | Off |
| `--corrupt-served` | Flip a bit in the files it serves | Off |
| `--wrong-checksum` | Write a wrong check value option | Off |
| `--abort-after <n>` | Abort (0x1003) after n status files | Off |
| `--silent` | Say nothing after the first status file, so the loader runs into its 13 second time-out. The operation never ends, so it needs `--seconds` | Off |
| `--scenario <file>` | Take the faults from a scenario file, which says which fault happens when, in place of the fault options above ([Scenarios](#scenarios)) | Off |
| `--operations <n>`, `--seconds <n>` | Stop after n operations (a refused one counts), or after n seconds | Run until killed |
| `--json` | One JSON object a line | One plain line an event |
| `--help`, `--version` | The usage; `adl_mock <version> events 1` | |

Faults are as in [the window](#what-it-does), with two that the window has no control for: `--refuse-code 0x1002`
and `--invalid-init-file`. Wrong options are refused, with exit code 2 and nothing on standard output: an option
given twice, a number out of its range, `--refuse-code` without `--refuse`, `--silent` without `--seconds`,
`--wait-first` with `--wait-every`, and parts that do not fit in a configuration list.

FIND is fixed to port 1001. Two programs on one computer sharing that port is not reliable, so a loader on the same
computer names the target by its address, as the window's does. On Linux and macOS ports below 1024 (59 and 1001)
may need privileges; give `--tftp-port` and `--find-port` high ports and tell the loader the TFTP port. The default
address, `127.0.0.1`, listens on this computer only and raises no firewall prompt; a rig that wants the network
says `--address 0.0.0.0` (or the address of one interface).

Several targets on one computer: the engine writes some of its datagrams from ports of its own, from 40000 up, on the
address it listens on, so two targets on one address take those ports from each other. Give each its own address
(`127.0.0.2`, `127.0.0.3` and so on; they have port 59 and port 1001 each). A datagram that cannot be sent because its port
is taken is lost, as UDP allows, and the loader's operation then times out.

### Output

Standard output carries the events and nothing else. Standard error carries what is not a result: a usage error,
a file that could not be stored, why a run failed. Each event is written, and flushed, when it happens.

By default an event is one plain line for a person: the seconds since the program started, the name of the event
and `key=value` pairs (a value with a space or a quote in it is written as a JSON string):

```text
0.003 started program=adl_mock version=0.1.1 events=1 thw_id_pos=HNPFMS_L address=127.0.0.1 find_port=1001 tftp_port=59 faults=none
1.210 operation_began operation=information
1.211 progress operation=information percent=0
...
5.730 operation_ended operation=information status=3 status_name=completed refused=false
5.731 stopped reason=operations operations=1
```

With `--json` an event is one JSON object on a line: UTF-8, ended by a line feed (on Windows the standard output is a text stream, so the line ends in a carriage return and a line feed; a reader that splits at the line feed and ignores a carriage return at the end of a line reads both), with `"event"` as the first key and
`"t"` as the second. `"t"` is the seconds since the program started, as a number with three decimals. A name that
came off the wire is written as JSON; bytes in it that are not UTF-8 are written as U+FFFD.

| `event` | When | Keys beside `event` and `t` |
| --- | --- | --- |
| `started` | First: the ports are open | `program` (`adl_mock`), `version`, `events` (the number of this format, 1), `thw_id_pos`, `address`, `find_port`, `tftp_port`, `faults` (the fault options that are on, as written, without their values) |
| `operation_began` | A request was accepted | `operation`: `information`, `upload`, `download_media_defined` or `download_operator_defined` |
| `progress` | The percentage of the list it has got through changed | `operation`, `percent` (0 to 100) |
| `upload_received` | An upload completed, before it ends | `part_number`, `files` (the names, the header file first), `stored_in` (the load's folder, or `null` when nothing was kept) |
| `operation_ended` | The last status file went out, or the operation was refused and never began | `operation`, `status` (the status code as a number, or `null` for an invalid initialization file), `status_name` (the name of the code: `completed`, `aborted_by_target`, `operation_denied`, `operation_not_supported`, ..., or `invalid_initialization_file`), `refused` |
| `scenario_step` | With `--scenario`: the trigger of a step occurred, before what the action causes | `step`, `trigger`, `at`, `operation`, `action`, `applied`, and `seconds`, `status` or `ms` for the action that has one ([Scenarios](#scenarios)) |
| `stopped` | Last | `reason` (`operations`, `seconds` or `error`), `operations` (how many ended), and with `--scenario` `scenario_unused` (the numbers of the steps that never fired) |

A reader refuses an `events` number it does not know, and ignores a key it does not know: a key can be added
without changing the number, and removing or renaming one, or changing what one means, changes it. A target that
has gone silent never ends its operation and writes no `operation_ended` for it.

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | It stopped as asked (`--operations` served, `--seconds` passed), or printed `--help` or `--version` |
| 1 | The run failed: the platform would not start, or waiting on the network failed (`stopped` says `error`) |
| 2 | Wrong usage |
| 3 | A folder cannot be used: `--holds` is not a folder or cannot be read, or `--storage` is not a folder and cannot be made |
| 4 | A port cannot be opened: it is in use, or needs a privilege |
| 5 | The scenario file given with `--scenario` cannot be read or is refused |

On exits 2, 3, 4 and 5 no event is written: the target never started.

### Scenarios

A scenario is a file that says which fault the target shows and when, so that a run with faults can be repeated: a list
of steps, each with a trigger (the nth request, the nth status file, or a time since the operation began) and an action
(any of the faults above, or a delay), and a seed that fixes every chance. `--scenario <file>` takes it in place of the
fault options (it cannot be given with one), and the file is read before anything is opened: a file that cannot be read
or is refused ends the program with exit code 5 and one sentence on the standard error. The file, the triggers, the
actions, the seed and every sentence are in the guide, [Scenarios for the mock target](user-guide/mock-scenarios.md).

The engine knows nothing of scenario files. It asks a plan (`FaultPlan`, `adl/mock/fault_plan.hpp`) at each request for
an initialization file, at each status file, and when a step set for a time after an operation began comes due, and does
what the answer says with the faults it already has, and one thing more, a datagram held back. `ScenarioPlayer`
(`libs/mock_scenario`) is the plan that a scenario makes: it counts the occurrences of each step, draws from the stream
of the step (SplitMix64, `libs/scenario_base`), and tells an observer about each, which is what `adl_mock` writes as the
`scenario_step` events. With no plan the engine behaves as it always did.

## Layout

```text
libs/mock_target_core/  adl_mock_target_core: the target without a window and without Qt
  engine                the 615A target behaviour: no Qt, no socket, one thread, driven by
                        datagrams and timers (like the loader's own engine, the other way round)
  link                  the seam the engine speaks through: RealTargetLink over adl::platform's
                        sockets, and the tests' FakeTargetLink on the fake network
  storage               the files it holds for a download and what an upload leaves on disk
  settings              the operator's settings turned into the engine's configuration
apps/mock/              adl_mock: the same target as a command (no window, no Qt)
  main.cpp              the call to run_command
  src/, include/        options, events, the JSON writer, and the run function
apps/mock_target/
  main.cpp              creates the QGuiApplication, one MockTargetModel and the QML engine
  qml/                  TargetWindow.qml (the window)
  core/                 adl_mock_core: the window's side, Qt Core only
    model               MockTargetModel: the worker thread, the properties and signals the
                        window binds to
tests/mock_target_core/ the engine against the loader's real engine on the fake platform
tests/mock_cli/         adl_mock: its writer, events and options, and the program against the loader
apps/mock_target/tests/ the model, and the window loaded offscreen
libs/ui/                 the shared design system (Theme, controls, header, footer, log)
```

The window (1280×900, at least 1120×760) follows the desktop's light or dark scheme and remembers its size and place. The header owns the run state and Start/Stop: a `SERVING` / `STOPPED` chip and the two buttons (Start needs a hardware ID; ports must be 1 to 65535). The primary button is the one that applies: Start while stopped, Stop while serving. Every field is disabled while the target is serving, so a configuration is changed by stopping, editing and starting again.
The left rail is the configuration, numbered (identity, ports, storage, behaviour, fault
injection), and the right is the evidence (the operation, the activity log and the stored files).
Fault injection is its own block with an `N ACTIVE` / `BEHAVING` chip and `Reset all`, and, because
that block is below the fold at 1280×900, any armed injection also shows as a "N faults injected" chip in
the header and the footer; pressing either scrolls the rail to the block, so a demo cannot quietly run
with a stale fault. The state is named once, in the header; the operation panel says what the target is
doing ("Waiting for a request", the operation's name, "Not started") and the footer says whether and
where it listens. The progress bar moves only while the target is working on a request, not while it
merely listens, and an error from the target is an `AppBanner`. The derived `THW_ID_POS` is shown and
copyable; an unset storage folder is called out as "still serves, not kept".

## How it is put together

- **The engine is the loader's engine turned round.** `MockEngine` holds no socket, thread or
  clock: it is handed datagrams and time, and it answers with datagrams and timers, exactly like
  `adl::arinc615a::DataLoader` on the loader's side. It is event-driven and single-threaded;
  `run()` is the loop that adapts it to a real link (wait, feed, fire timers), and over the fake
  link the world's events drive it directly.
- **Its TFTP is the second implementation.** Every transfer goes through
  `adl::arinc615a_mini::MiniTransfer` (the separate TFTP written for the tests, now shared
  from `libs/arinc615a_mini`), and every protocol file through `adl::arinc615a`'s codec. The
  mock target shares no transfer code with the loader's engine, so a dialogue that completes
  exercises two implementations of the transfer, both written in this repository. Neither has met
  a real target.
- **Uploads are checked, not just received.** Each file the target reads is verified against the
  check value its header file records (the header's check value, or the 16-bit CRC field), and
  the header file itself is decoded with `adl::arinc665::decode_header_file` to learn which
  files to read. A load whose file fails the check is failed in the upload status file, which
  is what makes the loader report the operation as failed.
- **The link owns the platform.** `RealTargetLink` makes an `adl::platform::Host` once per
  session on the engine's thread (like the loader's `system_host_factory`), binds the FIND and
  TFTP ports with `reuse_address` and a receive buffer, opens one socket per transfer port on
  first use, and waits on all of them with a deadline taken from the earliest pending timer.
- **The model is the loader's worker pattern.** `MockTargetModel` lives on the window's thread,
  owns the engine's `QThread`, and everything the engine reports crosses as queued signals of
  copied Qt values. Stopping reaches the session through the link's flag, which the run loop polls (a
  queued call cannot reach a thread that is inside the loop). The model counts the sessions it queues, and
  a stop covers the sessions queued up to the moment it is asked for, under the mutex that also guards the
  link a stop is delivered to: a stop that comes before the session has begun, while its link is being
  made, while it runs, or while it is taken down is delivered once and to the right session, never to a link
  that is being destroyed, and a stop between two quick starts is not lost (a single flag cleared by the next
  start lost it, and left the second session serving with the window showing it stopped). A stress test
  starts and stops the model 300 times with a different gap each time.
- **What the target stores is named safely.** A file the loader sends is stored only under a plain name (letters,
  digits and `. _ -`) that `adl::arinc665::is_portable_file_name` also accepts, so a colon, a device name
  (`NUL`, `COM1`...) or a trailing dot is refused on every platform; the load's folder is its part number
  with anything else turned into `_`, and a device name or trailing dot made harmless.
- **The files a download serves are read when Start is pressed** (the session), not when a download
  begins: a file put in `holds` afterwards is served after a stop and a start.
- **The window** lets the operator edit the identity, the ports, the storage folder and the
  injections before pressing Start, and shows the operation, its progress, the log and the
  files an upload stored while it runs.

## What has and has not been checked

Checked: the engine against the loader's real engine (`DataLoader` and `Runner` on the fake
platform, both ends on one fake network): FIND, a full Information operation, an Upload in which
every file arrives byte for byte, both Download modes, an operator abort with the target's
confirmation, and every injection the loader has a path for (denied, DLP time-out, protocol
error, checksum mismatch, a lost status file, a target-side abort, WAIT). The same dialogues
over **real sockets**, through the loopback integration test (`loopback.integration`): the
loader's runner over the real platform against the engine over `RealTargetLink` on 127.0.0.1 —
Information, an Upload stored byte for byte, both Download modes and an abort. The command `adl_mock`, started from its own run function in a thread, against the same real runner over loopback sockets (`mock.command`): Information, an Upload stored byte for byte, both Downloads, a refused operation counted, `--seconds`, and its exit codes for wrong usage, a folder that cannot be used and a port that cannot be opened; its JSON writer and options without a network (`mock.cli`). The model on its
worker thread, storage written to disk, and the window loaded offscreen with no QML complaints,
in the light and the dark scheme. The platform-isolation check covers the new sources.

Also checked, by hand, on Windows, with the three real programs on one machine and a script driving
the windows ([the screenshots](screenshots.md)): the whole demonstration of
[the user's guide](user-guide/demonstration.md) (Information, an Upload stored byte for byte, both
Downloads), a refused upload, a silent target with an abort that is not confirmed, a target that is not
running, WAIT, dropped status files and an abort by the target.

Not checked: FIND between the two applications on one machine does **not** work (the loader's
broadcast is answered by nothing: "0 targets answered", on Windows; FIND is fixed to port 1001 on
both ends by the standard, and two sockets sharing a port on one machine is not reliable) — typing
the target's address is the single-host path; on Linux and macOS binding ports 59 and 1001 may need
privileges (`Config::bind_address`, the window's **This computer only** setting, binds 127.0.0.1
instead of every interface: no firewall prompt on Windows, but on macOS a low port on a single
address may still need root; see the user's guide, Installing). The loader binds loopback too when
its target is a loopback address (`RunOptions::local_address`). The window at scales other than 100%; keyboard use and screen readers beyond the
accessible names.
