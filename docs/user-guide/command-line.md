# Command line and exit codes

Two of the programs are run from a command line: `adl_write_payloads`, which writes sample media sets, and `adl_load`, which runs one operation on one target. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

## `adl_write_payloads`

It writes the sample media sets of the libraries to a folder, so that you can try another loader on them, and reads each one back and verifies it. It is in the release packages beside the other programs ([Installing](installing.md#getting-the-programs)).

```text
usage: adl_write_payloads [--scenario <name>]... <output-directory>
       adl_write_payloads --list

Writes sample ARINC 665 media sets to <output-directory>/<scenario>/member_NNN.
With no --scenario, every scenario is written. A scenario directory that exists
and has files in it is never overwritten.
```

| Argument | What it does |
| --- | --- |
| `<output-directory>` | The folder to write into. It is made if it does not exist. Each scenario goes into a folder of its own, named for the scenario, with one `member_NNN` folder for each member. Give it once. Its name can have any characters in it, on Windows as elsewhere (accented letters, Japanese, an emoji). The program prints the folder as UTF-8, which a Windows console shows correctly when it is set to UTF-8 (`chcp 65001`). |
| `--scenario <name>` | Write only this scenario. It can be given more than once, and a scenario named twice is written once. |
| `--list` | Print the name and a one-line description of every scenario, and nothing else. It takes no other argument. |

With no `--scenario`, every scenario is written.

### The scenarios

`--list` prints this (the same four scenarios are described in [Files the programs read and write](../file-reference.md#scenarios)):

```text
two-member
    two members, a load across both, a batch and a stray file; CRC-32 and SHA-1 check values
arinc615
    an ARINC 615 part with a directory for each disk (Appendix F-1)
boeing777
    a Boeing 777 load with DISK.DIR and NON_LOAD.CRC (Appendix F-2)
arinc615-boeing777
    a load both kinds of loader can use (Appendix F-3)
```

### An example

This was run on Windows with the Release build, in a folder with no `samples` folder in it:

```text
> adl_write_payloads --scenario two-member --scenario boeing777 samples
two-member: 2 members, 12 files, 1 load, 1 batch, 1 finding -> samples\two-member
    2.2.3.2: the data file DATA.LUP is not a whole number of 16-bit words
boeing777: 1 member, 9 files, 1 load, 0 batches, 2 findings -> samples\boeing777
    2.1.3: the check characters are not the ones worked out from the rest of the part number
    2.1.3: the check characters are not the ones worked out from the rest of the part number
> echo %ERRORLEVEL%
0
```

For each scenario the program prints one line:

```text
<name>: <n> members, <n> files, <n> loads, <n> batches, <n> findings -> <folder>
```

A count of one is printed as `1 member`, `1 file`, `1 load`, `1 batch` or `1 finding`. After the line comes one indented line for each *finding*: the standard's clause and what departs from its recommendations. The samples have findings on purpose, because some of them follow the standard's own examples (Appendix F), which do not satisfy every recommendation. A finding is a warning. It does not make the program fail.

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Every scenario asked for was written, read back and verified. `--list` also ends with 0. |
| 1 | A scenario could not be built, written, read back or verified. The scenarios before it were written. |
| 2 | The command line could not be used. Nothing was written. The usage text is printed after the reason, on the error output. |

### Messages

The program prints errors on the error output, and the report and the list on the standard output.

| Message | Exit code | What to do |
| --- | --- | --- |
| `--scenario needs a name` | 2 | Put a scenario name after `--scenario`. |
| `unknown option <option>` | 2 | The only options are `--scenario` and `--list`. |
| `more than one output directory given` | 2 | Give one folder. |
| `no output directory given` | 2 | Give the folder to write into, or use `--list`. |
| `no scenario called <name> (see --list)` | 2 | Use a name that `--list` prints. |
| `<scenario>: cannot build: <reason>` | 1 | A defect in the program: the sample could not be built. Report it ([Support](../support.md)). |
| `<scenario>: cannot write <path>: <reason>` | 1 | The folder is not writable, or a `member_NNN` folder that has anything in it is there already. The program never writes into a folder that holds something. Remove it or choose another output folder. |
| `<scenario>: cannot read back <path>: <reason>` | 1 | What was written could not be read back. Check the disk, then report it if it happens again. |
| `<scenario>: does not verify: <reason>` | 1 | The set was written and does not pass the check. A defect in the program: report it. |

An example of the second to last case, from running the same command twice:

```text
> adl_write_payloads --scenario arinc615 samples
arinc615: cannot write samples\arinc615\member_001: member directory exists and is not empty
> echo %ERRORLEVEL%
1
```

The trees it writes have not been read by another loader ([What has been checked](../status.md)).

## `adl_load`

It runs one ARINC 615A operation on one target, or looks for targets, with no window, and is for scripts and other programs: what it does is written as lines of JSON that a program can read, and its exit code says how it ended. It does what the loader's window does for one target (an Information operation, an upload, a download, FIND). It is in the release packages beside the other programs ([Installing](installing.md#getting-the-programs)). Its text is English only, in every language. [The command adl_load](../load-command.md) is the page for programmers: every event, every key of the record and how the tests hold them.

```text
usage:
  adl_load find         [--seconds <n>] [--local-address <ipv4>] [--json]
  adl_load information  --target <ipv4> --position <THW_ID_POS> [common]
  adl_load upload       --target <ipv4> --position <THW_ID_POS> --media-set <folder>
                        (--load <part number>... | --all-loads) [common]
  adl_load download     --target <ipv4> --position <THW_ID_POS> --into <folder>
                        (--file <name>... | --pick <name>... | --pick-all) [common]
  adl_load --help
  adl_load --version

common: [--local-address <ipv4>] [--control-port <n>] [--json] [--record <file>]

Runs one ARINC 615A operation on one target, or looks for targets, with no window.
Options are written "--name value"; an option that takes several values is given
several times (--load A --load B).

  find          Broadcasts a FIND request and lists the targets that answer
                (--seconds, default 3, is how long it listens).
  information   Asks the target for its configuration.
  upload        Reads and checks the media set, then sends the loads: --load names
                them by part number, --all-loads takes every load of the set.
  download      Fetches files into a new folder inside --into. --file names the files;
                --pick and --pick-all let the target list its files first and choose
                from the list (--pick-all takes all of them).

  --json        Write one JSON object per line on standard output, one for each event,
                instead of a line for a person to read.
  --record      Write the operation record, a JSON file, when the operation ends.
  --control-port, --local-address
                The port the loader listens on (default 59) and the interface its
                sockets use (default: the loopback address for a target on this
                computer, every interface otherwise).

Exit codes: 0 completed, 1 the operation ran and failed, 2 wrong usage, 3 the media set
could not be read or changed after it was checked, 4 the target did not answer, 5 a
local error (a port, a folder or the record file).
The text is English. Killing the program abandons the operation and the target times
out by itself.
```

Options are written `--name value`, with a space. An option that takes several values is given several times. A value that begins with `--` is taken to be the next option, so a folder or a name that begins that way is written `./--name`.

| Option | What it does |
| --- | --- |
| `--target <ipv4>` | The target's address, four numbers of 0 to 255 separated by dots. |
| `--position <THW_ID_POS>` | The target's `THW_ID_POS` as it names itself, for example `HNPFMS_L`: 1 to 240 characters, none of them a control character, a slash or a backslash. For a download also none that a folder name cannot hold, because the download folder is named after it. |
| `--media-set <folder>` | An upload: the folder of the media set, as the media set generator writes it. It is read and checked before the target is asked anything. A large set takes a while to read, and nothing is written until it has been read. |
| `--load <part number>` | An upload: a load of the media set, by part number. Give it once for each load. They are sent in the order given. |
| `--all-loads` | An upload: every load of the media set. |
| `--into <folder>` | A download: the folder to write into. A folder of its own, `DNLD_DATA_<THW_ID_POS>_<number>`, is made inside it. |
| `--file <name>` | A download in which you name the files. Give it once for each file. A name is a plain file name, without a path. |
| `--pick <name>` | A download in which the target lists the files it holds and the command chooses from its list. Give it once for each file to take. A name the target does not list is a failure, and the files that it does list are still taken. |
| `--pick-all` | The same, taking every file the target lists. |
| `--seconds <n>` | `find`: how long to listen, 1 to 3600 seconds. The default is three, as in the window. |
| `--local-address <ipv4>` | The interface the sockets use. By default the loopback address for a target on this computer, and every interface otherwise. |
| `--control-port <n>` | The port the loader listens on for what the target sends, 1 to 65535. The default is 59. A target on this computer holds port 59 itself, so the loader needs another. |
| `--json` | Write one JSON object on a line for each event on the standard output, and nothing else there. Without it each event is one plain line for a person to read. |
| `--record <file>` | Write the operation record, a JSON file, when the operation ends, whatever the outcome ([Files the programs read and write](../file-reference.md#the-operation-record)). The folder it goes into has to be there. `find` writes no record. |

`--help` prints the usage and `--version` prints one line, `adl_load <version> events 1`, and both end with 0. The target is always reached at port 59, the standard's.

Every event has `event` first and `t` second, the seconds since the program started. The events are `started` (first), `target_found`, `media_set_checked`, `log`, `configuration`, `progress`, `file_list`, `file_received` and `finished` (last). `finished` carries the `result` (`completed` or `failed`), the `reason`, the `status_code` the target last reported (or `null`), a `detail` sentence and the `exit_code`. The reasons are `completed`, `target_refused`, `target_aborted`, `load_failed`, `missing_files`, `protocol_error`, `aborted`, `usage`, `media_set_unreadable`, `media_set_changed`, `no_answer` and `local_error`.

Killing `adl_load` abandons the operation, and a target that was in an operation times out by itself. There is one target and one operation to a process, and no retries.

### Exit codes of `adl_load`

| Code | Reason | Meaning |
| --- | --- | --- |
| 0 | `completed` | The operation completed, and for an upload every load did. For `find`: it ran, and it may have found nothing. `--help` and `--version` also end with 0. |
| 1 | `target_refused`, `target_aborted`, `load_failed`, `missing_files`, `protocol_error`, `aborted` | The operation ran and ended in a failure: the target refused it or ended it, the target failed a load, a download ended with fewer files than were asked for, or what the target sent could not be understood. |
| 2 | `usage` | The command line could not be used. Nothing was sent and no event was written. The reason is on the error output, with the usage after it. |
| 3 | `media_set_unreadable`, `media_set_changed` | The media set could not be read or checked, or a file of it changed after it was checked. Not a byte of a changed file is sent. |
| 4 | `no_answer` | The target did not answer. |
| 5 | `local_error` | A port could not be opened, a folder could not be made, or the record could not be written. |

### Messages of `adl_load`

What the command line is refused with goes to the error output and ends with code 2. The sentence names the argument that is wrong.

| Message | What to do |
| --- | --- |
| `No command given: find, information, upload or download.` | Start with one of the four commands. |
| `The first argument has to be a command (find, information, upload or download), not <argument>.` | Put the command first. |
| `Unknown command <word>: find, information, upload or download.` | Use one of the four commands. |
| `Unexpected argument <word>: a value belongs to the option before it.` | Put the word after the option it is the value of. |
| `Unknown option <option>.` | Use an option of the command in the usage above. |
| `Unknown option <option> (write --name value, with a space).` | Write the option and its value as two arguments. |
| `<option> is not an option of <command>.` | The option belongs to another command. |
| `<option> is given twice.` | Give it once. |
| `<option> <value> is given twice.` | Name each load or file once. |
| `<option> needs a value.` | Put the value after the option. |
| `<option> takes a whole number from <low> to <high>, not <value>.` | Write the number in digits. |
| `<option> takes an IPv4 address (four numbers of 0 to 255 separated by dots), not <value>.` | Write four numbers separated by dots. |
| `--target is missing.`, `--position is missing.`, `--media-set is missing.`, `--into is missing.` | The command needs that option. |
| `--position <value> is not a THW_ID_POS: 1 to 240 characters, none of them a control character, a slash or a backslash.` | Use the target's `THW_ID_POS`. |
| `--position <value> is not a THW_ID_POS a download can be made for: 1 to 240 characters, none of them a control character, a slash, a backslash or a character a folder name cannot hold.` | The download folder is named after it, so a colon, an asterisk and the like are not allowed. |
| `--load and --all-loads cannot be given together.` | Give one of them. |
| `Name the loads to send with --load, or give --all-loads.` | An upload has to say which loads. |
| `Name the files to download with --file or --pick, or give --pick-all.` | A download has to say which files. |
| `--file, --pick and --pick-all cannot be given together.` | Give one kind of choice. |
| `The media set has no load with the part number "<part number>".` | Use a part number that the media set holds ([Choose and check a media set](choose-a-media-set.md)). |
| `The part number "<part number>" names more than one load of the media set.` | A part number has to name one load. |
| `The media set holds no load to send.` | The set has nothing to upload. |
| `"<name>" is not a safe file name.` | Name the file as the target knows it: a plain name of at most 254 characters, without a path. |

With `--json` the sentences of what happened are in the events. In a `finished` event the `detail` is one of these, and the same words are the last lines of the log in the record:

| Message | Reason | What it means |
| --- | --- | --- |
| `The operation completed.` | `completed` | An Information operation, or a download of nothing that was asked for. |
| `The operation completed. The target read <n> file(s).` | `completed` | An upload. |
| `The operation completed. Received <n> file(s).` | `completed` | A download. |
| `The target refused the operation: <how it ended> (<the target's reason>).` | `target_refused` | The target said no: `denied by the target`, with the reason it gave, or `not supported by the target`. |
| `The target ended the operation: <how it ended> (<the target's reason>).` | `target_aborted` | `aborted by the target`, or `a file transfer failed` for a transfer that the target ended. |
| `The upload ended and the target failed <n> load(s): <part numbers>.` | `load_failed` | The upload ended and at least one load did not go. |
| `Received <n> of <n> requested file(s).` | `missing_files` | A download ended with fewer files than were asked for. |
| `The target failed <n> file(s).` | `missing_files` | Added to the one above when the target said that files failed. |
| `Received <n> of <n> picked file(s). The target does not list <names>.` | `missing_files` | A name that `--pick` gave and the target does not list. The files that it lists were taken. |
| `The target listed no file to download.` | `missing_files` | `--pick-all` or `--pick` found nothing to take in the target's list. |
| `The target's protocol version is not supported.` | `protocol_error` | The target's files are not the version this loader speaks. |
| `What the target sent could not be understood: <how it ended>.` | `protocol_error` | A file of an invalid structure, or one sent out of turn. |
| `The operation was aborted: <how it ended>.` | `aborted` | Ended from this side, which `adl_load` cannot do; the window can. |
| `The target did not answer: a file transfer timed out.` | `no_answer` | A request got no reply. |
| `The target did not answer: <how it ended>.` | `no_answer` | `data load protocol time-out`, or `the target was silent longer than its exception timer`. |
| `The media set could not be read: <why>.` | `media_set_unreadable` | The folder is not there, or a file of the set is damaged or missing. |
| `The media set changed after it was checked: <file>.` | `media_set_changed` | A file was edited, replaced or removed after the check, before or while it was being sent. |
| `The download folder could not be used: <why>.` | `local_error` | The folder named is a file, or cannot be made. |
| `The operation could not run: the request was refused (<why>).` | `local_error` | The loader would not make the request. |
| `The operation could not run: the loader's port could not be opened (<code>).` | `local_error` | Another program holds the port. Choose another with `--control-port`. |
| `The operation could not run: waiting for the network failed (<code>).` | `local_error` | The network could not be waited on. |
| `The loader could not carry out the operation: <how it ended>.` | `local_error` | A file could not be read or written here. |
| `The operation record could not be written: <why>. The operation had ended with the reason <reason>.` | `local_error` | The folder of `--record` is not there, or the file cannot be replaced. The `<why>` is `cannot create <file> (<code>)`, `cannot write <file> (<code>)` or `cannot replace <file> (<code>)`. |
| `No target answered.`, `<n> target answered.`, `<n> targets answered.` | `completed` | The end of `find`. |
| `FIND could not run: the port could not be opened (<code>).` | `local_error` | Another program holds port 1001 without sharing it, or `--local-address` is not an address of this computer. |
| `FIND could not run: waiting for the network failed (<code>).` | `local_error` | The network could not be waited on. |

The log lines that `adl_load` adds to what the target says (the target's own are in [The log, the receipt and failures](log-and-receipt.md)) are:

| Message | When |
| --- | --- |
| `Asking <address> (<THW_ID_POS>) for its configuration...` | An Information operation starts. |
| `Uploading <n> load(s) to <address> (<THW_ID_POS>)...` | An upload starts. |
| `Downloading <n> file(s) from <address> (<THW_ID_POS>)...` | A download in which you name the files starts. |
| `Asking <address> (<THW_ID_POS>) for the files it can download...` | A download in which the target lists its files starts. |
| `Nothing was received, so the empty download folder <folder> was removed.` | A download received nothing, so no folder is left. |
| `Finished: <how it ended>.` | The last line: `completed`; `completed, <n> load(s) failed`; `completed, <n> file(s) missing`; `the target listed no file to download`; `the target did not answer`; `the media set could not be read`; `the media set changed after it was checked`; `the download folder could not be used`; `the operation could not run`; or how the engine ended (`denied by the target`, `aborted by the target`, `a file transfer failed` and so on). |

Without `--json` each event is one line, and the words are for a person, not a format to read:

```text
adl_load <version>: upload, target <address> (<THW_ID_POS>)
Checked media set <part number> in <folder>: <n> members, <n> loads, <n> findings: <finding>; <finding>
info: Uploading 1 load(s) to <address> (<THW_ID_POS>)...
Progress: <n>%; <part number> <status> <n>%
Configuration: <n> hardware items; <name> (serial number <serial number>): <part number> <amendment>, <part number>
The target lists <n> files: <name>, <name>
Received <name>, <n> bytes, in <path>
Found <address>: <THW_ID_POS> (type <type name>, "<literal name>", manufacturer <code>)
Finished: failed, <reason> (exit code <n>): <detail> [<n> targets] [files in <folder>] [record in <file>]
```

A line that starts with a word and a colon (`info:`, `note:`, `warning:`, `error:`) is a log line. A control character in what a target sent is shown as `?`. A count of one is written `1 member`, `1 load`, `1 finding`, `1 file`, `1 byte` and `1 target`. A configuration item with no parts says `no parts`.

## The windows programs

`adl_app`, `adl_media_gen` and `adl_mock_target` have one command-line option, and these exit codes:

| Option | What it does |
| --- | --- |
| `--smoke-test` | Opens the window, draws one frame, prints `smoke-test: first frame drawn` and exits with code 0. It is for checking that a package starts. |

| Exit code | Meaning |
| --- | --- |
| 0 | You closed the window, or `--smoke-test` drew its frame. |
| 1 | The window could not be created. The program ends at once. |

No other exit code is set by the programs' code, and none is documented for them. A program that is stopped from outside, or that the system ends, has whatever code the system gives it.
