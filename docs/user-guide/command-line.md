# Command line and exit codes

Four of the programs are run from a command line: `adl_write_payloads`, which writes sample media sets, `adl_mock`, a mock target with no window, `adl_make_set`, which makes a media set from a description file and says what a media set holds, and `adl_licence`, which shows and manages the licence on the computer. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

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

## `adl_mock`

It is the mock target ([Run the mock target](mock-target.md) describes the one with a window) as a command, with no window and no Qt, for test rigs and CI jobs: a job starts it, points a data loader at its address and reads what it writes. It is a simulated target, not a real one, and it has not been run against real equipment. It answers the data loader's FIND request, serves the initialization files, and runs the Information, Upload and Download operations the way the window's target does, with the faults that the window has (and two it has no control for: refusing with status 0x1002 and serving an initialization file of an invalid structure). It is in the release packages beside the other programs ([Installing](installing.md#getting-the-programs)).

```sh
adl_mock --storage out/kept --operations 1 --json
```

It runs until it has served `--operations` operations, or for `--seconds`, or until it is killed. It handles no signal: killing it abandons what it was doing, and a loader that was in the middle of an operation times out by itself.

```text
usage: adl_mock [options]

A simulated ARINC 615A target, with no window, for test rigs and CI jobs. It is not a real
target. It listens until it has served --operations operations or --seconds have passed, or
until it is killed.

identity
  --hardware-id <text>      THW_ID, at most 15 characters (default HNPFMS)
  --position <text>         POS, at most 8 characters; THW_ID_POS is the two joined by _ (default L)
  --type-name <text>        type name in the FIND answer, at most 8 (default FMS)
  --literal-name <text>     literal name in the FIND answer, at most 20 (default "FMS LEFT")
  --manufacturer <text>     manufacturer code in the FIND answer, at most 3 (default HNP)
  --part <hardware>:<part number>[:<amendment>[:<designation>]]
                            a part to report in the configuration, repeated for more

files
  --holds <folder>          the files it holds for a download (default: one demonstration file)
  --storage <folder>        where uploads are stored (default: served and not kept)

network
  --address <ipv4>          the local address (default 127.0.0.1; 0.0.0.0 is every interface)
  --tftp-port <n>           (default 59)
  --find-port <n>           (default 1001)

pace
  --think-time-ms <n>       0 to 60000 (default 1000)
  --status-period <seconds> 0 to 3600 (default 4)

faults (all off by default)
  --wait-first <seconds>    answer the first request with WAIT
  --wait-every <seconds>    answer every request with WAIT
  --refuse <operation>      refuse information, upload, download-media or download-operator
  --refuse-code <code>      refuse with 0x1000 (default) or 0x1002; needs --refuse
  --invalid-init-file       serve an initialization file of an invalid structure
  --drop-status-every <n>   lose every nth status file
  --corrupt-served          flip a bit in the files it serves
  --wrong-checksum          write a wrong check value option
  --abort-after <n>         abort (0x1003) after n status files
  --silent                  say nothing after the first status file; needs --seconds
  --scenario <file>         take the faults from a scenario file, which says which happens when
                            (instead of the fault options above); see the guide
  --scenario-actions        print the actions a scenario file may use, one for each line, and exit

stopping
  --operations <n>          stop after n operations (a refused one counts)
  --seconds <n>             stop after n seconds

output
  --json                    one JSON object a line, for a program to read
  --help, --version

exit codes: 0 stopped as asked, 1 the run failed, 2 wrong usage, 3 a folder cannot be used,
4 a port cannot be opened, 5 a scenario file cannot be used
```

### The options

Options are written `--name value`. A value is never a list with commas: an option that takes several values (`--part`) is given several times.

| Option | Meaning | Default |
| --- | --- | --- |
| `--hardware-id <text>`, `--position <text>` | The target's identity. `THW_ID_POS` is the two joined by `_`, and it names the protocol files. Letters, digits, `.`, `_` and `-` only; at most 15 and 8 characters. | `HNPFMS`, `L` |
| `--type-name <text>`, `--literal-name <text>`, `--manufacturer <text>` | The rest of the FIND answer: at most 8, 20 and 3 characters | `FMS`, `FMS LEFT`, `HNP` |
| `--part <hardware>:<part number>[:<amendment>[:<designation>]]`, repeatable | The hardware and parts the Information operation reports. The parts of one piece of hardware share its name; the serial numbers are `SN-0001`, `SN-0002` and so on in the order the hardware is first named; a part with no designation is designated by its hardware | The engine's own list |
| `--holds <folder>` | The files it holds for a download: the files directly in the folder, read once when the program starts | A demonstration file, `DEMO.TXT` |
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
| `--scenario <file>` | Take the faults from a scenario file, which says which fault happens when, in place of the fault options. It cannot be given with one of them. The file is described in [Scenarios for the mock target](mock-scenarios.md). A scenario with a step that goes silent needs `--seconds` | Off |
| `--scenario-actions` | Print the words that `action` may have in a scenario file, one for each line, and end with exit code 0. It starts nothing and opens nothing. The first of `--help`, `--version` and `--scenario-actions` decides | |
| `--operations <n>`, `--seconds <n>` | Stop after n operations (a refused one counts), or after n seconds | Run until killed |
| `--json` | One JSON object a line | One plain line an event |
| `--help`, `--version` | The usage; `adl_mock <version> events 1` | |

A folder is given in the system's own way of writing it, in any characters the system allows: the program takes its arguments as UTF-8 and does not lose a letter that the system's code page lacks.

With no folder given with `--holds`, the one file it holds says: *The mock target serves the files it holds from the folder given with --holds in a download. Put files there and ask for them by name.*

### What it writes

Standard output carries the events and nothing else. Standard error carries what is not a result: a usage error, a file that could not be stored, why a run failed. Each event is one line, written and flushed when it happens.

By default a line is for a person: the seconds since the program started, the name of the event and `key=value` pairs.

```text
0.003 started program=adl_mock version=0.1.2 events=1 thw_id_pos=HNPFMS_L address=127.0.0.1 find_port=1001 tftp_port=59 faults=none scenario_features=wait,refuse,invalid_init_file,drop_status_file,abort,silent,corrupt_served,wrong_checksum,delay,drop_datagram,duplicate_datagram,reorder_datagram
1.210 operation_began operation=information
1.211 progress operation=information percent=0
5.730 operation_ended operation=information status=3 status_name=completed refused=false
5.731 stopped reason=operations operations=1
```

With `--json` a line is one JSON object, in UTF-8, for a program to read: `"event"` is the first key and `"t"` (the seconds, with three decimals) the second. On Windows the standard output is a text stream and each line ends in a carriage return and a line feed; a reader that splits at the line feed and ignores a carriage return at the end of a line reads both.

The events are `started`, `operation_began`, `progress`, `upload_received`, `operation_ended`, `scenario_step` and `scenario_datagram` (only with `--scenario`, see [Scenarios for the mock target](mock-scenarios.md#running-adl_mock-with-a-scenario)) and `stopped`. The keys of each, and what a reader should do with a key or a format number it does not know, are in [The mock target](../mock-target.md#output).

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | It stopped as asked (`--operations` served, `--seconds` passed), or printed `--help`, `--version` or `--scenario-actions` |
| 1 | The run failed: the platform would not start, or waiting on the network failed (`stopped` says `error`) |
| 2 | Wrong usage. Nothing was started |
| 3 | A folder cannot be used: `--holds` is not a folder or cannot be read, or `--storage` is not a folder and cannot be made |
| 4 | A port cannot be opened: it is in use, or needs a privilege |
| 5 | The scenario file given with `--scenario` cannot be read or is refused. The sentence on standard error says why ([the page of scenarios](mock-scenarios.md#a-file-that-is-refused)) |

On exits 2, 3, 4 and 5 no event is written: the target never started.

### Messages

Each message is one line of standard error, after `adl_mock: `. A usage error ends with the line `try 'adl_mock --help'`.

| Message | Exit code | What to do |
| --- | --- | --- |
| `unknown option <option>` | 2 | The options are the ones the usage text lists |
| `unexpected argument <argument>` | 2 | Every option starts with `--`, and a value follows its option |
| `<option> is given twice` | 2 | Give each option once. Only `--part` can be repeated |
| `<option> needs a value` | 2 | Put the value after the option |
| `<option> takes a whole number from <low> to <high>` | 2 | Give a number in the range the option has in the table above |
| `--hardware-id takes 1 to 15 printable ASCII characters` | 2 | The same kind of message for `--type-name`, `--literal-name` and `--manufacturer`, and `--position` (0 to 8), with their own limits |
| `<option> takes letters, digits, '.', '_' and '-' (it is part of file names)` | 2 | Use only those characters in `--hardware-id` and `--position` |
| `--part takes <hardware>:<part number>[:<amendment>[:<designation>]]` | 2 | Write the part in that form |
| `the hardware of --part takes 1 to 254 printable ASCII characters` | 2 | Give the hardware a name of that length |
| `the part number of --part takes 1 to 254 printable ASCII characters` | 2 | The same for the part number |
| `the amendment of --part takes 0 to 254 printable ASCII characters` | 2 | The same for the amendment, which may be left out |
| `the designation of --part takes 1 to 254 printable ASCII characters` | 2 | The same for the designation, which if left out is the hardware's name |
| `the parts given with --part do not fit in a configuration list` | 2 | Give fewer parts, or shorter names |
| `<option> needs a folder` | 2 | `--holds` and `--storage` take a folder, not an empty text |
| `--address takes an IPv4 address such as 127.0.0.1` | 2 | Write four numbers from 0 to 255 with dots between them |
| `--refuse takes information, upload, download-media or download-operator` | 2 | Name one of the four operations |
| `--refuse-code takes 0x1000 or 0x1002` | 2 | Use one of the two codes |
| `--refuse-code needs --refuse` | 2 | Say which operation to refuse |
| `--wait-first and --wait-every cannot both be given` | 2 | Give one of them |
| `--silent needs --seconds, because the operation never ends` | 2 | Say how long it should run |
| `--scenario needs a file` | 2 | `--scenario` takes the path of a file, not an empty text |
| `--scenario cannot be used with <option>: the scenario sets the faults` | 2 | Take the fault option out, or put the fault in the scenario |
| `--scenario with a step that goes silent needs --seconds, because the operation never ends` | 2 | Say how long it should run, as `--silent` needs |
| `--scenario <file>: <sentence>` | 5 | The file cannot be read, or it is not a scenario this version can use. The sentence says which step and what is wrong: [A file that is refused](mock-scenarios.md#a-file-that-is-refused) lists them |
| `the hardware ID and position make <name>, which is not a name every system can use for a file` | 2 | Choose another `--hardware-id` or `--position`: the protocol files are named after them |
| `--holds <folder>: <reason>` | 3 | The folder is not there, is not a folder, or cannot be read. The reason is the system's; the program says `not a folder` when it is a file |
| `--storage <folder>: <reason>` | 3 | The folder cannot be made or is not a folder |
| `the FIND port could not be bound: <reason>` | 4 | The port is in use, or needs a privilege. The reason is `port <n>: ` and the system's name for the error. When that is `permission denied` it ends `(a port below 1024 may need extra privilege; see Troubleshooting)`: on Linux and macOS give `--find-port` a high port, and the loader the same one |
| `the TFTP port could not be bound: <reason>` | 4 | The same, for `--tftp-port` |
| `the platform could not start (<reason>)` | 1 | The system would not give the program what it needs to run. Report it ([Support](../support.md)) |
| `waiting on the network failed` | 1 | The run ended in the middle: `stopped` says `error`. Start it again |

These come while it runs, and do not end it:

| Message | What to do |
| --- | --- |
| `there is no storage folder` | A defect in the program: report it |
| `the storage folder could not be made: <reason>` | The folder given with `--storage` is gone or cannot be written. The upload is not kept |
| `the load's folder could not be made: <reason>` | The same, for the folder of the load under it |
| `<file> was not stored: not a safe file name` | The loader sent a name that is not plain ASCII letters, digits, `.`, `_` and `-`, or is not a portable name. The other files of the upload are kept |
| `<file> could not be written to the storage folder` | The disk is full or not writable. The other files are kept |
| `an upload could not be stored` | Report it, with the lines before it |

### On one computer

FIND is fixed to port 1001. Two programs on one computer sharing that port is not reliable, so a loader on the same computer names the target by its address, as the window's does. On Linux and macOS ports below 1024 (59 and 1001) may need privileges: give `--tftp-port` and `--find-port` high ports and tell the loader the TFTP port. The default address, `127.0.0.1`, listens on this computer only and raises no firewall prompt; a rig that wants the network says `--address 0.0.0.0` (or the address of one interface).

Several targets on one computer: the engine writes some of its datagrams from ports of its own, from 40000 up, on the address it listens on, so two targets on one address take those ports from each other. Give each its own address (`127.0.0.2`, `127.0.0.3` and so on; each has port 59 and port 1001). A datagram that cannot be sent because its port is taken is lost, as UDP allows, and the loader's operation then times out.

## `adl_make_set`

`adl_make_set` makes a media set from a description file, with no window, and says what a media set holds. It is for a build or a script that makes the same media set again from a file, and for a program that needs the facts of a set. It is in the release packages beside the other programs ([Installing](installing.md#getting-the-programs)). It has three commands, which are separate on purpose:

```text
usage: adl_make_set make <description> --output <folder> [--source-root <folder>] [--json]
       adl_make_set verify <folder> [--json]
       adl_make_set inspect <folder> [--json]
       adl_make_set --help
       adl_make_set --version

make reads a description file and writes the media set it describes to --output, reads it back and
verifies it, and only then puts it in place. verify checks a media set folder, and inspect says
what one holds. With --json the result is JSON on standard output.
```

| Command | What it does | What it writes |
| --- | --- | --- |
| `make <description> --output <folder>` | Reads the description file ([the description](../file-reference.md#the-media-set-description)) and the files it names, builds the set, writes it beside `--output`, reads it back and checks it with the loader's own check, and only then moves it into place | The media set folder |
| `verify <folder>` | Runs the loader's own check on a media set folder, the check that the loader runs when you choose a folder | Nothing |
| `inspect <folder>` | Reads a media set (any set, not only one that `make` wrote) and says everything a comparison or a list of its contents needs ([the inspection](../file-reference.md#the-media-set-inspection)) | Nothing |

It is the media set generator's window without the window: it makes the sets the same way, by the same rules for names and part numbers, and what it makes is read by the loader. The output for a description is the same bytes on every run and every desktop.

### The options

| Option | Commands | What it does |
| --- | --- | --- |
| `<description>` | `make` | The description file. Give it once |
| `<folder>` | `verify`, `inspect` | The media set folder: the one that has `member_001` in it. Give it once |
| `--output <folder>` | `make`, required | The folder that becomes the media set. It is made, with its parents, if it does not exist. If it exists it must be empty (an empty folder is taken over), and a folder with anything in it is refused and not touched. The generator's window adds `_2` to a name that is taken; this command does not, so that a program knows where the set is |
| `--source-root <folder>` | `make` | The folder that a file's `source` in the description is relative to. The default is the folder that holds the description file |
| `--json` | all | Write the result as JSON on standard output ([below](#what-adl_make_set-writes)). Without it, a few plain lines |
| `--help` | | Print the usage and exit with 0 |
| `--version` | | Print `adl_make_set <version> events 1` and exit with 0 |

A path or a name in an argument can have any characters in it, on Windows as elsewhere (accented letters, Japanese, an emoji): the program takes its arguments as Unicode, and prints a path as UTF-8, which a Windows console shows correctly when it is set to UTF-8 (`chcp 65001`). No command handles a signal: stopping `make` abandons it, and leaves a folder `<output>.writing` beside the output, which you can delete (the next run uses `<output>.writing_2` and so on).

### What `make` does

1. It reads the description. The description's limits and every rule are checked before anything is read from the disk. Each thing that is wrong is said, with its place in the file.
2. It checks that `--output` is free. A folder with anything in it ends the run with exit code 4, before any file is read.
3. It reads every source file whole, counting the bytes.
4. It builds each load and the set, and writes the members to `<output>.writing` beside the output.
5. It reads that folder back and checks it as the loader does: every file, every CRC and every check value. A set that does not read, or is not the one described, is removed and the run ends with exit code 1.
6. It moves the set into place. A set is in `--output` whole and checked, or not at all. Whatever fails after step 4 removes what was written, and says whether anything is left.

### What `adl_make_set` writes

Without `--json`, `make` and `verify` write one line for the set in the form of `adl_write_payloads`, `<part number>: <n> members, <n> files, <n> loads, <n> batches, <n> findings -> <folder>`, then one indented line for each finding ([Findings of the media set check](findings.md)). A count of one is written `1 member`, `1 file`, `1 load`, `1 batch` or `1 finding`. `inspect` writes that line, then one line for each load, `load <part number> (<header file>, member <n>): <n> data files, <n> support files`, and one for each finding. These lines are for a person and are not a format. A failure is one sentence on the error output, `adl_make_set: ` and the sentence, and nothing on standard output.

With `--json`, `make` and `verify` write JSON lines on standard output and nothing else: one object on a line, UTF-8, ended by a line feed (on Windows a carriage return may come before it, and a reader accepts both), each written when it happens. Every event has `event` first and `t` second, the seconds since the program started with milliseconds. A reader ignores a key it does not know. The first event is `started` and the last is `finished`, whose `exit_code` is the program's exit code.

| `event` | Command | When | Keys beside `event` and `t` |
| --- | --- | --- | --- |
| `started` | both | First | `program`, `version`, `events` (the number of this format of events, 1: a reader refuses a number it does not know), `command` (`make` or `verify`), and `description` and `output` for `make` or `folder` for `verify`, as they were given |
| `description_read` | `make` | The description is valid | `media_set_part_number`, `members`, `loads` (the count), `files` (the count of source files), `batch` (`true` or `false`) |
| `progress` | `make` | While the source files are read | `percent`: whole percent of the bytes read, at most one event for each value, never going down |
| `set_written` | `make` | The members are on disk, before they are checked | `members`, `files` (every file on every member, the lists included), `bytes` (the size of all of them) |
| `media_set_checked` | both | The loader's check found the set good | `path` (`--output` or the folder given), `part_number`, `members`, `loads` (the count), `batches` (the count), `files` (every file on every member), `findings` (a list of sentences `clause: message`) |
| `log` | both | Any time | `severity` (`info`, `note`, `warning` or `error`) and `text`. `make` writes a `note` when it takes over an empty output folder, and an `error` for each thing that is wrong in a description |
| `finished` | both | Last | `result` (`completed` or `failed`), `reason`, `detail` (one English sentence), `exit_code`, and `output` (`make`: the folder when it completed, otherwise `null`), `members`, `files` and `finding_count` (`null` when no set was read) |

For `make`, `media_set_checked` is written before the set is moved into place: **the set is in `--output` only when `finished` says `completed`.** Wrong usage writes no events: one sentence and the usage on the error output, and exit code 2.

`inspect --json` does not write events. It writes one JSON object on one line, which has its own `format`, `kind`, `result`, `reason` and `exit_code` and holds the facts of the set; a folder that is not a media set gives the same object with the failure in it. [The media set inspection](../file-reference.md#the-media-set-inspection) gives every key.

### Exit codes of `adl_make_set`

| Code | `reason` | Meaning |
| --- | --- | --- |
| 0 | `completed` | Done. A finding, a place where the set departs from the standard's recommendations, does not stop it |
| 1 | `set_invalid` | The set does not read or does not verify: for `make` the set that was just written (nothing is kept), for `verify` and `inspect` the folder given. For these two it is the result they exist to report |
| 2 | `usage` | The command line cannot be used. Nothing was written and there are no events |
| 3 | `description_invalid`, `source_unreadable`, `folder_unreadable` | An input cannot be used: the description file, a source file that it names, or the folder given to `verify` or `inspect` |
| 4 | `output_exists` | `--output` is there already and is not an empty folder. Nothing was touched |
| 5 | `local_error` | The computer ran out of memory, a folder could not be made or written, the disk is full, the set could not be moved into place, or standard output could not be written |

`reason` is one word for a program to read; `detail` is the sentence for a person. The exit code that `finished` says is the one the program ends with.

### Messages of `adl_make_set`

The program prints a sentence on the error output (or in `detail` and in a `log` event with `--json`) for each of these. `<where>` is a place in the description, written as the reader found it: `loads[1].files[0].source`. The places are `format`, `kind`, `media_set.part_number`, `media_set.members`, `media_set.files_check`, `media_set.files_list_check`, `loads[n].part_number`, `loads[n].header_file`, `loads[n].type`, `loads[n].target_hardware_ids`, `loads[n].check`, `loads[n].files`, `loads[n].files[n].source`, `loads[n].files[n].name`, `loads[n].files[n].kind`, `loads[n].files[n].check`, `batch.part_number`, `batch.file_name`, `batch.comment` and `batch.targets`, with `[n]` for the number of an entry of a list, counted from 0.

**Wrong usage (exit code 2)**

| Message | What to do |
| --- | --- |
| `no command given (make, verify or inspect)` | Start with the command. |
| `unknown command <word>` | The commands are `make`, `verify` and `inspect`. |
| `unknown option <option>` | The options are `--output`, `--source-root`, `--json`, `--help` and `--version`. |
| `<option> needs a value` | Put the folder after `--output` or `--source-root`. |
| `--output is given twice`, `--source-root is given twice` and `--json is given twice` | Give each of these options once. |
| `make takes one description file` | Give `make` the description file, once. |
| `verify takes one folder` and `inspect takes one folder` | Give the media set folder, once. |
| `make needs --output` | Say where to make the set. |
| `--output is not for verify` and the like (`--source-root is not for inspect`) | These two options are for `make`. |

**The description (exit code 3, `description_invalid`)**

| Message | What to do |
| --- | --- |
| `Cannot read <path>.` | The description file is not there or cannot be read. |
| `<path> is larger than 4 MiB.` and `The text is larger than 4 MiB.` | A description is at most 4 MiB. |
| `The text is not JSON that this program reads: a key given twice, a comment, a trailing comma, a byte order mark, text after the value, a control character in a string, bytes that are not UTF-8 or a value nested more than eight deep are refused.` | The text is not JSON, or it has one of the things that are refused. The reader says no more than that, and not where. |
| `The description is not an object.` | The file holds a list, a string or a number. It has to be an object. |
| `<where>: this key is not known.` | The key is mistyped or is not in the format ([the keys](../file-reference.md#the-keys)). |
| `<where>: this key is missing.` (for the top level: `format: this key is missing.`, `kind: this key is missing.`, `media_set: this key is missing.` and `loads: this key is missing.`) | A key that is required is not there. |
| `<where>: expected an object.` and `<where>: expected a list.` | The value is of another kind. |
| `<where>: expected a list of <a> to <b> entries.` | A list is empty or too long. |
| `<where>: expected a string of 1 to <n> bytes.` | A text is empty or too long, or is not text. |
| `<where>: expected a whole number from <a> to <b>.` | A number is a fraction, is outside its range, or is not a number. |
| `<where>: expected one of none, crc8, crc16, crc32, md5, sha1.`, `<where>: expected one of data, support.` and `<where>: expected one of OPS, NDB, ADB, TST, or an object with description and id.` | A word is not one of those the format has: a check type, a kind of file, or a load type. |
| `<where>: expected printable ASCII characters.` | A load type description or a batch comment has a character that is not printable ASCII. |
| `<where>: this source path is not a relative path written with /.` | A `source` is absolute, has a `\` or a `:`, has an empty part or a part `.`, or ends in `/`. Write it relative, with `/` between its parts, and use `--source-root` to say where it starts. |
| `format <n> is not one this program reads (it reads format 1).` | The description is of another format. |
| `format: expected a whole number.` | `format` is not a number. |
| `kind: expected one of adl-media-set-description.` | The file is not a media set description. |
| `More than 20000 files in all.` | A description names at most 20000 files. |
| `<where>: <clause>: <what the standard recommends>` | A name or a part number breaks a rule that the generator's window marks too: the clause of ARINC 665-3 and what it asks, as in [Findings of the media set check](findings.md). |
| `<where>: this name will not work on every system.` | A name has a character that a folder name cannot hold on every system, ends in a dot or a space, or is a device name such as `CON`. |
| `<where>: another file of this load has this name.` | Two files of a load have names that differ only in capitals. |
| `<where>: another load has this part number.` and `<where>: another load has this header file name.` | Two loads have the same part number or header file name. |
| `<where>: this hardware id is given twice.` | A target hardware ID is repeated in a load. |
| `<where>: the load has no data file.` | A load has only support files. |
| `<where>: the media set part number is empty` and the like (`the load part number is empty`, `the header file name is empty`, `the batch part number is empty`, `the batch file name is empty`, `the name is empty`) | A text that has to be given is empty. |
| `<where>: the number of members is not from 1 to 255.` and `<where>: the set has no load.` | The set is outside what a media set can be. |
| `<where>: the batch names no target.` and `<where>: the target gets no load.` | A batch has to name a target, and each target a load. |
| `<where>: no load of the set has this part number.` | A batch target names a load that the description does not have. |
| `The load could not be built: <reason>.` and `The media set could not be built: <reason>.` | The ARINC 665 library refused what was described, for the reason it gives. |

**The files and the folders (exit codes 3, 4 and 5)**

| Message | Exit code | What to do |
| --- | --- | --- |
| `Cannot read <path>.` | 3 (`source_unreadable`) | A file the description names is not there, is a folder, or cannot be read. |
| `<path> is too large to be read into memory.` | 3 (`source_unreadable`) | The file is too large for this computer to hold, with the others, while the set is made. A set needs about three times the size of its files in memory. |
| `<folder> is not a folder, or cannot be read.` | 3 (`folder_unreadable`) | The folder given to `verify` or `inspect` is not there or is a file. |
| `<folder> is there already and is not an empty folder.` | 4 (`output_exists`) | `--output` is taken. Give another folder, or remove what is there. |
| `The media set could not be written below <folder> (<what the system said>).` | 5 (`local_error`) | A folder could not be made or written, or the disk is full. |
| `The media set could not be written below <folder>: <reason> (<path>).` | 5 (`local_error`) | A file could not be written. What had been written is removed. |
| `Too many unfinished sets (<name>.writing...) are beside the output.` | 5 (`local_error`) | There are a hundred folders `<output>.writing` and so on beside the output, left by runs that were stopped. Delete them. |
| `The media set could not be put in place at <folder> (<what the system said>).` | 5 (`local_error`) | The finished set could not be moved to `--output`. |
| `Nothing was kept: what had been written was removed.` | | Said after a failure that came after the writing started: nothing is left of the set. |
| `What had been written could not be removed, and is still in <folder>.` | | Said instead of the one above when the folder `<output>.writing` could not be removed. Remove it. |
| `The computer ran out of memory while the media set was being made.` | 5 (`local_error`) | Make a smaller set, or close other programs. |
| `The media set could not be made: <reason>.` and `The job was stopped.` | 5 (`local_error`) | Something that is not expected. Report it. |
| `The empty folder <folder> was taken over.` | | A `note`: `--output` was an empty folder, and the set is in it now. |
| `Standard output could not be written.` | 5 (`local_error`) | The program that reads standard output has gone. |

**The set (exit code 1, `set_invalid`)**

| Message | What to do |
| --- | --- |
| `The written media set could not be read back: <reason> (<path>).` | The set that `make` wrote could not be read from the disk. Nothing is kept. Try again, and check the disk. |
| `The written media set does not verify: <reason>.` | The set that `make` wrote has a file or a check value that does not verify. Nothing is kept. This is a fault of the program or the disk: report it. |
| `The written media set is not the one described: <what>.` | What was read back is not what the description says (a part number, a number of members or loads or files). Nothing is kept. Report it. |
| `The media set could not be read: <reason> (<path>).` and `The media set could not be read: <reason>.` | `verify` or `inspect` could not read the folder as a media set, or the set does not verify. The reason is the loader's: the same as when it refuses the folder. |

And the plain lines of success: `Media set <part number> written and verified: <n> members, <n> files, <n> findings.` is the `detail` of a `make` that completed, `Media set <part number> verified: ...` that of `verify`, and `Media set <part number> read: ...` that of `inspect`.

### An example

A description and its files are in `sets/`, as in [the description](../file-reference.md#the-media-set-description). On Windows, with the Release build, `make` is run with `--json`, and the same set is then checked and read:

```text
> adl_make_set make sets\acm.json --output out\ACM-SET-0001 --json
{"event":"started","t":0.000,"program":"adl_make_set","version":"0.1.2","events":1,"command":"make","description":"sets\\acm.json","output":"out\\ACM-SET-0001"}
{"event":"description_read","t":0.010,"media_set_part_number":"ACM-SET-0001","members":2,"loads":2,"files":4,"batch":true}
{"event":"progress","t":0.014,"percent":82}
{"event":"progress","t":0.014,"percent":87}
{"event":"progress","t":0.015,"percent":91}
{"event":"progress","t":0.015,"percent":100}
{"event":"set_written","t":0.059,"members":2,"files":13,"bytes":2444}
{"event":"media_set_checked","t":0.086,"path":"out\\ACM-SET-0001","part_number":"ACM-SET-0001","members":2,"loads":2,"batches":1,"files":13,"findings":["2.2.3.2: the data file APPCODE.LUP is not a whole number of 16-bit words","2.2.3.2: the data file NAVDATA.LUP is not a whole number of 16-bit words"]}
{"event":"finished","t":0.090,"result":"completed","reason":"completed","detail":"Media set ACM-SET-0001 written and verified: 2 members, 13 files, 2 findings.","exit_code":0,"output":"out\\ACM-SET-0001","members":2,"files":13,"finding_count":2}
> echo %ERRORLEVEL%
0
> adl_make_set verify out\ACM-SET-0001
ACM-SET-0001: 2 members, 13 files, 2 loads, 1 batch, 2 findings -> out\ACM-SET-0001
    2.2.3.2: the data file APPCODE.LUP is not a whole number of 16-bit words
    2.2.3.2: the data file NAVDATA.LUP is not a whole number of 16-bit words
```

The two findings are the standard's recommendation that a data file be a whole number of 16-bit words; the two files have 81 and 5 bytes. A finding does not stop anything.

Running `make` again with the same `--output` ends with exit code 4, because the set is there:

```text
> adl_make_set make sets\acm.json --output out\ACM-SET-0001
adl_make_set: out\ACM-SET-0001 is there already and is not an empty folder.
> echo %ERRORLEVEL%
4
```

## `adl_licence`

It shows the licence on this computer, copies a licence file into your licence folder, takes one out again and
checks a file without keeping it. It has no window, so it is for a computer with no display and for scripts. What
a licence is, what the editions are and what the windows show of it are on [Your licence and edition](licence.md). It
is in the release packages beside the other programs ([Installing](installing.md#getting-the-programs)).

```text
usage: adl_licence show
       adl_licence import <file>
       adl_licence remove <name>
       adl_licence verify <file>
       adl_licence --help
       adl_licence --version

Shows and manages the ARINC Data Loader licence on this computer. Every program of ARINC Data Loader reads the
same licence files. show and verify change nothing. A licence file is a record of what was bought: no program
needs one to work.

commands:
  show           the edition and the licence in force, and each licence file the programs find
  import <file>  check a licence file, or a list of withdrawn licences, and copy it to your licence folder
  remove <name>  remove a file from your licence folder, by the name show prints
  verify <file>  judge a file as a program would, and copy nothing

Exit codes: 0 done, 1 could not be done, 2 the command line is wrong.
ADL_SETTINGS_DIR moves the licence folders and the clock file, as it moves the settings of the programs.
```

It reads and writes the same files, in the same folders, as the three programs with windows ([Where licence
files go](licence.md#where-licence-files-go)), and `ADL_SETTINGS_DIR` moves them the same way. It uses no network and sends
nothing. `show` and `verify` change nothing at all. `import` and `remove` change your licence folder, and `import`
also changes the clock file when the licence it copies carries a clock reset ([The date](licence.md#the-date)). The
file you import is never changed or removed.

Every name on the command line can have any characters in it, on Windows as elsewhere (accented letters,
Japanese, an emoji). The program prints what came from a file or a folder (a name, a licence number, the name of
the licensee) with each control character, and each character that sets the direction of text, replaced by `?`, so
that a file cannot change what your terminal shows. It never prints a key, a signature or the contents of a file.

The examples below were made with a throwaway key that no release accepts, and with licence files made for the
tests (`tests/licence/data/licences`), on Windows. A release that carries ARINC Data Loader's own keys prints the
same lines for a licence made with them.

### `adl_licence show`

It judges every licence file in your licence folder and in the folder of the computer, and prints the licence that
the programs use and then the files.

```text
> adl_licence show
Edition: pro
State: valid
Licence: ADL-2026-900001
Kind: annual
Licensed to: Example Lab Pty Ltd
Term: 2026-01-01 to 2027-01-01
Days left: 214
Seats: 1
Features: support
Computers: any
Files:
  ADL-2026-900001.adllic (user, valid, in use)
```

With no licence file it prints the Community edition and no files:

```text
> adl_licence show
Edition: community
State: none
Reason: there is no licence
Files: none
```

The words before the colon are the same in every release, so that a script can read them: a line may be added,
and no line is renamed. Dates are UTC dates, not the computer's local ones.

| Line | What it says |
| --- | --- |
| `Edition:` | `community`, `pro` or `enterprise`. A licence that has ended, has not started or is not valid is `community` |
| `State:` | `none` (no licence file), `valid`, `expiring` (30 days or fewer left, a trial 3 or fewer), `grace`, `expired`, `not_yet_valid`, `clock_suspect` (the date of this computer looks wrong) or `invalid` |
| `Reason:` | Why the state is what it is, in a few words. Not printed when the licence is in force |
| `Licence:` | The licence number. This line and the ones down to `Computers:` are printed when a licence was read, even one that has ended or was withdrawn |
| `Kind:` | `trial`, `annual` or `enterprise` |
| `Licensed to:` | The name of the licensee. The email address and the contact a licence may hold are not printed |
| `Term:` | The first and the last day of the licence |
| `Days left:` | Whole days to the end of the term; 0 is the last day. Printed when the licence is in force and not in its grace period |
| `Grace ends:` | The last day of the grace period, in the state `grace` |
| `Seats:` | The number of seats, or `site licence` |
| `Features:` | What the licence includes, such as `support`, or `none` |
| `Computers:` | The computers the licence names (a name ending in `*` stands for every name that starts so), or `any` |
| `Updates until:` | The end of the version window, when the licence has one |
| `Files:` | The files found, one on each line below it: the name, whether it is in `user` (your licence folder) or `machine` (the folder of the computer), its state, why when it is `invalid`, and `in use` for the one the programs use. `none` when there is no file |
| `Revocation lists:` | How many signed lists of withdrawn licences were read and accepted, and how many were refused. Printed when there is one |
| `Note:` | A line that explains something about what was found |

A `Note:` line is printed when there are licence files and this release has no key to check them with, when a
folder holds more than 64 files (the rest are not read) and when a folder could not be read.

### `adl_licence import <file>`

It checks the file as a program would and, if a program would accept it, copies it into your licence folder under
the licence number (`ADL-2026-900001.adllic`). The file can be a licence file or a signed list of withdrawn licences
(`.adlrev`). An import never replaces a file: a name that is taken by a different file gets `-2`, `-3` and so on, and
the same file twice is copied once. A licence that has ended or has not started is accepted, because it is your
file, and the answer says what state it is in. A licence that was withdrawn, that names other computers or another
product, that needs a newer version or whose signature does not verify is refused, and nothing is written. A licence
that carries a clock reset sets the time the program remembers back to the day the licence was issued, once.

```text
> adl_licence import C:\licences\valid.adllic
Imported licence ADL-2026-900001 as ADL-2026-900001.adllic.
This is now the Pro edition.
```

A licence that has ended says so on the second line, and so do the other states:

| Second line of the answer | When |
| --- | --- |
| This is now the *Pro* or *Enterprise* edition. | The licence is in its term, or ends soon |
| This licence ended on *date*. It was imported, but it includes nothing now. | The licence has ended and its grace period too |
| This licence ended on *date*. It was imported, and its grace period lasts until *date*. | The licence is in its grace period |
| This licence starts on *date*. | The licence has not started |
| The date on this computer looks wrong, so the licence cannot be judged yet. It was imported. | The clock of this computer is behind the latest time the program has seen |

### `adl_licence verify <file>`

It does what `import` does with the file and stops before it writes anything: it prints what the file amounts to and
says whether a program would accept it. It takes the signed lists of withdrawn licences in your folders into account
and says that the folder is full when an import would fail for that. It does not try to write, so it does not find
a folder that cannot be written. Its exit code is 0 exactly when `import` would copy the file or find it in your
licence folder already.

```text
> adl_licence verify C:\licences\expired.adllic
Edition: community
State: expired
Reason: the licence has ended
Licence: ADL-2026-900004
Kind: annual
Licensed to: Example Lab Pty Ltd
Term: 2025-03-01 to 2026-03-01
Seats: 1
Features: support
Computers: any
A program would accept this licence file.
```

For a signed list of withdrawn licences it prints `Sequence:` (the number of the list; a program keeps the list with
the highest number), `Licences withdrawn:` and `Keys withdrawn:`, the numbers of each, and then that a program would
accept the list. A file that a program would refuse prints what it can say about it, and the reason on the error
output, with exit code 1:

```text
> adl_licence verify C:\licences\machines.adllic
Edition: community
State: invalid
Reason: the licence does not name this computer
Licence: ADL-2026-900011
Kind: annual
Licensed to: Example Lab Pty Ltd
Term: 2026-01-01 to 2027-01-01
Seats: 1
Features: support
Computers: BENCH-*, lab-pc-01
verify: This licence is for other computers. This computer is OFFICE-PC. Ask your administrator for a licence that names it.
```

### `adl_licence remove <name>`

It removes one file from your licence folder. Give the name that `show` prints. A name is the name of a file and
nothing more: a folder, a path and a file that is not a licence file or a list are refused. The folder of the
computer is not yours to change: a file there is left alone, and the answer says so. The file the licence was
imported from is not touched, so the licence can be imported again.

```text
> adl_licence remove ADL-2026-900001.adllic
Removed ADL-2026-900001.adllic from your licence folder. The file it was imported from was not touched.
```

### `adl_licence --version`

It prints one line, the program's name and the version it was built from (`adl_licence v0.1.2`).

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | It was done. `show` could look, whatever it found, and that includes no licence at all. `import` copied the file, or found it in your licence folder already. `remove` removed the file. `verify` says a program would accept the file. `--help` and `--version` also end with 0 |
| 1 | It could not be done. `show` could not judge, because the clock of this computer cannot be read or the computer has no licence folder. `import` copied nothing. `remove` removed nothing. `verify` says a program would refuse the file. The reason is printed on the error output, after the name of the command |
| 2 | The command line could not be used. Nothing was done. The reason and then the usage text are printed on the error output |

### Messages

Errors are printed on the error output as `<command>: <sentence>`; a command line that cannot be used as
`adl_licence: <sentence>`, followed by the usage text. The report is printed on the standard output.

| Message | Exit code | What to do |
| --- | --- | --- |
| give a command | 2 | Say what to do: `show`, `import`, `remove` or `verify` |
| unknown command *word* | 2 | Use one of the four commands |
| unknown option *word* | 2 | There are no options but `--help` and `--version`, and they stand alone |
| *command* takes no argument | 2 | `show` takes nothing after it |
| *command* needs the name of one file | 2 | Give one file to `import` or `verify` |
| remove needs the name of one licence file, as show prints it | 2 | Give one name, as `show` prints it |
| *option* cannot be given with anything else | 2 | Use `--help` or `--version` alone |
| the files and the clock of this computer could not be opened | 1 | The program could not start the system services it needs. Try again, and report it if it happens again ([Support](../support.md)) |
| the clock of this computer cannot be read, so a licence cannot be judged | 1 | The computer has no clock the program can read. Nothing can be said about a licence |
| this computer has no folder for licence files | 1 | The system gave no place to keep a licence. Set `ADL_SETTINGS_DIR` to a folder |
| cannot read *file*: it does not exist | 1 | Check the name and the folder |
| cannot read *file*: it is not a file, or it could not be read | 1 | The name is a folder, or you are not allowed to open the file |
| cannot read *file*: it is larger than 65536 bytes, which no licence file is | 1 | It is not a licence file |
| This licence needs a newer version of ARINC Data Loader. Install the latest version, then import the licence again. | 1 | The licence was made for a later version than this one |
| This licence was withdrawn. Contact support. | 1 | The owner withdrew the licence, or a list in one of your folders does |
| This licence is for other computers. This computer is *name*. Ask your administrator for a licence that names it. | 1 | The licence names computers, and this is not one of them |
| This licence is for another product. Ask for an ARINC Data Loader licence. | 1 | The file is a licence for something else |
| This file is not a valid ARINC Data Loader licence. It may be damaged or incomplete. Ask for the file again. | 1 | The file is damaged, cut short or not a licence |
| This version of ARINC Data Loader cannot check licence files yet, so it accepts none. The Community edition works as it always did. | 1 | This release has no key to check a licence with ([What a customer sees until the keys are in](licence.md#what-a-customer-sees-until-the-keys-are-in)) |
| The licence folder already holds 64 files. Remove one, then import again. | 1 | Remove a file you do not need |
| The licence could not be saved in *folder*. Check that the folder can be written. | 1 | The disk is full or the folder is read only |
| *name* is in the licence folder of this computer, which only an administrator can change. | 1 | Ask the administrator, or leave it |
| there is no file called *name* in your licence folder; show prints the names | 1 | Use a name that `show` prints, with `user` after it |
| *name* is not the name of a licence file: give a name as show prints it, with no folder | 1 | Give a name and not a path |
| the file *name* could not be removed | 1 | The file is in use or you may not remove it |
| unexpected failure: *reason* | 1 | A defect in the program: report it ([Support](../support.md)) |

What a command prints when it works:

| Message | Command |
| --- | --- |
| Imported licence *number* as *name*. | `import` of a licence |
| Imported the list of withdrawn licences as *name*. A licence on it is shown as withdrawn. | `import` of a list |
| That file is already in the licence folder as *name*. Nothing was copied. | `import` of a file that is there already |
| A program would accept this licence file. | `verify` of a licence |
| A program would accept this list of withdrawn licences. | `verify` of a list |
| It is in the licence folder already as *name*. | `verify` of a file that is there already |
| Note: This version of ARINC Data Loader cannot check licence files yet, so it accepts none. The Community edition works as it always did. | `show`, when there are licence files |
| Note: A licence folder holds more than 64 files; the rest were not read. | `show` |
| Note: A licence folder could not be read. | `show` |

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
