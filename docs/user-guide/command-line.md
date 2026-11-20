# Command line and exit codes

Two of the programs are run from a command line: `adl_write_payloads`, which writes sample media sets, and `adl_mock`, a mock target with no window. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

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

stopping
  --operations <n>          stop after n operations (a refused one counts)
  --seconds <n>             stop after n seconds

output
  --json                    one JSON object a line, for a program to read
  --help, --version

exit codes: 0 stopped as asked, 1 the run failed, 2 wrong usage, 3 a folder cannot be used,
4 a port cannot be opened
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
| `--operations <n>`, `--seconds <n>` | Stop after n operations (a refused one counts), or after n seconds | Run until killed |
| `--json` | One JSON object a line | One plain line an event |
| `--help`, `--version` | The usage; `adl_mock <version> events 1` | |

A folder is given in the system's own way of writing it, in any characters the system allows: the program takes its arguments as UTF-8 and does not lose a letter that the system's code page lacks.

With no folder given with `--holds`, the one file it holds says: *The mock target serves the files it holds from the folder given with --holds in a download. Put files there and ask for them by name.*

### What it writes

Standard output carries the events and nothing else. Standard error carries what is not a result: a usage error, a file that could not be stored, why a run failed. Each event is one line, written and flushed when it happens.

By default a line is for a person: the seconds since the program started, the name of the event and `key=value` pairs.

```text
0.003 started program=adl_mock version=0.1.2 events=1 thw_id_pos=HNPFMS_L address=127.0.0.1 find_port=1001 tftp_port=59 faults=none
1.210 operation_began operation=information
1.211 progress operation=information percent=0
5.730 operation_ended operation=information status=3 status_name=completed refused=false
5.731 stopped reason=operations operations=1
```

With `--json` a line is one JSON object, in UTF-8, for a program to read: `"event"` is the first key and `"t"` (the seconds, with three decimals) the second. On Windows the standard output is a text stream and each line ends in a carriage return and a line feed; a reader that splits at the line feed and ignores a carriage return at the end of a line reads both.

The events are `started`, `operation_began`, `progress`, `upload_received`, `operation_ended` and `stopped`. The keys of each, and what a reader should do with a key or a format number it does not know, are in [The mock target](../mock-target.md#output).

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | It stopped as asked (`--operations` served, `--seconds` passed), or printed `--help` or `--version` |
| 1 | The run failed: the platform would not start, or waiting on the network failed (`stopped` says `error`) |
| 2 | Wrong usage. Nothing was started |
| 3 | A folder cannot be used: `--holds` is not a folder or cannot be read, or `--storage` is not a folder and cannot be made |
| 4 | A port cannot be opened: it is in use, or needs a privilege |

On exits 2, 3 and 4 no event is written: the target never started.

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
