# The command adl_load

`adl_load` runs one ARINC 615A operation on one target, or looks for targets, with no window. It is for scripts and for other programs: what it prints is the same kind of thing the loader's log and receipt show, as lines of JSON that a program can read, and its exit code says how it ended. The loader's window ([the application](app.md)) does the same work for a person.

It has no Qt and no window, and it is English only: nothing it writes is translated, whatever language the window is in. The source is in `apps/load`; the library it is a thin `main` over (`adl_load_cli`) is not installed. The work of an operation is `adl::loader_core` ([Using the libraries](libraries.md)).

## Usage

```text
adl_load find         [--seconds <n>] [--local-address <ipv4>] [--json]
adl_load information  --target <ipv4> --position <THW_ID_POS> [common]
adl_load upload       --target <ipv4> --position <THW_ID_POS> --media-set <folder>
                      (--load <part number>... | --all-loads) [common]
adl_load download     --target <ipv4> --position <THW_ID_POS> --into <folder>
                      (--file <name>... | --pick <name>... | --pick-all) [common]
adl_load --help
adl_load --version

common: [--local-address <ipv4>] [--control-port <n>] [--json] [--record <file>]
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
| `--control-port <n>` | The port the loader listens on for what the target sends, 1 to 65535; the default is 59. Anything but 59 is offered to the target in the `port` option of the request, which matters when the target is on this computer and holds 59 itself. |
| `--json` | Standard output carries JSON lines and nothing else. |
| `--record <file>` | Write the operation record when the operation ends, whatever its outcome. `find` writes no record. |

Wrong options are told on standard error, with the usage after them, and the exit code is 2; no event is written, not even `started`. That includes what only the media set shows: a part number it does not hold, or `--all-loads` for a set with no load.

The target is always reached at port 59, the standard's. A target that listens elsewhere cannot be reached with this program. On Linux and macOS a program may need privileges to bind a port below 1024; the loader's own port (`--control-port`) is for that, and the target's is not the program's to choose.

## What the output is

Standard output carries the result and standard error carries wrong usage and nothing else. Without `--json` each event is one plain line for a person to read; its words are not a format to depend on. With `--json`:

- One object on a line, UTF-8, ended by a line feed, and written out and flushed at once, so that a reader sees each event when it happens. On Windows a carriage return may come before the line feed; a reader accepts both. Text that a target sent and that is not valid UTF-8 is written with U+FFFD for each byte that is not.
- `event` is the first key of every event and `t` the second: the seconds since the program started, as a number with milliseconds (`12.345`).
- The first event is `started`. It carries the program's name and version and `events`, the number of this format, which is 1. A reader refuses a number it does not know.
- The last event is `finished`, and it is written once. Its `exit_code` is the exit code of the process.
- A reader ignores a key it does not know. A key can be added without changing the number; removing a key, renaming one or changing what one means changes it.

| `event` | When | Keys beside `event` and `t` |
| --- | --- | --- |
| `started` | First | `program`, `version`, `events`, `command`, and for an operation `target` and `position` |
| `target_found` | `find`, once for each answer, as it comes | `address`, `hardware_id`, `type_name`, `position`, `literal_name`, `manufacturer_code`, `thw_id_pos` |
| `media_set_checked` | `upload`, after the check | `path`, `part_number`, `members`, `loads` (the count), `findings` (a list of sentences) |
| `log` | Any time | `severity` (`info`, `note`, `warning` or `error`), `text` |
| `configuration` | `information`, when the target's list is read | `hardware`: a list of `literal_name`, `serial_number` and `parts`, a list of `part_number`, `amendment`, `designation` |
| `progress` | `upload`, on each status file from the target | `percent`, `loads`: a list of `part_number`, `header_file`, `status`, `percent` |
| `file_list` | `download` with `--pick` or `--pick-all`, when the target's list is read | `files`: a list of `name`, `description` |
| `file_received` | `download`, once for each file | `name`, `size`, `path` |
| `finished` | Last | `result`, `reason`, `status_code`, `detail`, `exit_code`, and where they apply `count` (`find`), `download_folder` (a download), `record` (when `--record` was given) |

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
| `local_error` | A port could not be opened, a folder could not be made, or the record could not be written | 5 |

An upload reads and checks the media set before it asks the target anything, as the window does. A set that cannot be read gives `started`, a `log` event with the reason and `finished`, with no `media_set_checked`. A set whose files are no longer what was checked is refused before the target is contacted, or, if a file changes while it is being sent, when the target asks for it: not a byte of the changed file is sent. A set that reads is structurally valid; the `findings` of `media_set_checked` are where it departs from the standard's recommendations, and they do not stop an upload.

## The operation record

With `--record` the operation record, one JSON file, is written when the operation ends, and it is written whatever the outcome. It is what the window's receipt and log show, as data, and a program that keeps, signs or compares records can read it: [The operation record](file-reference.md#the-operation-record) has its keys, and their order is fixed. The same events write the same bytes. Its text is English, its times are UTC (`2026-11-19T10:15:30Z`, or `null` where the platform has no wall clock), and every key is there in every record, `null` or empty where it does not apply.

The record is written under its name with `.part` after it and renamed, so a reader never sees half of one. The folder it goes into has to be there. A record that cannot be written is `local_error`, exit code 5, in the `finished` event; the reason the operation had ended with is in its `detail`. For an upload that stopped at an unreadable media set the record's `media_set` holds the path and an empty part number and 0 members.

## Stopping, and what it does not do

No command handles a signal. Killing `adl_load` abandons what it was doing, and a target that was in an operation times out by itself. There are no retries, no plan file, and no way to read a description from a file: one process is one target and one operation, so a program that needs more starts as many as it needs.

A firewall on the computer may ask about the program the first time it listens on the network; `tools/network-access/windows/adl_firewall.ps1` makes the rule beforehand. Nothing is asked when the target is on the same computer and the loader's sockets stay on the loopback address, which is what they do by default.

On Windows an argument that holds a character outside the system's code page (a folder name, for one) cannot be given.

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

The options, the events and the reasons are unit tests (`tests/load`). Whole commands are run on the fake platform against the simulated target, with no socket: an Information operation, uploads of one load and of all loads, both kinds of download, a refusal, a target that ends the operation, a load the target fails, a target that does not answer, a media set that cannot be read and one that changes, `find`, and the records, compared byte for byte with files in `tests/load/golden`. The same commands run on the real host over sockets on 127.0.0.1 against the mock target's engine (`tests/loopback`; they need port 59 of the loopback address and are skipped where an account may not bind it), and the built program is started as a process for `--version`, `--help`, wrong usage and an unreadable media set. The options have a [fuzz harness](fuzzing.md). See [Testing](testing.md) for the labels.
