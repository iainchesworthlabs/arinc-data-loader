# Where files go, and limits

## Where files go

| What | Where |
| --- | --- |
| A media set the **generator** writes | `<output folder>/<media set part number>/member_001/...` (one `member_NNN` per member). If that folder is not empty, `<part number>_2`, `_3` ... is used. Nothing is ever overwritten |
| Files a **download** fetches | `<download folder>/DNLD_DATA_<THW_ID_POS>_<number>/`. The download folder is a [Preference](preferences.md) (default `Documents/ArincDownloads`). Each download makes a new folder; `<number>` is the lowest free one starting at 1, and a download that brings back no file removes its empty folder again, so its number is free again. A file is written under a temporary name ending in `.part` and renamed when it has arrived whole; an existing file is never overwritten |
| What an upload leaves in the **mock target** | `<storage folder>/<load part number>/<file name>` |
| What a download can fetch from the mock target | the files in `<storage folder>/holds`, read when the target is started. You make the `holds` folder yourself; the mock target does not |
| The loader's **activity log** | In memory only. It is not written to a file, it is lost when the program closes, and it keeps the last 2000 lines unless you change that in Preferences. Copy what you need before closing |
| **Settings** (Preferences, last media set, last target, language, window size and position, whether the first-run screen has been passed) | The system's per-user settings store, one per program. On Windows: `HKEY_CURRENT_USER\Software\iainchesworthlabs\ARINC Data Loader` (and `ARINC Media Set Generator`, `ARINC Mock Target`). To start from the first launch again, delete the key (`reg delete "HKCU\Software\iainchesworthlabs\ARINC Data Loader" /f`) while the program is closed. Elsewhere the place is the one Qt's settings use on that system, and has not been checked. If the environment variable `ADL_SETTINGS_DIR` names a folder when a program starts, all three programs keep their settings in an ini file below that folder instead, and write nothing to the usual place |

The programs do not send anything anywhere except to the target (and the FIND broadcast on the
local network): there is no telemetry, no update check and no account. They open a web page (the
GNU licence or the project's repository) only when you click such a link in About.

## Limits

| Thing | Limit |
| --- | --- |
| Members in a media set | 1 to 255 |
| File name | 255 characters in a media set (the standard); 254 for a file you ask a target for in a download |
| Folders deep inside a member | 4 |
| Part number of a media set | 15 characters; of a load or batch, exactly `MMMCC-SSSS-SSSS` |
| Loads per media set made by the generator | 1 (the loader reads sets with more) |
| File size in the generator | Whatever fits in the computer's memory: each file is read whole. Demonstrations were made with files of a few kilobytes to a few megabytes |
| Media set size in the loader | Choosing or re-checking a media set reads every file of it whole into memory, so the check needs memory for the whole set. During an upload the files are read from disk again, in blocks, as the target asks |
| Files the loader keeps open at once | 8 |
| Targets one FIND lists | 64 |
| Batch files in the loader | The loader reads and counts them (the media set's **Properties** show "Batches") but does not use them. An upload sends only the loads you tick |
| Files named in one media defined download | 256. A name that is already in the list is ignored. A name that ends in `.part` is refused |
| `THW_ID_POS` | 1 to 240 characters, none of them a control character, a slash or a backslash. For a download it must also be able to be part of a folder name, because the download folder is named after it: printable ASCII only, and no colon, asterisk, question mark, quote, angle bracket or bar |
| Mock target FIND answer | Hardware ID 15 characters, type name 8, position 8, literal name 20, manufacturer code 3. A longer one is not sent; see [the mock target](mock-target.md#01-identity) |
| Activity log lines | 100 to 1 000 000 (default 2000) |
| Ports | 1 to 65535 |
| Time-outs (fixed by the standard) | TFTP: 2 seconds and one retry per packet; operation: 13 seconds without a status file; FIND: it listens for 3 seconds |
| A job at a time | One. While the loader runs a FIND, an operation or the reading of a media set, it refuses another |

## What has not been checked

This is an early release. Be clear about what is known and what is not.

**Checked** (by the project's automated tests, which run on every change): the file formats of
ARINC 665-3 against an independent writer of the same tables; the 615A protocol, the operations and
FIND against a simulated target in memory; the operations (Information, Upload, both Downloads and
an abort) against the mock target's engine over real sockets on one machine; the loader's windows
driven by a script, offscreen.

**Checked by hand on Windows** (not by the automated tests): the whole demonstration of this
guide, with the three programs on one machine and a script driving the windows. FIND over real
sockets is not part of the automated tests, and between the loader and the mock target on one
machine it does not work (the loader reports "0 targets answered"), which is why the demonstration
types the target's address.

**Not checked:**

- **Anything against real equipment or another vendor's tool.** Neither the media sets the
  generator writes nor the 615A dialogues have been exchanged with a real target or a third-party
  loader. Where the standard is open to more than one reading, the choices made are listed in
  [the ARINC 665 coverage](../arinc665-coverage.md#places-the-standard-is-unclear) and
  [the 615A codec notes](../arinc615a-codec.md#places-the-standard-is-unclear).
- The way a target's hardware ID and position are turned into `THW_ID_POS`
  (`HNPFMS` at `L` becomes `HNPFMS_L`) follows the standard's examples and has not been confirmed
  with a real target. The field can be edited if a target expects something else.
- Which check value a real target would ask for each file of an upload.
- A media set of many gigabytes, a very slow or lossy network, and a target that is still busy
  with another loader.
- FIND with more than one network adapter, or across routers.
- Windows is the platform that has been used by hand. Linux and macOS build and run their tests in
  continuous integration, but nobody has driven the windows on them.
- Screen readers; only the controls' accessible names are in place.
- The translations (see [Languages and colour](languages-and-colour.md#language)).
- There is no program for the target side yet, so nothing here loads real equipment, and the mock
  target is not one.
