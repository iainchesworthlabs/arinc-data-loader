# Command line and exit codes

Two of the programs are run from a command line: `adl_write_payloads`, which writes sample media sets, and `adl_make_set`, which makes a media set from a description file and says what a media set holds. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

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
