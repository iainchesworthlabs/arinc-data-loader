# Command line and exit codes

Two of the programs are run from a command line: `adl_write_payloads`, which writes sample media sets, and `adl_licence`, which shows and manages the licence on the computer. The three programs with windows (`adl_app`, `adl_media_gen` and `adl_mock_target`) take one option and nothing else.

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
