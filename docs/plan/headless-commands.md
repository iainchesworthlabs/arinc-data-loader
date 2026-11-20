# The commands without a window: their contract

This is a plan. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). None of these commands is built yet. It belongs to [the Pro features plan](pro-features.md): it writes down, before they are built, what the public commands of packages C2, C3 and C4 take and give. The Pro tools are written against this page at the same time as the commands are, so the two have to agree on it first.

Written 2026-11-19. The owner agreed to the programs, their options, their output and their exit codes on that day ([decision D](pro-features.md#10-decisions-for-the-owner)); this page is the detail. Once a command is built, the guide's command-line page is the source of truth and this page is history.

## 1. Rules for every command

- **No window and no Qt.** The text is English and is not translated.
- **Options** are written `--name value`. An option that takes several values is given several times (`--load A --load B`), never as a list with commas. `--help` prints the usage and exits 0. `--version` prints one line, `<program> <version> events <n>`, and exits 0. The program's name is written without `.exe` on every system.
- **Standard output** carries the result. **Standard error** carries usage errors and anything that is not a result.
- **`--json`**: standard output carries JSON lines and nothing else. One object on a line, UTF-8, ended by a line feed, and written out at once so that a reader sees each event when it happens. On Windows a carriage return may come before the line feed. A reader accepts both.
- **Every event** has `"event"` as its first key and `"t"`, the seconds since the program started, as a number with milliseconds.
- **The first event** is `started`. It carries `"program"`, `"version"` and `"events"`, the number of this events format, which is 1. A reader refuses a number it does not know.
- **Growth.** A reader ignores a key it does not know. A key can be added without changing the number. Removing a key, renaming one or changing what one means changes the number.
- **Exit codes.** 0 is success, 1 is "it ran and the outcome was a failure", 2 is wrong usage. Each program lists the rest.
- **Stopping.** No command handles a signal. Killing one abandons what it was doing, and a target that was in an operation times out by itself.
- **Addresses** are dotted IPv4. **Folders and files** are paths as the operating system writes them.
- **Arguments are Unicode on every system.** On Windows a command reads its arguments from the wide command line, through the platform layer, and not from `argv`, which carries them in the code page and loses any character outside it. A path given as an argument works whatever characters it has.

## 2. `adl_load`: one operation on one target

Package C3. It reads and checks a media set before it sends anything, as the window does, and refuses a set that cannot be read.

```text
adl_load find         [--seconds <n>] [--local-address <ipv4>] [--json]
adl_load information  --target <ipv4> --position <THW_ID_POS> [common]
adl_load upload       --target <ipv4> --position <THW_ID_POS> --media-set <folder>
                      (--load <part number>... | --all-loads) [common]
adl_load download     --target <ipv4> --position <THW_ID_POS> --into <folder>
                      (--file <name>... | --pick <name>... | --pick-all) [common]

common: [--local-address <ipv4>] [--control-port <n>] [--target-port <n>] [--json] [--record <file>]
```

- `--target-port` is the port the target listens on for the request that starts an operation. The default is 59, which is the standard's. Another port is for a simulated target that cannot have 59, on a system that keeps low ports for privileged programs. A real target is on 59. It does not make two simulated targets on one address work: each still sends from ports of its own choosing ([section 4](#4-adl_mock-the-mock-target-as-a-command)).

- `--file` is the download in which the loader names the files. `--pick` and `--pick-all` are the download in which the target lists its files and the command chooses from the list; a `--pick` name that the target does not list is a failure.
- `--local-address` defaults to what the window does: the loopback address when the target is on this computer, every interface otherwise. `--control-port` defaults to the window's default.
- `--seconds` is how long `find` listens. The default is the window's three seconds.
- `--record` writes the operation record of [section 3](#3-the-operation-record) when the operation ends, whatever its outcome. `find` writes no record.

**Events**

| `event` | When | Keys beside `event` and `t` |
| --- | --- | --- |
| `started` | First | `program`, `version`, `events`, `command`, and for an operation `target` and `position` |
| `target_found` | `find`, once for each answer | `address`, `hardware_id`, `type_name`, `position`, `literal_name`, `manufacturer_code`, `thw_id_pos` |
| `media_set_checked` | `upload`, after the check | `path`, `part_number`, `members`, `loads` (count), `findings` (list of sentences) |
| `log` | Any time | `severity` (`info`, `note`, `warning`, `error`), `text` |
| `configuration` | `information`, when the target's list is read | `hardware`: list of `literal_name`, `serial_number`, `parts`: list of `part_number`, `amendment`, `designation` |
| `progress` | `upload`, on each status from the target | `percent`, `loads`: list of `part_number`, `header_file`, `status`, `percent` |
| `file_list` | `download` with `--pick` or `--pick-all`, when the target's list is read | `files`: list of `name`, `description` |
| `file_received` | `download`, once for each file | `name`, `size`, `path` |
| `finished` | Last | `result`, `reason`, `status_code`, `detail`, `exit_code`, and where they apply `count` (`find`), `download_folder`, `record` |

`result` is `completed` or `failed`. `reason` is one short word for a program to read; `detail` is one English sentence for a person. `status_code` is the last ARINC 615A status code the target reported, as a decimal number (4096 for 0x1000), or `null`. It includes the target's answer to the request that starts the operation, so a refused operation carries the refusal's code.

The `status` of a load is the name of its status in lower case with underscores (`in_progress`, `completed` and so on), the same word in every language and every version. It is not a sentence.

| `reason` | Meaning | Exit code |
| --- | --- | --- |
| `completed` | The operation completed, and for an upload every load did | 0 |
| `target_refused` | The target refused the operation | 1 |
| `target_aborted` | The target ended the operation | 1 |
| `load_failed` | The upload ended and the target failed at least one load | 1 |
| `missing_files` | The download ended with fewer files than were asked for, or a file failed | 1 |
| `protocol_error` | What the target sent could not be understood, or its protocol version is not supported | 1 |
| `aborted` | The operation was aborted from this side. `adl_load` has no way to do that; a record written by a window can carry it | 1 |
| `usage` | Wrong options (no events are written) | 2 |
| `media_set_unreadable` | The media set could not be read or checked | 3 |
| `media_set_changed` | A file changed after the set was checked | 3 |
| `no_answer` | The target did not answer | 4 |
| `local_error` | A port could not be opened, a folder could not be made, or the record could not be written | 5 |

`find` exits 0 when it ran, with `count` 0 when no target answered.

**The loader's port and the target's.** The loader tells the target which port to write to, in the TFTP `port` option of its request, whenever the target could not otherwise know: when `--control-port` is not 59, and when `--target-port` is not 59. A loader and a target that are both on 59 exchange no option. Found on 2026-11-20: without the second case, `--target-port 5059` alone timed out against a simulated target on 5059, which wrote to the loader's 5059 while the loader listened on 59.

### 2.1 `adl_load serve`: one loader, several targets

Package C10. The owner agreed to it, and to this detail, on 2026-11-20. It is being built and is not finished.

**Why.** Every `adl_load` listens on the loader's port. Two of them on one computer cannot both have port 59 on one address, and a real target writes to 59. Found on 2026-11-20 with the built commands: of three operations started at once, two ended with `local_error`, "the loader's port could not be opened (address in use)". So operations on several targets at once need one program that holds the port and tells the targets apart. `serve` is that program. It is a seam for another program to drive. It has no plan, no retries, no schedule and no report: it runs the operations it is asked for and says what happened.

```text
adl_load serve [--local-address <ipv4>] [--control-port <n>] [--max-operations <n>] [--json]
```

- It opens the control port once. Then it reads requests from standard input and writes events to standard output. The output is always JSON lines; `--json` is accepted and changes nothing.
- `--local-address` defaults to every interface. The rule of section 2, loopback for a target on this computer, cannot apply, because no target is known when the port is opened. For simulated targets on this computer give `--local-address 127.0.0.1`.
- An operation is told from another by the address its datagrams come from, so one address has one operation at a time. A datagram from an address that has no operation is dropped and nothing is sent back, as one `adl_load` does today.
- `--max-operations` is how many operations run at once, 1 to 64. The default is 8.
- Starting an operation does not hold up the ones that are running. A media set is read and checked away from the loop that serves the targets, because a large set takes longer to read than a target waits.

**Requests.** One request is one line of UTF-8 ended by a line feed, and a carriage return before the line feed is accepted. Its fields are separated by one tab character. A field is taken as it is, with no quoting, so a path may have spaces. A field cannot hold a tab or the end of a line.

| Request | Fields | What it does |
| --- | --- | --- |
| `run` | `run`, an id, then a command and its arguments as [section 2](#2-adl_load-one-operation-on-one-target) writes them for `information`, `upload` and `download`, one argument in a field | Starts the operation |
| `cancel` | `cancel`, an id | Aborts that operation from this side. It ends with the reason `aborted` |
| `stop` | `stop` | No new operation is taken. Those that are running finish, and the program ends. The end of the input does the same |

- An id is 1 to 64 of the characters `A` to `Z`, `a` to `z`, `0` to `9`, `.`, `_` and `-`. It is the caller's, and it may be used again once its operation has finished.
- `--local-address`, `--control-port` and `--json` are not taken in a request, because they belong to the program. Neither are `--help`, `--version`, `find` and `serve`.
- `--record` works as in section 2, with one file for each operation.
- After `stop`, a `run` is refused. A `cancel` is still taken, so that a caller can end what is running. A second `stop` changes nothing.
- A line is at most 1 MiB. A longer one is refused and skipped. A last line that the input ends in the middle of, with no line feed, is refused, so a `run` that was cut off never starts.
- The requests are lines of arguments, and not JSON, so that the command needs no JSON reader. The repository has none until the licence work brings one.

An example, with `→` standing for a tab:

```text
run→fms-left.1→upload→--target→192.168.10.21→--position→HNPFMS_L→--media-set→E:\sets\FMS STD14→--all-loads→--record→E:\runs\fms-left.1.json
```

**Events**

- The first event is `started`, with `program`, `version`, `events`, `command` (`serve`), `address`, `control_port` and `max_operations`. It is written once the control port is open, so it means that requests are being served. The last is `stopped`, with `reason` (`stop`, `end_of_input` or `error`) and `operations`, the count that were started.
- An event with an `id` belongs to that operation. The events of one id are the events section 2 gives for its command, from `started` to `finished`, each with `id` after `t`. `t` is the seconds since the program started. The `exit_code` of a `finished` is the code `adl_load` would have ended with had it run that operation alone. A `finished` is written when the operation's engine has nothing left to do, and its address is in use until then.
- `request_refused` is a request that was not done. It has `id` (`null` when the line had none that could be read), `request` (`run`, `cancel` or `stop`, or `null` when that could not be read), `line` (the number of the input line) and `detail`. A `run` is refused when its arguments are wrong, when its id is in use, when its target's address is in an operation, when `--max-operations` are running, and after `stop`. Wrong arguments include those that only show once the media set is read, such as a part number it does not hold: one `adl_load` calls that wrong usage and writes no events, and so does this. A `cancel` is refused when no operation has its id. A line that cannot be read is refused.

**Exit codes:** 0 it stopped as asked, whatever happened to the operations, because each one's outcome is in its `finished`. 2 wrong usage. 5 the control port could not be opened, or standard output could not be written. When the port cannot be opened there are no events: one sentence on standard error, which says to give `--local-address` when the port is in use.

**A caller that finds no `serve`.** An `adl_load` from before this command answers `adl_load serve` with wrong usage, exit code 2. A caller then falls back to one `adl_load` for each operation, and runs one at a time for each place a loader listens.

## 3. The operation record

Package C2. One JSON file for one operation. It is what the window's receipt and log show, as data. A Pro tool keeps, signs and compares these files, so the format is fixed here.

- **Encoding:** UTF-8, no byte order mark, lines ended by a line feed, two spaces of indentation, keys in the order of the table below, and a line feed at the end. Two runs with the same events write the same bytes.
- **Text is English**, whatever language a window is in.
- **Times** are UTC, written `2026-11-19T10:15:30Z`. A time is `null` where the platform has no wall clock.
- A key that does not apply to the operation is present and is `null` or an empty list, so that every record has the same keys.

| Key | Value |
| --- | --- |
| `format` | 1 |
| `kind` | `adl-operation-record` |
| `program` | `name` and `version` of the program that wrote it |
| `operation` | `information`, `upload`, `download_media_defined` or `download_operator_defined` |
| `target` | `address` and `position` (the `THW_ID_POS`) |
| `started_at`, `ended_at` | Times |
| `result`, `reason`, `status_code`, `detail` | As in the `finished` event |
| `media_set` | For an upload: `path`, `part_number`, `members`. Otherwise `null` |
| `loads` | For an upload: list of `part_number`, `header_file`, `status`, `percent` |
| `configuration` | For an Information operation: the list of hardware items that the `configuration` event carries as `hardware`, written as an array. `null` otherwise, and when the target's list never arrived |
| `files` | For a download: list of `name`, `size`, `path` |
| `download_folder` | For a download: the folder the files were written into, or `null` when none was kept |
| `licence` | `null` when the program that wrote the record says nothing. `adl_load` writes `edition` (`community`, `pro` or `enterprise`), `state` and `number` (`null` unless a licence is in force) of the licence in force when the operation started, as the licensing plan's slice 5b says |
| `log` | List of `at` (a time), `severity`, `text` |

Every value in `configuration` is text: `literal_name`, `serial_number`, `part_number`, `amendment` and `designation` are strings, and one the target did not give is an empty string. `parts` is a list, empty when the hardware reports none.

A reader refuses a `format` it does not know and ignores a key it does not know.

## 4. `adl_mock`: the mock target as a command

Package C4b. One simulated target in one process. Its options, events and exit codes were worked out in that package's notes and are repeated here so that a rack file can be mapped onto them.

| Option | Meaning | Default |
| --- | --- | --- |
| `--hardware-id <text>`, `--position <text>` | The target's identity; its `THW_ID_POS` is the two joined by `_` | `HNPFMS`, `L` |
| `--type-name <text>`, `--literal-name <text>`, `--manufacturer <text>` | The rest of the FIND answer | `FMS`, `FMS LEFT`, `HNP` |
| `--part <hardware>:<part number>[:<amendment>[:<designation>]]`, repeatable | The hardware and parts it reports | The engine's own list |
| `--holds <folder>` | The files it holds for a download | One demonstration file |
| `--storage <folder>` | Where uploads are stored | None: uploads are served and not kept |
| `--address <ipv4>` | The local address | `127.0.0.1` |
| `--tftp-port <n>`, `--find-port <n>` | Ports | 59, 1001 |
| `--think-time-ms <n>`, `--status-period <seconds>` | Pace | 1000, 4 |
| `--wait-first <seconds>`, `--wait-every <seconds>` | WAIT on the first request, or on every request | Off |
| `--refuse <information, upload, download-media or download-operator>` | Refuse that operation | Off |
| `--refuse-code <0x1000 or 0x1002>` | With which status | `0x1000` |
| `--invalid-init-file` | Serve an initialization file of an invalid structure | Off |
| `--drop-status-every <n>` | Lose every nth status file | Off |
| `--corrupt-served` | Flip a bit in the files it serves | Off |
| `--wrong-checksum` | A wrong check value option | Off |
| `--abort-after <n>` | Abort after n status files | Off |
| `--silent` | Say nothing after the first status file. Needs `--seconds`, because the operation never ends | Off |
| `--operations <n>`, `--seconds <n>` | Stop after n operations (refused ones count), or after n seconds | Run until killed |
| `--json` | JSON lines | Plain lines |

**Limits.** A value outside them is wrong usage (exit code 2). They were read from the built command on 2026-11-20, and a rack file may not allow more than they do.

| Option | Takes |
| --- | --- |
| `--hardware-id` | 1 to 15 printable ASCII characters |
| `--position` | 0 to 8 printable ASCII characters |
| `--type-name` | 1 to 8 printable ASCII characters |
| `--literal-name` | 1 to 20 printable ASCII characters |
| `--manufacturer` | 1 to 3 printable ASCII characters |
| `--tftp-port`, `--find-port` | 1 to 65535 |
| `--think-time-ms` | 0 to 60000 |
| `--status-period` | 0 to 3600 |
| `--wait-first`, `--wait-every` | 1 to 65535 |
| `--drop-status-every`, `--abort-after` | 1 to 1000000 |
| `--operations` | 1 to 1000000000 |
| `--seconds` | 1 to 31536000 |

| `event` | Keys beside `event` and `t` |
| --- | --- |
| `started` | `program`, `version`, `events`, `thw_id_pos`, `address`, `find_port`, `tftp_port`, `faults` (the fault options that are on) |
| `operation_began` | `operation` |
| `progress` | `operation`, `percent` |
| `upload_received` | `part_number`, `files` (names), `stored_in` (a folder, or `null`) |
| `operation_ended` | `operation`, `status` (a number, or `null` for an invalid initialization file), `status_name`, `refused` |
| `stopped` | `reason` (`operations`, `seconds`, `error`), `operations` (count) |

- `operation` is `information`, `upload`, `download_media_defined` or `download_operator_defined`, as in the operation record.
- `status_name` is the name of the status code in lower case with underscores, or `invalid_initialization_file` when there is no code.
- `faults` lists the names of the fault options that are on, without their values.
- `--silent` without `--seconds` is wrong usage.
- Several targets on one computer each need an address of their own (`127.0.0.2`, `127.0.0.3`): a target sends from ports of its own choosing, and two on one address collide.

Exit codes: 0 stopped as asked, 1 the run loop failed, 2 wrong usage, 3 a folder cannot be used, 4 a port cannot be opened.

## 5. `adl_make_set`: the generator as a command

Package C6. Not specified yet. It waits for the licence work's strict JSON reader, and its description file is designed with it.

## 6. What the Pro tools rely on

- The `events` number and the rule that unknown keys are ignored.
- `finished` being the last event, and the exit code agreeing with its `exit_code`.
- `reason` being one of the words in the table of section 2.
- The operation record's keys, and its bytes being the same for the same events.
- The `configuration` of an Information operation being in the record: the configuration audit reads it from there.
- One process being one target or one operation, so that a Pro tool can start as many as it needs. For operations on several targets at once it is one `adl_load serve` ([section 2.1](#21-adl_load-serve-one-loader-several-targets)), because two loaders cannot share the port.
- In `serve`: the events of one id being what one `adl_load` would have written for that operation, with `id` added, and exactly one `finished` or one `request_refused` whose `request` is `run` for every `run`. A `request_refused` whose `request` is `cancel` says nothing about the operation: a `cancel` can arrive just after its operation finished.
