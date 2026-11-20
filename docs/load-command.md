# The command adl_load

`adl_load` runs one ARINC 615A operation on one target, or looks for targets, with no window. It is for scripts and for other programs: what it prints is the same kind of thing the loader's log and receipt show, as lines of JSON that a program can read, and its exit code says how it ended. The loader's window ([the application](app.md)) does the same work for a person.

It has no Qt and no window, and it is English only: nothing it writes is translated, whatever language the window is in. The user's guide has the options, the exit codes and every sentence it says ([Command line and exit codes](user-guide/command-line.md#adl_load)); this page is for the programs that start it and read what it writes. The source is in `apps/load`; the library it is a thin `main` over (`adl_load_cli`) is not installed. The work of an operation is `adl::loader_core` ([Using the libraries](libraries.md)).

## Usage

```text
adl_load find         [--seconds <n>] [--local-address <ipv4>] [--json]
adl_load information  --target <ipv4> --position <THW_ID_POS> [common]
adl_load upload       --target <ipv4> --position <THW_ID_POS> --media-set <folder>
                      (--load <part number>... | --all-loads) [common]
adl_load download     --target <ipv4> --position <THW_ID_POS> --into <folder>
                      (--file <name>... | --pick <name>... | --pick-all) [common]
adl_load serve        [--local-address <ipv4>] [--control-port <n>] [--max-operations <n>]
adl_load --help
adl_load --version

common: [--local-address <ipv4>] [--control-port <n>] [--target-port <n>] [--json] [--record <file>]
        [--scenario <file>]
```

Options are written `--name value`, with a space. An option that takes several values is given several times (`--load A --load B`), never as a list with commas. A value that begins with `--` is taken to be the next option, so a folder or a name that begins that way is written `./--name`. `--help` prints the usage and exits 0. `--version` prints one line, `adl_load <version> events 1`, and exits 0; the name has no `.exe` on any system. Both are taken as soon as they are met, before the arguments after them are read.

| Option | Meaning |
| --- | --- |
| `--target <ipv4>` | The target's address, four numbers of 0 to 255 separated by dots. |
| `--position <THW_ID_POS>` | The target's `THW_ID_POS`, as it names itself (`HNPFMS_L`): 1 to 240 characters, none of them a control character, a slash or a backslash. For a download also none that a folder name cannot hold (a colon, an asterisk, a question mark, quotes, angle brackets, a bar), because the download folder is named after it. |
| `--media-set <folder>` | An upload: the folder of the media set, one folder for each member, as the generator and `adl_write_payloads` write it. |
| `--load <part number>` | An upload: a load of the set to send, by its part number. Given once for each load; they are sent in the order given. A part number that the set does not hold, or that names two loads of it, is wrong usage. |
| `--all-loads` | An upload: every load of the set. |
| `--into <folder>` | A download: the folder to write into. A folder of its own, `DNLD_DATA_<THW_ID_POS>_<number>`, is made inside it with the lowest number that is free, as the window does; `--into` itself is made if it is not there. |
| `--file <name>` | A download in which the loader names the files (the media defined download). Given once for each file. A name has to be a plain file name, without a path. |
| `--pick <name>`, `--pick-all` | A download in which the target lists the files it holds and the command chooses from the list (the operator defined download). `--pick` names the files to take, once for each; `--pick-all` takes all of them. |
| `--seconds <n>` | `find`: how long to listen, 1 to 3600. The default is three, the window's. |
| `--local-address <ipv4>` | The interface the sockets are bound to. By default what the window does: the loopback address when the target is on this computer (an address of `127.0.0.0/8`), every interface otherwise. For `find`, every interface. |
| `--control-port <n>` | The port the loader listens on for what the target sends, 1 to 65535; the default is 59. The loader tells the target which port to write to, in the `port` option of its request, when its own port is not 59 or the target's port is not 59: a target on another port would otherwise write to its own port, where the loader is not. A loader and a target that are both on 59 exchange no option. Another loader port matters when the target is on this computer and holds 59 itself. |
| `--target-port <n>` | The port the target listens on for the request that starts an operation, 1 to 65535; the default is 59, the standard's. Another port is for a simulated target that cannot have 59: a system that keeps low ports for privileged programs, or several targets on one address. A real target is on 59. `find` does not have it. |
| `--max-operations <n>` | `serve`: how many operations run at once, 1 to 64; the default is 8. |
| `--json` | Standard output carries JSON lines and nothing else. |
| `--record <file>` | Write the operation record when the operation ends, whatever its outcome. `find` writes no record. |
| `--scenario <file>` | `information`, `upload`, `download`: do something wrong on purpose, as the file says ([below](#--scenario-faults-on-purpose)). `find` and `serve` refuse it. |

Wrong options are told on standard error, with the usage after them, and the exit code is 2; no event is written, not even `started`. That includes what only the media set shows: a part number it does not hold, a part number that names two of its loads, or `--all-loads` for a set with no load. So that this can be wrong usage with no event, an upload reads and checks the media set **before** it writes `started`: with a large set, `started` is written after the time the read takes, and its `t` is then not 0.

The target is reached at port 59, the standard's, unless `--target-port` says another. On Linux and macOS a program may need privileges to bind a port below 1024: `--control-port` is the loader's own, and `--target-port` is for a simulated target that cannot have 59 either.

## What the output is

Standard output carries the result and standard error carries wrong usage and nothing else. Without `--json` each event is one plain line for a person to read; its words are not a format to depend on. With `--json`:

- One object on a line, UTF-8, ended by a line feed, and written out and flushed at once, so that a reader sees each event when it happens. On Windows a carriage return may come before the line feed; a reader accepts both. Text that a target sent and that is not valid UTF-8 is written with U+FFFD for each byte that is not.
- `event` is the first key of every event and `t` the second: the seconds since the program started, as a number with milliseconds (`12.345`).
- The first event is `started`. It carries the program's name and version and `events`, the number of this format, which is 1. A reader refuses a number it does not know.
- The last event is `finished`, and it is written once. Its `exit_code` is the exit code of the process.
- A reader ignores a key it does not know. A key can be added without changing the number; removing a key, renaming one or changing what one means changes it.

| `event` | When | Keys beside `event` and `t` |
| --- | --- | --- |
| `started` | First | `program`, `version`, `events`, `command`, and for an operation `target` and `position`; with `--scenario` also `scenario`, `scenario_steps`, `scenario_seed` |
| `target_found` | `find`, once for each answer, as it comes | `address`, `hardware_id`, `type_name`, `position`, `literal_name`, `manufacturer_code`, `thw_id_pos` |
| `media_set_checked` | `upload`, after the check | `path`, `part_number`, `members`, `loads` (the count), `findings` (a list of sentences) |
| `log` | Any time | `severity` (`info`, `note`, `warning` or `error`), `text` |
| `configuration` | `information`, when the target's list is read | `hardware`: a list of `literal_name`, `serial_number` and `parts`, a list of `part_number`, `amendment`, `designation` |
| `progress` | `upload`, on each status file from the target | `percent`, `loads`: a list of `part_number`, `header_file`, `status`, `percent` |
| `file_list` | `download` with `--pick` or `--pick-all`, when the target's list is read | `files`: a list of `name`, `description` |
| `file_received` | `download`, once for each file | `name`, `size`, `path` |
| `scenario_step` | With `--scenario`, each time a step acts on a datagram or on the operation | `step`, `trigger`, `at`, `operation`, `action`, `applied`, `datagram`, `file`, `block`, `to`, and the one parameter of the action, under the key the file has for it (`copies`, `ms`, `within_ms`, `for_ms`, `delta`, `code`, `seconds` or `status`) |
| `finished` | Last | `result`, `reason`, `status_code`, `detail`, `exit_code`, and where they apply `count` (`find`), `download_folder` (a download), `record` (when `--record` was given), `scenario_unused` (when `--scenario` was given) |

The `log` events are the lines of the record's log, one for each: what the window's log shows, in English. The `status` of a load is the name of its status in lower case with underscores (`accepted`, `in_progress`, `completed`, `item_failed` and so on): the same word in every version, not a sentence. Paths are written as the operating system writes them, as they were given or joined to what was given.

`result` is `completed` or `failed`. `reason` is one word for a program to read, and `detail` one English sentence for a person. `status_code` is the last ARINC 615A status code the target reported, as a decimal number (4096 for 0x1000), or `null`; it includes the target's answer to the request that starts the operation, so a refused operation carries the refusal's code. `download_folder` is the folder the files were written into, or `null` when none was kept (a download that received nothing leaves no empty folder). `record` is the path the record was written to, or `null` when `--record` was given and the record could not be written.

## Reasons and exit codes

| `reason` | Meaning | Exit code |
| --- | --- | --- |
| `completed` | The operation completed, and for an upload every load did. For `find`: it ran, and `count` may be 0 | 0 |
| `target_refused` | The target refused the operation, or does not do it | 1 |
| `target_aborted` | The target ended the operation | 1 |
| `load_failed` | The upload ended and the target failed at least one load | 1 |
| `missing_files` | The download ended with fewer files than were asked for, or a file failed. A `--pick` name that the target does not list is one: the files that are listed are still taken, and the name is in `detail`. A list with nothing to take, for `--pick-all`, is one too | 1 |
| `protocol_error` | What the target sent could not be understood, or its protocol version is not supported | 1 |
| `aborted` | The operation was aborted from this side. `adl_load` has no way to do that; a record written by the window can carry it | 1 |
| `usage` | Wrong options (no events are written) | 2 |
| `media_set_unreadable` | The media set could not be read or checked | 3 |
| `media_set_changed` | A file changed after the set was checked | 3 |
| `no_answer` | The target did not answer | 4 |
| `local_error` | A port could not be opened, a folder could not be made, the record could not be written, or the scenario file could not be read or was refused (no event is written for the last) | 5 |

An upload reads and checks the media set before it asks the target anything, as the window does. A set that cannot be read gives `started`, a `log` event with the reason and `finished`, with no `media_set_checked`. A file or folder in a member whose name is not printable ASCII (a media set can hold nothing else) makes the set unreadable, and the sentence names it. A set whose files are no longer what was checked is refused before the target is contacted, or, if a file changes while it is being sent, when the target asks for it: not a byte of the changed file is sent. A set that reads is structurally valid; the `findings` of `media_set_checked` are where it departs from the standard's recommendations, and they do not stop an upload.

## The operation record

With `--record` the operation record, one JSON file, is written when the operation ends, and it is written whatever the outcome. It is what the window's receipt and log show, as data, and a program that keeps, signs or compares records can read it: [The operation record](file-reference.md#the-operation-record) has its keys, and their order is fixed. The same events write the same bytes. Its text is English, its times are UTC (`2026-11-19T10:15:30Z`, or `null` where the platform has no wall clock), and every key is there in every record, `null` or empty where it does not apply.

The record is written under its name with `.part` after it and renamed, so a reader never sees half of one. The folder it goes into has to be there. A record that cannot be written is `local_error`, exit code 5, in the `finished` event; the reason the operation had ended with is in its `detail`. For an upload that stopped at an unreadable media set the record's `media_set` holds the path and an empty part number and 0 members.

## `--scenario`: faults on purpose

The loader can be made to misbehave at chosen moments and the same way every run, so that a supplier of a target can see how the target reacts: a datagram of the loader's is dropped, repeated, delayed, held back, damaged or replaced, or the loader goes silent or aborts. The faults are done to the datagrams the loader sends, by a small interface of the engine (`adl::arinc615a::FaultHook`, [the engine](arinc615a-engine.md#the-fault-hook)); without `--scenario` the engine has no hook and an operation, its events and its record are what they are without this section. [The loader's scenario file](file-reference.md#loader-scenario-file) is the file, the sibling of the one `adl_mock --scenario` reads, and says what each action does.

- **The file is read first.** Before a port or a folder is opened or an event is written, the program reads the file (at most 256 KiB, through the platform layer, so the name may hold any character) with the strict JSON reader of the licence library and checks it as [the file's page](file-reference.md#when-the-file-is-refused) says. A file that cannot be read or is refused ends the program with exit code 5 and one sentence on standard error, `adl_load: --scenario <file>: <why>`, and nothing on standard output. A file that is read but names a media set that cannot be read is exit code 3 as usual.
- **`started`** has `scenario` (the path as it was given), `scenario_steps` and `scenario_seed` (a number, or `null`).
- **`scenario_step`** is written when a step acts, before the loader's own events of what follows. `step` is the number of the step in the file, from 1. `trigger` and `at` say what put it in effect: `request` (the target's nth request), `status_file` (the nth status file it started to write) or `after_ms` (milliseconds since the loader's first datagram). `operation` is `information`, `upload`, `download_media_defined` or `download_operator_defined`. `action` is the file's word. `applied` is `true`: the event is written when the step acts, and a chance that said no writes nothing. `datagram`, `file`, `block` and `to` say which datagram was acted on (the kind, the file its transfer is for, the block of a `data` or `ack` datagram, and `address:port`); each is `null` where it does not apply, and all four are for `abort` and `silent`, which act on the operation. The parameter of the action follows, under the key the action has for it: `copies`, `ms` (delay), `within_ms` (reorder), `for_ms` (silent, `null` for a silence of the rest of the operation), `delta` (a negative number for a shorter datagram), `code` (error), `seconds` (wait), `status` (abort_message and abort, as a decimal number); drop, corrupt and check_value have none.
- **`finished`** has `scenario_unused`: the numbers of the steps that were in effect for this operation and never acted, a list that is empty when every one did. A step for another operation (`"operation": "upload"` in an Information run) is not in it.
- **The commands never say that a target passes or fails.** They say what the loader did and how the operation ended (`reason`, `status_code`); what the target did about it is for a program that reads both ends, such as a conformance kit.
- **The same events every time.** One scenario file against one target gives the same `scenario_step` events (without `t`, and without the port in `to`, which a target chooses) from run to run, for triggers that count requests and status files, because every chance and every range is drawn from a stream of the file's `seed`. A trigger in milliseconds is as exact as the clock and the target. `serve` takes no scenario: a `run` request with `--scenario` is refused.
- **The events number stays 1.** An event and keys are added; none is removed or changed, and they appear only with `--scenario`.

## `serve`: one loader, several targets

Every `adl_load` listens on the loader's port, and a real target writes to port 59, so two of them on one computer cannot both have it: of operations started at once, all but the first end with `local_error`, "the loader's port could not be opened (address in use)". `adl_load serve` is one process that holds the port once and runs operations on several targets at once from it. It is a seam for another program to drive. It has no plan, no retries, no schedule and no report: it runs the operations it is asked for and says what happened. The user's guide has its sentences ([`adl_load serve`](user-guide/command-line.md#adl_load-serve-one-loader-several-targets)); this page says how it works.

```text
adl_load serve [--local-address <ipv4>] [--control-port <n>] [--max-operations <n>]
```

It opens the control port, writes `started`, and reads requests from standard input until it is told to stop or the input ends. The output is always JSON lines. `--local-address` is the interface for the control socket and for every transfer socket; the targets are not known when the port is opened, so unlike one operation it does not choose loopback for a target on this computer, and without the option it listens on every interface. `--max-operations` is how many operations run at once, 1 to 64 and 8 by default. `--json` is accepted and changes nothing.

**One address has one operation at a time.** That is what tells the operations apart. The loader library's `OperationPool` (`adl/loader_core/operation_pool.hpp`) holds the control socket and gives each datagram on it to the operation whose target sent it. Each operation is its own engine object, with its own workspace and its own transfer sockets on ports of the system's choosing. A datagram from an address that has no operation is dropped and counted, which is what a loader with one operation does with a datagram that is not from its target. A mock target sends from ports of its own choosing, so two on one address collide; each target of a rig needs an address of its own, which on one computer is `127.0.0.2`, `127.0.0.3` and so on.

**Requests** are lines of UTF-8 on standard input, ended by a line feed (a carriage return before it is accepted), with the fields separated by one tab. A field is taken as it is, with no quoting, so a path may have spaces; it cannot hold a tab or the end of a line. They are lines of arguments and not JSON so that the command needs no JSON reader. No code page touches the bytes: a line is taken as UTF-8 as it arrives, and a byte that is not UTF-8 refuses the line. (On Windows standard input is a text stream, so a byte 0x1A, which UTF-8 text never holds, would end the input.)

| Request | Fields | What it does |
| --- | --- | --- |
| `run` | `run`, an id, then a command and its arguments as above, one argument in a field | Starts the operation. `information`, `upload` and `download` only; `--local-address`, `--control-port`, `--json`, `--help` and `--version` are not taken, because they belong to the program. `find` is not a request. `--record` works as above, with a file for each operation. |
| `cancel` | `cancel`, an id | Aborts that operation from this side. It ends with the reason `aborted` once the target has confirmed it. |
| `stop` | `stop` | No new operation is taken. The ones that are running finish, and the program ends. The end of the input does the same. After a `stop` a `run` is refused, a `cancel` is still taken (a caller that is told to end has to be able to end what is running, which may be a target in a long WAIT), and a second `stop` changes nothing. |

An id is 1 to 64 of `A` to `Z`, `a` to `z`, `0` to `9`, `.`, `_` and `-`, the caller's own, and it may be used again once its operation has finished. A line holds at most 1048576 bytes. A last line that has no line feed was perhaps cut short, and is refused, not run.

**Events.** The first is `started`, with `program`, `version`, `events`, `command` (`serve`), `address`, `control_port` and `max_operations`; it is written once the port is held, so it means that requests are being taken. The last is `stopped`, with `reason` (`stop`, `end_of_input` or `error`) and `operations`, the count that were started. In between:

| `event` | When | Keys beside `event` and `t` |
| --- | --- | --- |
| an event of an operation | From the operation's `started` to its `finished` | Those of that event above, with `id` after `t` |
| `request_refused` | A request that was not done | `id` (`null` when the line had none that could be read), `request` (`run`, `cancel` or `stop`, or `null` when the line could not be read that far), `line` (the number of the input line, from 1), `detail` |

The events of one id are what one `adl_load` writes for that operation, with `id` added, and every `run` is answered by exactly one `finished`, or by one `request_refused` whose `request` is `run` (a refused `cancel` has `cancel` there, so that a `cancel` that came just after its operation finished is not taken for a refused `run` of the same id). The `exit_code` of a `finished` is the code `adl_load` would have ended with for that operation alone. Each event is written as a whole line, at once, from one thread, so lines of different operations never mix.

A `run` is refused when its arguments are wrong, when its id is in use, when its target's address is in an operation, when `--max-operations` operations are running, and after `stop`. Wrong usage that only the media set shows is a refusal of the `run` as well, and nothing of that operation is written. A `cancel` for an id that is not running is refused. A line that cannot be read is refused: empty, not UTF-8, with a NUL, naming no request, over 1048576 bytes, or the last one with no line feed.

An operation that ends, however it ends, does not end the program: each outcome is in its `finished`. The exit codes are 0 (it stopped as asked), 2 (wrong usage), and 5 (the control port could not be opened, the requests could not be read, or standard output could not be written). A port that cannot be opened is told on standard error, with no event, because `started` means the port is held; when the port is in use the sentence says to give `--local-address`, since listening on every interface is refused when anything holds the port on one of them (on Linux, simulated targets on port 59 on the same computer) and listening on one interface is not.

The loop reads requests, steps the pool for 20 milliseconds, and writes what finished, on one thread. Standard input is read on a thread of its own, since reading blocks, and what it reads is handed over under a lock. So a request is taken at most about 20 milliseconds late. After a `stop` the pipe may still be open and the thread blocked in its read, which nothing can end: the program writes and flushes its last event and ends the process there (`quick_exit`), since leaving the usual way would wait for that thread on some systems. 

**Starting an operation does not hold up the ones that are running.** An upload's media set is read and checked away from the loop, because a set of a gigabyte takes longer than the 13 seconds that a target waits for the loader's next word (checking one of 1 GB took 17 seconds in a release build on the development machine). A `Preparer` (`apps/load/include/adl/load/preparer.hpp`) takes the work of a request, runs it on a thread of its own, one request at a time and in the order they came, and gives the result back to the loop when the loop asks, without the loop ever waiting; the tests give an interface that works by the fake clock. The loop writes nothing for the id until its preparation ends: the id is in use, the address is taken and it counts among the operations that run at once. When it ends, the operation's `started`, `media_set_checked` and the rest are written and the operation goes to the pool, or, when the choice does not fit the set (a part number it does not hold), the `run` is refused with the number of the line it was on. A `cancel` while it is prepared is taken and remembered: the operation starts when its set has been read and ends at once with the reason `aborted`, no datagram sent, so that the caller sees the same events as for a cancel a moment later. The preparation of an operation that is not an upload is trivial and is done by the loop, as it was found. Dropping the program waits for the set that is being read, which cannot be interrupted.

A program that finds an `adl_load` from before `serve` meets wrong usage, exit code 2, when it starts `adl_load serve`; it can then fall back to one `adl_load` for each operation, and start one at a time for each place a loader listens.

## Stopping, and what it does not do

No command handles a signal. Killing `adl_load` abandons what it was doing, and a target that was in an operation times out by itself. There are no retries, no plan file, and no way to read a description from a file: one `adl_load` is one target and one operation, and a program that needs several at once runs `adl_load serve`, because two loaders on one computer cannot both have port 59.

A firewall on the computer may ask about the program the first time it listens on the network; `tools/network-access/windows/adl_firewall.ps1` makes the rule beforehand. Nothing is asked when the target is on the same computer and the loader's sockets stay on the loopback address, which is what they do by default.

The arguments are read as UTF-8 text on every system (`adl::platform::arguments`), so a folder can be named with any characters, on Windows too, and the paths in the events and the record are UTF-8.

## Examples

```sh
adl_load find --seconds 5 --json
adl_load information --target 10.0.0.2 --position HNPFMS_L --json --record info.json
adl_load upload --target 10.0.0.2 --position HNPFMS_L --media-set sets/one --all-loads --json
adl_load download --target 10.0.0.2 --position HNPFMS_L --into out --pick-all --json
```

A run of the second, with `--json`, writes lines of this kind (shortened):

```text
{"event":"started","t":0.000,"program":"adl_load","version":"0.1.1","events":1,"command":"information","target":"10.0.0.2","position":"HNPFMS_L"}
{"event":"log","t":0.000,"severity":"info","text":"Asking 10.0.0.2 (HNPFMS_L) for its configuration..."}
{"event":"configuration","t":2.001,"hardware":[{"literal_name":"FMS LEFT","serial_number":"SN-0001","parts":[...]}]}
{"event":"finished","t":4.002,"result":"completed","reason":"completed","status_code":3,"detail":"The operation completed.","exit_code":0,"record":"info.json"}
```

## Tests

The options, the events and the reasons are unit tests (`tests/load`). Whole commands are run on the fake platform against the simulated target, with no socket: an Information operation, uploads of one load and of all loads, both kinds of download, a refusal, a target that ends the operation, a load the target fails, a target that does not answer, a media set that cannot be read and one that changes, `find`, and the records, compared byte for byte with files in `tests/load/golden`. The same commands run on the real host over sockets on 127.0.0.1 against the mock target's engine on high ports (`tests/loopback`, with `--target-port` and `--control-port`, so an account that may not bind port 59 runs them too), and `serve` is run against eight mock targets at once, each on a loopback address of its own, with a cancel and with a record for each operation. The built program is started as a process for `--version`, `--help`, wrong usage and an unreadable media set. `serve` is run on the fake platform with two and with eight simulated targets at once, a cancel, each refusal, the end of the input while operations run and an operation that fails beside one that completes, and the library's pool has its own tests. `--scenario` has its own: on the fake platform (`tests/load/scenario_test.cpp`) a file with no step changes nothing but the keys it adds, a dropped request comes again and a dropped one every time ends as `no_answer`, a step that never acted is in `scenario_unused` and a step for another operation is not, each of the four operations is named as the record names it, the loader's abort ends the operation as `aborted`, the same file gives the same events twice, and a file that is not there, is not JSON, says something the loader cannot do or is too large ends with exit code 5, one sentence and no event, before the media set is read. On the real host (`tests/loopback/load_scenario_test.cpp`) the loader uploads to the mock target's engine with one action to a test (a block dropped, repeated three times, delayed, held back, changed in one byte, cut short, replaced by an ERROR, a WAIT before the file is asked for again, an ABORT in place of a status file's answer, the loader's own abort, a silence of three seconds, and a check value that is changed), and what the mock target logged and stored is looked at; a scenario file in a folder named with characters no Windows code page holds all of is read, and the same file gives the same steps twice. The fault interface and the player have their own tests ([The fault hook](arinc615a-engine.md#the-fault-hook)). The options and the request lines have [fuzz harnesses](fuzzing.md), and so has the scenario file. See [Testing](testing.md) for the labels.
