# Command line and exit codes

One of the programs is run from a command line: `adl_write_payloads`, which writes sample media sets. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

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
