# Quick reference

The numbers and names the guide uses, on one page. Each section says where the detail is.

## The programs

| Program | File | What it is for |
| --- | --- | --- |
| ARINC Data Loader | `adl_app` | Find a target, read what it holds, upload a load, download files |
| ARINC Media Set Generator | `adl_media_gen` | Make a media set from files, and check it |
| ARINC Mock Target | `adl_mock_target` | Stand in for a target, and misbehave on request |
| (command line) | `adl_write_payloads` | Write sample media sets ([Command line](command-line.md)) |
| (command line) | `adl_make_set` | Make a media set from a description file, check one, say what it holds ([Command line](command-line.md#adl_make_set)) |
| (command line) | `adl_licence` | Show, import and remove a licence ([Command line](command-line.md#adl_licence)) |

## Ports

All UDP over IPv4.

| Port | Who listens | For |
| --- | --- | --- |
| 59 | The target. The mock target's *TFTP control port*, which you can change. | The loader's requests. The loader always sends its requests to port 59 of the target. |
| 1001 | The target (the mock target's *FIND port*, which you can change). The loader, while a FIND runs. | FIND |
| 59, unless you change it (Preferences, *Control port*) | The loader, during an operation | The target comes back here with status files, lists, and in a download the files. Change it if the target is on the same computer. |
| Chosen by the system | The loader, one for each transfer | The files of one transfer |
| 40000 and up | The mock target, for each transfer | Its replies |

On Linux, ports below 1024 need extra privilege ([Troubleshooting](troubleshooting.md#a-port-is-in-use-or-not-allowed)). [What the firewall asks](installing.md#what-the-firewall-asks).

## Time-outs

| What | Value |
| --- | --- |
| A packet with no answer | 2 seconds, and one retry |
| No status file | 13 seconds, then "data load protocol time-out" |
| FIND listens for | 3 seconds |

## The operations and their files

Each file is named `<THW_ID_POS>` and an extension, for example `HNPFMS_L.LCI`. The detail is in [Files the programs read and write](../file-reference.md#the-twelve-arinc-615a-protocol-files).

| Operation | The loader reads | The loader writes | The target writes |
| --- | --- | --- | --- |
| Information | `.LCI` | | `.LCS`, `.LCL` |
| Upload | `.LUI` | `.LUR` | `.LUS` |
| Download, media defined | `.LND` | `.LNR` | `.LNS`, and the files |
| Download, operator defined | `.LNO` | `.LNA` | `.LNL`, `.LNS`, and the files |

## A media set

```text
<part number>/
  member_001/
    LOADS.LUM
    FILES.LUM
    BATCHES.LUM        only if the set has a batch
    <load folder>/
      <name>.LUH       header file
      <name>.LUP       data files
      <name>.LDR       support files
```

[The media set on disk](../file-reference.md#the-media-set-on-disk) has the rest. The folder layout is this project's own.

## Check values

| Number | Type | Length in a file |
| --- | --- | --- |
| 1 | 8-bit CRC | 2 bytes |
| 2 | 16-bit CRC | 2 bytes |
| 3 | 32-bit CRC | 4 bytes |
| 4 | MD5 | 16 bytes |
| 5 | SHA-1 | 20 bytes |

## Where files go

| What | Where |
| --- | --- |
| A media set from the generator | `<output folder>/<media set part number>/member_NNN` |
| A download | `<download folder>/DNLD_DATA_<THW_ID_POS>_<number>/` |
| An upload, in the mock target | `<storage folder>/<load part number>/<file name>` |
| Settings | The system's per-user settings store ([Configuration reference](configuration.md)) |
| Licence files | Your licence folder, and the machine folder an administrator keeps ([Your licence and edition](licence.md#where-licence-files-go)) |
| The activity log and the receipt | In memory only |

## Keys

| Key | What it does |
| --- | --- |
| Ctrl+F (Cmd+F on a Mac) | Run FIND (the loader) |
| The system's Preferences key | Open Preferences (the loader) |
| The system's Quit key | Close the window |
| Ctrl+C (Cmd+C on a Mac) in the activity log | Copy the log |

[Keyboard](keyboard.md) has the rest.

## Exit codes

| Program | Code | Meaning |
| --- | --- | --- |
| `adl_write_payloads` | 0 | Everything was written and verified |
| | 1 | A scenario could not be built, written, read back or verified |
| | 2 | The command line could not be used |
| `adl_make_set` | 0 | Done: the set was made and verified, or checked, or read |
| | 1 | The set does not read or does not verify |
| | 2 | The command line could not be used |
| | 3 | The description, a source file or the folder given cannot be used |
| | 4 | `--output` is there already and is not an empty folder |
| | 5 | A local failure: memory, a disk, or standard output |
| `adl_licence` | 0 | It was done: `show` looked, a file was imported or was there already, a file was removed, or a program would accept the file |
| | 1 | It could not be done, or a program would refuse the file |
| | 2 | The command line could not be used |
| The three programs with windows | 0 | The window was closed, or `--smoke-test` drew its frame |
| | 1 | The window could not be created |

## Limits

The main ones: 1 to 255 members; 64 targets from one FIND; 256 files in one media defined download; a `THW_ID_POS` of 1 to 240 characters; one job at a time. [Limits and known issues](limits.md) has all of them.
