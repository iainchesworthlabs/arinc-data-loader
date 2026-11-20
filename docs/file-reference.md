# Files the programs read and write

This page is for engineers and technicians who prepare media sets or look at what the programs leave on disk, and for programmers who write or read the same files. It describes the media set folder tree, the twelve ARINC 615A protocol files, the folders the programs create, which check value or CRC is recorded where, and the sample writer `adl_write_payloads`.

The standard describes media, not a folder of members, so the folder layout of a media set is this project's own ([ARINC 665-3 coverage](arinc665-coverage.md#not-yet-verified)). No other tool has read a tree these programs wrote ([what has been checked](status.md#what-is-not-done)).

## The media set on disk

A media set is a folder that holds one folder per member and nothing else. The generator writes it, `adl_write_payloads` writes the samples in the same form, and the loader opens it. `adl::media_tree` ([`media_tree.cpp`](../libs/media_tree/src/media_tree.cpp)) writes and reads the folders. `build_media_set` ([`media_set.cpp`](../libs/arinc665/src/media_set.cpp)) decides which file goes on which member.

### One member

This is what the generator writes for one load, one member and a batch (the file names other than the list files are examples):

```text
ACM-SET-0001/                  the media set folder, named after the media set part number
  member_001/
    LOADS.LUM
    FILES.LUM
    BATCHES.LUM                only if the set has a batch
    ACM.LUB                    the batch file, only if the set has a batch
    ACM4712345678/             the load's folder
      ACM47.LUH                header file
      NAVDATA.LUP              data files
      APPCODE.LUP
      CONFIG.LDR               support file
```

- A member folder is called `member_` and three digits, `member_001` to `member_255`. Any other entry in the media set folder (a stray file, a folder with another name, `member_000`, `member_256`) makes the loader refuse the folder. So does a file or a folder inside a member whose name is not one every system accepts: the name may use printable ASCII only (no accented letter, no Japanese, no emoji), none of the characters `/ \ : * ? " < > |`, and must not end in a dot or a space or be a device name such as `CON`. The loader does not skip such an entry or load the rest: it refuses the folder and names the entry, whatever the entry's name is made of (a name that is not even valid text, which Windows and Linux let a file have, is refused the same way).
- The root of every member holds `LOADS.LUM` and `FILES.LUM`. `BATCHES.LUM` is written only when the set has a batch, and then on every member. Reading needs the first two on each member and treats `BATCHES.LUM` as optional.
- A load's files are in the load's folder, the part root directory. It is named after the load's part number without its hyphens (`ACM47-1234-5678` gives `ACM4712345678`), which is what the standard recommends (3.2.4.1.1).
- The generator puts the header file, the data files and the support files of a one-member set in that folder.
- The generator puts the batch file in the root of member 1. Its name is the first three characters of the batch part number and `.LUB`.

### More than one member

With more than one member, the generator keeps the header file, the support files and the batch file on member 1. It deals the data files out round-robin: the first goes to member 1, the second to member 2, and so on, back to member 1 after the last member. Every data file goes in a folder of the load's name inside the load's folder, so its path is `member_00N/<load>/<load>/<file>`. This holds for member 1 as well.

This is a set of three members with four data files and a batch:

```text
ACM-SET-0001/
  member_001/
    LOADS.LUM
    FILES.LUM
    BATCHES.LUM
    ACM.LUB
    ACM4712345678/
      ACM47.LUH                header file
      CONFIG.LDR               support file
      ACM4712345678/
        FIRST.LUP              data file 1
        FOURTH.LUP             data file 4
  member_002/
    LOADS.LUM
    FILES.LUM
    BATCHES.LUM
    ACM4712345678/
      ACM4712345678/
        SECOND.LUP             data file 2
  member_003/
    LOADS.LUM
    FILES.LUM
    BATCHES.LUM
    ACM4712345678/
      ACM4712345678/
        THIRD.LUP              data file 3
```

Each member has its own list files. A member that gets no data file holds only its list files.

The source is `GeneratorWorker::run` in [`generator_worker.cpp`](../apps/media_gen/core/src/generator_worker.cpp). [Make a media set](user-guide/make-a-media-set.md#how-the-files-are-placed) describes the same placement in words.

### What a program may choose

The library is more general than the generator. For each load, `build_media_set` takes a part root directory (the default is the part number without hyphens) and the member of the header file. For each data or support file it takes a member and a folder below the part root, such as `disk001\`. A batch file and other files (such as `NON_LOAD.CRC` and `DISK.DIR`) can go on any member and in any folder. The sample sets use this ([Scenarios](#scenarios)). Two files of one name in one folder of one member are refused.

A pathname inside a set starts at the root of the member with a backslash and ends with one, for example `\ACM4712345678\`. `adl::media_tree` turns each into native folders. A file more than four directories deep is a finding.

The loader does not assume the generator's layout. It finds each file through `FILES.LUM`, which gives the file's member, folder, name and CRC. A file that is on a member and is not listed in `FILES.LUM` is a finding.

### The list files

| File | Where | What it holds |
| --- | --- | --- |
| `LOADS.LUM` | Root of every member | The media set part number, the number of this member and of all members. For each load: its part number, header file name, the member that holds the header file, and its target hardware IDs. It ends with a 16-bit CRC |
| `FILES.LUM` | Root of every member | For every file on the set except itself: name, folder, member, 16-bit CRC and, if chosen, a check value. It ends with its own check value (if chosen) and a 16-bit CRC |
| `BATCHES.LUM` | Root of every member, only if the set has a batch | For each batch: its part number, file name and member. It ends with a 16-bit CRC |

The copies on different members are the same except for the member number and the CRC. `FILES.LUM` lists the `LOADS.LUM` and `BATCHES.LUM` of every member, each with its member, and records the CRC of those two files as 0x0000.

### What reading a tree requires

- The folder holds only `member_NNN` folders. Below a member there are only folders and ordinary files. A link or any other kind of entry is refused.
- Every file and folder name passes the portable name test ([File names](#file-names)).
- Each member has `LOADS.LUM` and `FILES.LUM`, and the lists agree between members.
- Each file that `FILES.LUM` lists is there, with the CRC and check value it records.
- Each load in `LOADS.LUM` is found through `FILES.LUM` and verified from end to end: header file, data files, support files, Load check value and Load CRC.
- Where the standard says "must", a departure is an error and the set is not read. Where it says "should", the departure is a finding and the set is read.

Writing a tree refuses a member folder that has anything in it, writes nothing if anything is invalid, and removes what it made if a write fails part of the way.

## The twelve ARINC 615A protocol files

The loader and the target exchange these files by TFTP during an operation. They are not part of a media set. The loader holds each in memory while it is decoded and writes none of them to disk. Each is named `<THW_ID_POS>` followed by its extension, for example `HNPFMS_L.LCI`. The extensions are case sensitive.

| Extension | Section | Operation | Written by | How it moves | What it holds |
| --- | --- | --- | --- | --- | --- |
| `.LCI` | 6.4.1 | Information | The target | The loader reads it from the target | Load configuration initialization: the target's answer to the start of the operation (accepted, denied with a reason, or not supported) |
| `.LCL` | 6.4.2 | Information | The target | The target writes it to the loader | Load configuration list: the target's hardware, each item with its parts |
| `.LCS` | 6.4.3 | Information | The target | The target writes it to the loader | Load configuration status: how the operation is going |
| `.LUI` | 6.4.1 | Upload | The target | The loader reads it from the target | Load uploading initialization: as `.LCI` |
| `.LUR` | 6.4.4 | Upload | The loader | The loader writes it to the target | Load uploading request: the loads to upload, each by header file name and load part number (at least one) |
| `.LUS` | 6.4.5 | Upload | The target | The target writes it to the loader | Load uploading status: how the operation is going, and how far each load has got |
| `.LND` | 6.4.1 | Download, media defined | The target | The loader reads it from the target | Load downloading initialization: as `.LCI` |
| `.LNR` | 6.4.6 | Download, media defined | The loader | The loader writes it to the target | Load downloading request: the files to fetch |
| `.LNO` | 6.4.1 | Download, operator defined | The target | The loader reads it from the target | Load downloading initialization: as `.LCI` |
| `.LNL` | 6.4.8 | Download, operator defined | The target | The target writes it to the loader | Load downloading list: the files the target could give |
| `.LNA` | 6.4.9 | Download, operator defined | The loader | The loader writes it to the target | Load downloading answer: the files the operator chose |
| `.LNS` | 6.4.7 | Download, both modes | The target | The target writes it to the loader | Load downloading status: how the operation is going, and how far each file has got |

The order of the files in each operation:

- **Information.** The loader reads `.LCI`. The target writes `.LCS` status files and `.LCL`. The status 0x0003 completes the operation. If the target sent no status file, it is complete when `.LCL` has arrived.
- **Upload.** The loader reads `.LUI`. The target writes a `.LUS` that says "accepted, not yet started". Only then does the loader write `.LUR`. The target reads the header and data files from the loader and keeps writing `.LUS` files. The status 0x0003 completes the operation.
- **Download, media defined.** The loader reads `.LND`. At the status 0x0001 it writes `.LNR`. The target writes the files, and `.LNS` status files.
- **Download, operator defined.** The loader reads `.LNO`. The target writes `.LNL`. The loader writes `.LNA`. The target writes the chosen files, and `.LNS` status files.

The loader starts a transfer with a TFTP read request for an initialization file and with a write request for its own lists. The target starts a transfer with a write request to the loader's control port for its status files and lists. In an upload the target also reads the load's files from the loader, and in a download it writes the requested files to the loader; those are data files and not protocol files. Every protocol file starts with its length (32 bits, counting itself) and the Protocol Version, the two characters `A4`. Numbers are big endian. The loader's default buffer for a protocol file is 32 KB, and a larger file is refused. [ARINC 615A codec](arinc615a-codec.md#the-protocol-files-section-64) has the layouts, and [ARINC 615A engine](arinc615a-engine.md#an-operation-dataloader) has the operations.

## Folders and files the programs create

### The loader's download folder

Every download writes into a new folder, `<download folder>/DNLD_DATA_<THW_ID_POS>_<number>/`.

- **The download folder** is set in [Preferences](user-guide/preferences.md). The default is `ArincDownloads` in the user's Documents folder (in the home folder if there is no Documents folder). It is made, with its parents, if it does not exist.
- **The number** is the lowest one, from 1, for which no file or folder of that name exists. After 10 000 numbers the download is refused.
- **The folder is made before the operation starts.** It is removed again if it is still empty when the operation ends, for example because the target refused, never answered or sent no file. Its number is then free again. A folder that holds anything stays.
- **`THW_ID_POS` must be usable as part of a folder name.** It must be 1 to 240 characters with no control character, `/` or `\`, and `DNLD_DATA_<THW_ID_POS>_1` must pass the portable name test. If not, the download is refused before anything is made.
- **Only the files the operation asked for are written, once each.** In the media defined mode they are the names the operator typed (up to 256 names, each a plain name of at most 254 characters, with no path). In the operator defined mode they are the names left ticked in the target's list. A request to write any other file is refused.
- **Each file is written as `<name>.part`** and renamed when it has arrived whole. A file that fails is thrown away. A file that exists is never overwritten. A name that ends in `.part` is refused. At most 8 files are open at once.
- **A file whose check value does not match is kept.** The log says so.
- **Not written:** the protocol files, and the `DNLD_INFO_...` folder that the standard also lists (5.4.4.3).

Sources: [`download_directory.cpp`](../libs/loader_core/src/download_directory.cpp), [`engine_worker.cpp`](../apps/app/core/src/engine_worker.cpp) and [`directory_files.cpp`](../libs/arinc615a_runner/src/directory_files.cpp). The operator's view is [Download](user-guide/download.md#where-the-files-go).

### The generator's output

The generator writes `<output folder>/<media set part number>/member_NNN/...`.

- The output folder must exist (the window says so if it does not).
- The set is written to `<part number>.writing` beside its final folder (`.writing_2` and so on if that name is taken), read back from disk and verified, and only then renamed. If anything fails or the operator cancels, the `.writing` folder is removed. If it cannot be removed, the banner says where it is.
- The final folder is named after the media set part number. If a folder of that name holds anything, the next free name is used: `<part number>_2`, `_3` and so on up to `_100`. An empty folder of that name is taken over. Nothing is ever overwritten.
- Each source file is read whole into memory.
- The generator writes one load per media set.

Source: [`generator_worker.cpp`](../apps/media_gen/core/src/generator_worker.cpp). The operator's view is [Make a media set](user-guide/make-a-media-set.md#generate).

### The mock target's storage and `holds`

The storage folder is chosen in the mock target's window. With none, the target still serves, and an upload is not kept.

- **An upload** is stored in `<storage folder>/<load folder>/<file name>`. The load folder is the load part number with every character other than a letter, a digit, `.`, `_` and `-` replaced by `_`. A part number such as `ACM47-1234-5678` is unchanged. A file of the same name from an earlier upload is replaced.
- **Only a plain file name is stored**: at most 254 characters, letters, digits, `.`, `_` and `-`, and a portable name. The log reports any other file as "not a safe file name".
- **A download** serves the ordinary files directly inside `<storage folder>/holds`. Folders inside `holds` are skipped. The list is read once, when Start is pressed, so a file added later is served after a stop and a start. The program does not make `holds`.
- **With no storage folder, or nothing in `holds`,** the target holds one text file, `DEMO.TXT`.

Source: [`storage.cpp`](../libs/mock_target_core/src/storage.cpp) (the folders) and [`model.cpp`](../apps/mock_target/core/src/model.cpp) (the log lines the window shows). The operator's view is [Run the mock target](user-guide/mock-target.md#03-storage).

### Settings and the log

Each program keeps its settings, language and window size and position in the system's per-user settings store. On Windows that is the registry key `HKEY_CURRENT_USER\Software\iainchesworthlabs\<program name>`. On macOS it is a property list, and on Linux a configuration file. If the environment variable `ADL_SETTINGS_DIR` names a folder, all three programs use an ini file `<folder>/iainchesworthlabs/<name>.ini` instead and write nothing to the usual place. See [Where files go](user-guide/files-and-limits.md#where-files-go) and [the application](app.md#where-the-settings-are-kept).

The loader's activity log is kept in memory only. It is not written to a file.

## File names

Three tests apply to file names. All three are in the code.

| Test | Where | What it requires |
| --- | --- | --- |
| The standard's rules for a name in a media set (ARINC 665-3, 2.2.2) | `check_file_name`, `check_header_file_name` and the checks for data, support and batch files in `adl::arinc665`. The generator's form applies them, together with the next test | At most 255 characters. Printable ASCII with no blank. None of `~ / : \`. Not `.` or `..`. A header file name begins with a manufacturer's code and ends in `.LUH`. A batch file name ends in `.LUB`. A data file does not use an extension the standard reserves, except `.LUP`. A support file does not either, except `.LDR` and `.HDR` |
| `is_portable_file_name` | `adl::arinc665`. `adl::media_tree` applies it to every file and folder name it writes or reads. The generator's form applies it to the names of the files in its table, to the header file name and to the batch file name. The loader applies it to the download folder name. The mock target applies it before it stores a file | One name that is the same file on every platform. Printable ASCII (0x20 to 0x7E). None of `/ \ : * ? " < >` and not the vertical bar. Not `.` or `..`. Not ending in a dot or a space. Not a device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1` to `COM9`, `LPT1` to `LPT9`, `CONIN$`, `CONOUT$`, in any case, with or without an extension). The length is not limited |
| `is_safe_file_name` | `adl::arinc615a_runner`. The loader applies it to the file names an operator types for a media defined download, and `DirectoryFileSink` applies it to every name a target sends | `is_portable_file_name`, at most 254 characters, and not ending in `.part` |

A name can pass the first test and fail the second, for example one with `*` or `?`, or `CON.LUP`. The generator's form applies both, so it marks such a name on its row (or under the header file or batch part number field) and the set cannot be generated until it is changed. Where a name fails both, the form shows the first test's message, and the second test's once that is put right. A tree written by something other than the generator is refused by the second test when it is written or read.

The rules are documented in [Make a media set](user-guide/make-a-media-set.md#file-names) (both rules, as the generator applies them), in [ARINC 615A engine](arinc615a-engine.md#download-544-633-634) (`is_safe_file_name` and the download sink), and in [Testing](testing.md#what-there-is) (`portable_name_test.cpp`). The operator's rule for a download is in [Download](user-guide/download.md#media-defined-you-name-the-files).

## Check values and CRCs

ARINC 665 numbers its check value types (Table 5.1). The number is what is written in a file, and what the TFTP option `checksum_<n>` carries as `<n>`.

| Number | Type | Length in a file |
| --- | --- | --- |
| 1 | 8-bit CRC | 2 bytes |
| 2 | 16-bit CRC | 2 bytes |
| 3 | 32-bit CRC | 4 bytes |
| 4 | MD5 | 16 bytes |
| 5 | SHA-1 | 20 bytes |

A check value is stored in whole 16-bit words, most significant byte first, so an 8-bit CRC takes two bytes. A file with no check value has the length 0x0000 and nothing more. The CRCs that the standard fixes (the 16-bit CRC of a file, the Header File CRC, the Load CRC) are not a choice. The check values are, and the generator offers none, the three CRCs, MD5 and SHA-1.

| Where | What is recorded | Type | Chosen by |
| --- | --- | --- | --- |
| Header file, each data file | Its length (in words and in bytes), a CRC and a check value | CRC: 16-bit, always. Check value: none or any of the five | The operator, per file (the Check value column of the generator) |
| Header file, each support file | Its length in bytes, a CRC and a check value | As for a data file | As for a data file |
| Header file, the whole load | The Load check value | None or any of the five | The operator (Load check value) |
| Header file, the whole load | The Load CRC | 32-bit CRC, always | Fixed by the standard |
| Header file, itself | The Header File CRC | 16-bit CRC, always | Fixed by the standard |
| `FILES.LUM`, each listed file | A CRC of the whole file, and a check value | CRC: 16-bit, always (0x0000 for `LOADS.LUM` and `BATCHES.LUM`). Check value: none or any of the five | The operator (Each listed file) |
| `FILES.LUM`, itself | A check value, and a CRC at the end of the file | Check value: none or any of the five. CRC: 16-bit | The operator (`FILES.LUM` itself) |
| `LOADS.LUM`, `BATCHES.LUM` and a batch file | A CRC at the end of the file | 16-bit CRC | Fixed by the standard |

The Load check value and the Load CRC cover the whole load. The standard does not say in what order the files are taken. The library takes the header file first, then the data files, then the support files, each in the order the header lists them. This is the project's reading. It has not been checked against another tool's files ([ARINC 665-3 coverage](arinc665-coverage.md#places-the-standard-is-unclear)).

In the TFTP exchange, `checksum_<n>` carries the check value of one file as hexadecimal characters:

- **When the target reads a file from the loader** (an upload), the option names the file when a load holds two files of one name. The value is the 16-bit CRC the header file records. The loader serves the file of that name whose CRC matches. If only one file has the name, it is served whatever the option says. If several have it and none matches, the target is told the file is not found.
- **When the target writes a file to the loader** (a download), the option gives the check value the loader should verify. A mismatch is reported in the log, the file is kept and the operation goes on.

Which check value a real target asks for in an upload has not been checked ([Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked)).

## `adl_write_payloads`

`adl_write_payloads` writes sample media sets to a folder, for trying other loaders. It is a command-line program. The other three programs open a window.

```text
adl_write_payloads [--scenario <name>]... <output-directory>
adl_write_payloads --list
```

- `--list` prints each scenario's name and a one-line description. It takes no other argument.
- With no `--scenario`, every scenario is written. `--scenario` can be repeated. A name that is not a scenario is an error.
- Each scenario goes to `<output-directory>/<scenario>/member_NNN/...`. The output directory is made if it does not exist.
- A `member_NNN` folder that holds anything is never written into. The program reports "cannot write" and stops.
- Each scenario is read back from disk and verified before it is reported. The report is one line, `<name>: <n> members, <n> files, <n> loads, <n> batches, <n> findings -> <folder>`, followed by one indented line for each finding.
- The exit status is 0 when every scenario was written and verified, 1 when one failed to build, write, read back or verify, and 2 for a usage error.

The program is in [`main.cpp`](../apps/write_payloads/main.cpp) and the scenarios are in [`scenarios.cpp`](../libs/samples/src/scenarios.cpp). The release packages carry the program: beside the others in the Windows zip, in `bin/` in the Linux archive, and in `tools/` in the macOS disk image ([Releasing](releasing.md#what-a-release-contains)). [Building](building.md#writing-sample-payloads) shows the commands. The trees it writes have not been read by another loader.

### Scenarios

| Scenario | Media set | Members | What it holds |
| --- | --- | --- | --- |
| `two-member` | `ACM-SET-0001` | 2 | One load (`ACM47-1234-5678`, header file `ACM47.LUH`, for the hardware IDs `ACME` and `0A2B`). Data file `APP.LUP` is in the load's folder on member 1, and `DATA.LUP` is in `disk001` below it on member 2. Support file `CONFIG.LDR` is on member 1. The batch file `ACM47.LUB` is on member 2. `NON_LOAD.CRC`, in no header file, is in the root of member 1. `FILES.LUM` records a CRC-32 for each listed file and has a SHA-1 of its own. The header file records a CRC-32 for `APP.LUP`, an MD5 for `CONFIG.LDR` and a CRC-16 for the whole load |
| `arinc615` | `ACM-615-SET` | 1 | An ARINC 615 part (Appendix F-1) with a folder for each disk. Two data files are both called `LOAD.BIN` and differ in content: one is in `disk001` and one in `disk002`. The support files `CONFIG.LDR` and `EXCONFIG.LDR` are in `disk001`. Header file `ACM60.LUH` |
| `boeing777` | `ACM-777-SET` | 1 | A Boeing 777 load (Appendix F-2). Data files `ACM12345.001` and `ACM12345.002`. The legacy header `ACM12345.HDR` and `CONFIG.LDR` are support files. `DISK.DIR` and `NON_LOAD.CRC` are in the root of the member and in no header file. Header file `ACM61.LUH` |
| `arinc615-boeing777` | `ACM-ALL-SET` | 1 | A load both kinds of loader can use (Appendix F-3). Data file `ACM12345.001`. Support files `ACM12345.HDR`, `CONFIG.LDR` and `EXCONFIG.LDR`. Header file `ACM62.LUH` |

The three Appendix F sets record no check value beyond the 16-bit CRCs, which are always written.

## The media set description

`adl_make_set make` makes a media set from a description file: a JSON file that names the files to put in the set, their part numbers, the number of members, the check value types and the batch file. The same description and the same files give the same set, byte for byte, on every run and on every desktop, because a description holds nothing of the computer it was written on: no output path, no time, and no path of its own (a `source` is relative). [Command line and exit codes](user-guide/command-line.md#adl_make_set) says how the program is run. The window of the media set generator makes one load; a description can make several.

The file is UTF-8 without a byte order mark. It is read with the strict JSON reader of the licence library (`adl/licence/json.hpp`), which refuses what a plain parser lets through, so that no two readers can disagree about what a file says: a key given twice in one object, a comment, a trailing comma, text after the value, a control character in a string, bytes that are not UTF-8, a lone surrogate in an escape, and a value nested more than eight levels deep (a description needs five). The reader says only that the text is not JSON it reads, and does not say where. A key that is not in the tables below is refused, with its place in the file, so that a mistyped key cannot make a set other than the one meant. A new key that changes what a set is will come with a new `format`.

### An example

Two loads, two members and a batch. The files it names are in a folder `payloads` beside it. The first load is for two targets and the second is for one of them, so the batch that is derived names both loads for `ACME` and the first for `ACMEFMS`.

```json
{
  "format": 1,
  "kind": "adl-media-set-description",
  "media_set": {
    "part_number": "ACM-SET-0001",
    "members": 2,
    "files_check": "sha1",
    "files_list_check": "crc32"
  },
  "loads": [
    {
      "part_number": "ACM47-1234-5678",
      "header_file": "ACM47.LUH",
      "type": "OPS",
      "target_hardware_ids": ["ACME", "ACMEFMS"],
      "check": "sha1",
      "files": [
        { "source": "payloads/app.bin", "name": "APPCODE.LUP", "kind": "data", "check": "crc32" },
        { "source": "payloads/nav.bin", "name": "NAVDATA.LUP", "kind": "data", "check": "md5" },
        { "source": "payloads/CONFIG.LDR", "kind": "support" }
      ]
    },
    {
      "part_number": "ACM4C-0100-0200",
      "type": { "description": "Test software", "id": 4 },
      "target_hardware_ids": ["ACME"],
      "files": [
        { "source": "payloads/selftest.bin", "name": "SELFTEST.LUP", "check": "crc16" }
      ]
    }
  ],
  "batch": {
    "part_number": "ACM4D-0100-0300",
    "comment": "Both loads for the unit"
  }
}
```

### The keys

A key marked required has to be there. Any other key may be left out, and then the default applies. A JSON `null` is the same as leaving the key out. A string that is there has to hold at least one character, so to say "no comment" leave `comment` out.

**The whole file**

| Key | Value |
| --- | --- |
| `format` | Required. The whole number 1. Any other number is refused: "format 2 is not one this program reads (it reads format 1)" |
| `kind` | Required. The text `adl-media-set-description` |
| `media_set` | Required. An object, below |
| `loads` | Required. A list of 1 to 100 objects, below, in the order they are listed in `LOADS.LUM` |
| `batch` | An object, below. No batch when it is left out |

**`media_set`**

| Key | Value | Default |
| --- | --- | --- |
| `part_number` | Required. The media set part number. The rules of ARINC 665-3 section 3.1 that the generator's window applies: at most 15 characters, no blank, not ending in a hyphen, without I, Q and Z | |
| `members` | The number of members, 1 to 255 | 1 |
| `files_check` | The check value that `FILES.LUM` records for each file it lists: a check type | `none` |
| `files_list_check` | The check value that `FILES.LUM` records for itself: a check type | `none` |

A **check type** is one of `none`, `crc8`, `crc16`, `crc32`, `md5` and `sha1`: the types of ARINC 665-3 table 5.1.

**Each of `loads`**

| Key | Value | Default |
| --- | --- | --- |
| `part_number` | Required. The load part number, in the form `MMMCC-SSSS-SSSS` with the right check characters (section 2.1). Two loads cannot have the same one | |
| `header_file` | The header file's name: a manufacturer's code, then the rest, ending in `.LUH`, and a name every system accepts. Two loads cannot have the same one | The first three characters of the part number, then `.LUH` |
| `type` | The load type: `null` for none, one of `OPS`, `NDB`, `ADB` and `TST` (the window's four: operational software 0x0001, navigation database 0x0002, aeronautical database 0x0003 and test software 0x0004, with the English words the window writes into the header file), or an object `{"description": ..., "id": ...}` for a type that the system designer assigned: the description is 1 to 255 printable ASCII characters and the id is 0 to 65535 | none |
| `target_hardware_ids` | A list of 0 to 64 target hardware IDs, each 4 to 15 characters or an ARINC 429 equipment code (section 1.5). One twice is refused | empty |
| `check` | The load check value that the header file records: a check type | `none` |
| `files` | Required. A list of 1 to 2000 objects, below, in the order of the header file's lists. At least one is a data file | |

At most 20000 files are named in all.

**Each of `files`**

| Key | Value | Default |
| --- | --- | --- |
| `source` | Required. The file to read: a relative path with `/` between its parts, 1 to 1024 bytes. It has no `\`, no `:`, no control character, no part that is empty or is `.`, and does not start with `/`. A part `..` is allowed. It is read from the folder that holds the description, or from `--source-root`. The path is never written into the set | |
| `name` | The name on the media set. The rules of the window: ARINC 665's rules for a data file or for a support file (a data file cannot have an extension that table 3.2.2-1 reserves, such as `LDR`), and a name every system accepts (no `* ? " < > |`, no dot or space at the end, no device name such as `CON`). Two files of one load cannot have names that differ only in capitals | The last part of `source` |
| `kind` | `data` or `support`. A support file is for a file such as `CONFIG.LDR` | `data` |
| `check` | The check value that the header file records for this file: a check type | `none` |

**`batch`**

| Key | Value | Default |
| --- | --- | --- |
| `part_number` | Required. The batch part number, in the form `MMMCC-SSSS-SSSS` | |
| `file_name` | The batch file's name, ending in `.LUB` and a name every system accepts (`NUL.LUB` is refused) | The first three characters of the batch part number, then `.LUB` |
| `comment` | 1 to 255 printable ASCII characters | none |
| `targets` | A list of 1 to 64 objects `{"id": ..., "loads": [...]}`: the `THW_ID_POS` of a target, and 1 to 100 load part numbers of this set | Each target hardware ID of the loads, in the order they are first met, with the loads that name it, in the order of the loads |

A batch that names no target (no `targets`, and no load has a hardware ID) is refused.

The strings of the text keys above are at most 255 bytes, before the rule of the standard that applies to them.

### How the set is laid out

This is the layout of the generator's window, and the description does not change it.

- A load's folder is named for its part number without the hyphens (`ACM4712345678`) and is in the root of the member that holds its header file, which is member 1. The header file and the support files are in it.
- With one member, the data files are there too. With more than one, a load's data files are dealt out round-robin starting at member 1, each into a folder of the same name inside the load's folder (`ACM4712345678/ACM4712345678`), on member 1 as on the others. Each load starts again at member 1.
- Two loads can each have a file of one name: they are in folders of their own.
- The batch file is in the root of member 1. `BATCHES.LUM` is on every member when there is a batch.
- Every member has `LOADS.LUM` and `FILES.LUM`. The tree is written as `member_001`, `member_002` and so on ([The media set on disk](#the-media-set-on-disk)).

What a description leaves out: user defined data, part flags, target positions in the header file, files that belong to no load (such as `DISK.DIR`), a `member` for a file, and a description on standard input. They can be added as keys.

## The media set inspection

`adl_make_set inspect <folder> --json` reads any media set, not only one that `adl_make_set` made, and writes **one JSON object on one line** that says what a comparison, a manifest or a rule about a site's own part numbers and layout needs. It is not an event: it has no `event` and no `t`, and no `started` or `finished`. It carries its own outcome, so a folder that is not a media set is a document too, with the lists empty. It is written once, when the work is done, and nothing else is written to standard output.

It is UTF-8, written by hand, with the keys in the order of the tables below, no spaces, and a line feed at the end (on Windows a carriage return may come before it). A reader accepts both. Every string is valid UTF-8: a byte that is not part of a well-formed sequence is written as U+FFFD. A reader refuses a `format` it does not know and ignores a key it does not know. A key can be added without changing `format`; removing one, renaming one or changing what one means changes it. The order of every list is the order in the media set's own files, so the same set always gives the same document.

The set is read with `read_media_set`, which verifies every file and every check value, so a set that can be inspected is a set that verifies.

### The keys

**The whole document**

| Key | Value |
| --- | --- |
| `format` | 1 |
| `kind` | `adl-media-set-inspection` |
| `program` | `{"name": "adl_make_set", "version": ...}` |
| `result` | `completed` or `failed` |
| `reason` | `completed`, `set_invalid` or `folder_unreadable`: the words of the table in [Command line and exit codes](user-guide/command-line.md#exit-codes-of-adl_make_set) |
| `detail` | One English sentence |
| `exit_code` | The exit code of the program |
| `folder` | The folder as it was given |
| `media_set` | The header fields, below; `null` when `result` is `failed` |
| `loads` | The loads, below; empty when it failed |
| `batches` | The batches, below; empty when it failed |
| `files` | Every file that `FILES.LUM` lists, below; empty when it failed |
| `unlisted_files` | Files that are on a member and that `FILES.LUM` does not list (it never lists itself), by member, then folder, then name: `name`, `pathname`, `member`, `size`; empty when there are none |
| `findings` | Where the set departs from the standard's recommendations, as sentences `clause: message`; empty when there are none |

**`media_set`**

| Key | Value |
| --- | --- |
| `part_number` | The media set part number |
| `members` | The number of members the lists say |
| `members_present` | The member numbers that were read, in order |
| `format_versions` | `{"loads_list": ..., "files_list": ..., "batches_list": ...}`: the file format version of each list, as text such as `0xA004`; `batches_list` is `null` when the set has no batch |
| `user_defined_data` | `{"loads_list": ..., "files_list": ..., "batches_list": ...}`: the user defined data of each list as upper-case hexadecimal, an empty string for none; `batches_list` is `null` when the set has no batch |
| `files_list_check_type` | The check type word of the check value that `FILES.LUM` records for itself |
| `files_list_check_value` | Its value, or `null` |
| `member_sizes` | A list with one entry for each member read, in order: `{"member": ..., "files": ..., "size": ...}`, the number of the member, how many files are on it and their size in bytes. It counts every file on the member: the lists, `FILES.LUM` (which is in neither `files` nor `unlisted_files`) and any file that `FILES.LUM` does not list. So the sum of `size` over the members is what the set takes on disk, which the sizes in `files` and `unlisted_files` do not give |

**Each of `loads`** (from `LOADS.LUM` and the header file)

| Key | Value |
| --- | --- |
| `part_number`, `header_file` | The part number and the header file's name |
| `header_member`, `header_pathname` | The member that holds the header file, and its folder as `FILES.LUM` writes it (`\ACM4712345678\`) |
| `format_version` | The file format version of the header file, such as `0x8004` |
| `type` | `{"description": ..., "id": ...}`, or `null` |
| `is_download` | The part flag, `true` or `false` |
| `target_hardware_ids` | A list of text |
| `target_hardware_ids_with_positions` | A list of `{"id": ..., "positions": [...]}`; empty when the load is for every position |
| `check_type`, `check_value` | The load check value: a check type word and its value, or `none` and `null` |
| `load_crc` | The Load CRC, 8 upper-case hexadecimal digits |
| `user_defined_data` | Upper-case hexadecimal, an empty string for none |
| `data_files`, `support_files` | Lists of load files, below, in the order of the header file |

**Each load file** (what the header file records for a file, joined with where `FILES.LUM` has it)

| Key | Value |
| --- | --- |
| `name`, `part_number` | As in the header file. The file's own part number is usually an empty string |
| `size` | The size in bytes of the file on its member |
| `crc16` | The 16-bit CRC, 4 upper-case hexadecimal digits |
| `check_type`, `check_value` | The check value that the *header file* records for the file: a check type word and its value, or `none` and `null` |
| `member`, `pathname` | Where the file is, found as ARINC 665-3 section 3.2.4.2 says: the file of that name below the load's folder, then the one with the CRC the header records, then the first |

**Each of `batches`**

| Key | Value |
| --- | --- |
| `part_number`, `file_name` | The batch part number and the batch file's name |
| `member`, `pathname` | Where the batch file is |
| `format_version`, `comment` | The file format version, such as `0x9004`, and the comment (an empty string for none) |
| `targets` | A list of `{"target_hardware_id_position": ..., "loads": [{"header_file_name": ..., "load_part_number": ...}]}` |

**Each of `files`** (what `FILES.LUM` lists, `LOADS.LUM` and `BATCHES.LUM` of every member included)

| Key | Value |
| --- | --- |
| `name`, `pathname`, `member` | The file and where it is |
| `size` | The size in bytes of the file on its member |
| `crc16` | The 16-bit CRC, 4 upper-case hexadecimal digits |
| `check_type`, `check_value` | The check value that *`FILES.LUM`* records for the file: a check type word and its value, or `none` and `null` |

A check type word is `none`, `crc8`, `crc16`, `crc32`, `md5` or `sha1`, and `type_<n>` for a number that is none of them (`type_9`). A check value is the bytes as the file stores them, in 16-bit words with the most significant byte first, as upper-case hexadecimal: an 8-bit CRC is four digits, a zero byte and the CRC.

### An example

The set of the example above, made by `adl_make_set make` and read by `adl_make_set inspect --json` (the document is one line; it is wrapped here, and the list of `files` is cut after its first two entries):

```json
{
  "format": 1,
  "kind": "adl-media-set-inspection",
  "program": { "name": "adl_make_set", "version": "0.1.2" },
  "result": "completed",
  "reason": "completed",
  "detail": "Media set ACM-SET-0001 read: 2 members, 13 files, 2 findings.",
  "exit_code": 0,
  "folder": "out/ACM-SET-0001",
  "media_set": {
    "part_number": "ACM-SET-0001",
    "members": 2,
    "members_present": [1, 2],
    "format_versions": { "loads_list": "0xA004", "files_list": "0xA004", "batches_list": "0xA004" },
    "user_defined_data": { "loads_list": "", "files_list": "", "batches_list": "" },
    "files_list_check_type": "crc32",
    "files_list_check_value": "347489EF",
    "member_sizes": [
      { "member": 1, "files": 9, "size": 1545 },
      { "member": 2, "files": 4, "size": 899 }
    ]
  },
  "loads": [
    {
      "part_number": "ACM47-1234-5678",
      "header_file": "ACM47.LUH",
      "header_member": 1,
      "header_pathname": "\\ACM4712345678\\",
      "format_version": "0x8004",
      "type": { "description": "Operational software", "id": 1 },
      "is_download": false,
      "target_hardware_ids": ["ACME", "ACMEFMS"],
      "target_hardware_ids_with_positions": [],
      "check_type": "sha1",
      "check_value": "FA115FD351D82BBA22114E26D601900A77EECF58",
      "load_crc": "ED82CAFF",
      "user_defined_data": "",
      "data_files": [
        {
          "name": "APPCODE.LUP", "part_number": "", "size": 81, "crc16": "F283",
          "check_type": "crc32", "check_value": "2FDA8452",
          "member": 1, "pathname": "\\ACM4712345678\\ACM4712345678\\"
        },
        {
          "name": "NAVDATA.LUP", "part_number": "", "size": 5, "crc16": "49D6",
          "check_type": "md5", "check_value": "EB61EEAD90E3B899C6BCBE27AC581660",
          "member": 2, "pathname": "\\ACM4712345678\\ACM4712345678\\"
        }
      ],
      "support_files": [
        {
          "name": "CONFIG.LDR", "part_number": "", "size": 4, "crc16": "BFFA",
          "check_type": "none", "check_value": null,
          "member": 1, "pathname": "\\ACM4712345678\\"
        }
      ]
    }
  ],
  "batches": [
    {
      "part_number": "ACM4D-0100-0300",
      "file_name": "ACM.LUB",
      "member": 1,
      "pathname": "\\",
      "format_version": "0x9004",
      "comment": "Both loads for the unit",
      "targets": [
        {
          "target_hardware_id_position": "ACME",
          "loads": [
            { "header_file_name": "ACM47.LUH", "load_part_number": "ACM47-1234-5678" },
            { "header_file_name": "ACM.LUH", "load_part_number": "ACM4C-0100-0200" }
          ]
        }
      ]
    }
  ],
  "files": [
    {
      "name": "LOADS.LUM", "pathname": "\\", "member": 1, "size": 132, "crc16": "0000",
      "check_type": "sha1", "check_value": "4A49883D21A91B38D3F5036CA9FE7367E14D2C12"
    }
  ],
  "unlisted_files": [],
  "findings": [
    "2.2.3.2: the data file APPCODE.LUP is not a whole number of 16-bit words",
    "2.2.3.2: the data file NAVDATA.LUP is not a whole number of 16-bit words"
  ]
}
```

The second load, the second batch target and the other files are cut from this copy. A folder that is not a media set gives the same keys with the failure in them:

```json
{"format":1,"kind":"adl-media-set-inspection","program":{"name":"adl_make_set","version":"0.1.2"},"result":"failed","reason":"folder_unreadable","detail":"nothing is not a folder, or cannot be read.","exit_code":3,"folder":"nothing","media_set":null,"loads":[],"batches":[],"files":[],"unlisted_files":[],"findings":[]}
```
