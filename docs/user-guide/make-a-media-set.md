# Make a media set

The **ARINC Media Set Generator** turns files from your disk into a media set: it works out the
header file, the three list files and the check values, writes the folder tree the loader opens,
and then reads that tree back to prove it is sound. You decide the parts that are a person's
choice (names, part numbers, which check values), and it enforces the rest.

![The generator, empty](../images/gen-empty.png)

The window has a rail on the left with three numbered blocks to fill in, the table of files on the
right, and an action bar at the bottom. The button at the bottom right is always the main action:
**Generate media set**. It stays dimmed, and the bar says what is missing, until everything needed
is present and valid. The header chip says **INCOMPLETE**, **READY**, **GENERATING** or
**GENERATED**.

## 01 Media set

| Field | What to enter |
| --- | --- |
| **Part number** | The media set's part number, for example `ACM-SET-0001`. Rules are [below](#media-set-part-number) |
| **Members** | How many members (disks) the set has, 1 to 255. One is right for a small load. With more, the data files are spread over the members, see [How the files are placed](#how-the-files-are-placed) |
| **Each listed file** | The check value kept in `FILES.LUM` for every file it lists: none, 8-bit CRC, 16-bit CRC, 32-bit CRC, MD5 or SHA-1 |
| **FILES.LUM itself** | The check value for the `FILES.LUM` list file itself, from the same choices |
| **Choose output directory...** | The folder in which the set is written. It must exist. The set goes into a new folder inside it named after the media set part number |

The demonstration uses **32-bit CRC** for each listed file and **16-bit CRC** for `FILES.LUM`.

## 02 Load

| Field | What to enter |
| --- | --- |
| **Part number** | The load's part number, for example `ACM47-1234-5678`. Rules are [below](#load-and-batch-part-numbers) |
| **Header file** | The header file's name. It follows the load part number (first three characters plus `.LUH`, so `ACM47.LUH`) until you type your own |
| **Load type** | *None*, or one of *Operational software* (OPS), *Navigation database* (NDB), *Aeronautical database* (ADB) or *Test software* (TST). The choice is written into the header file with its code |
| **Load check value** | The check value the header file carries for the whole load: none, 8-bit CRC, 16-bit CRC, 32-bit CRC, MD5 or SHA-1 |
| **Target hardware IDs** | The kinds of equipment the load is for. Type an identifier and press **Add**; **Remove** takes one away. The rules are [below](#target-hardware-ids) |

## 03 Batch

Tick **Include a batch file** to add one batch file that names the load for every target hardware
ID you listed. It needs its own part number (for example `ACM4C-0100-0200`). The batch file's name
is made from the first three characters of that part number and `.LUB`, and the window shows it
("Written as ACM.LUB"). Leave the box unticked if you do not need a batch.

## The files

**Add files...** opens a file chooser where you can pick several files at once. Each becomes a
row of the table, in the order they were added (**Move up** and **Move down** change the order of
the row you have clicked; **Clear** empties the table; **Remove** on a row takes that file out):

| Column | Meaning |
| --- | --- |
| **Name on the media set** | The file's name inside the set. It starts as the file's own name; you can edit it. It is checked as you type |
| **Kind** | **data** for the load's payload, **support** for files that accompany it, for example `CONFIG.LDR`. Every file starts as *data*; change the ones that are support files. A load needs at least one data file |
| **Check value** | The check value the *header file* records for this file |
| **Size** | The file's size (read-only) |

A row with a problem shows its reason under it, and **Generate media set** stays dimmed until every
row is right. The same name twice (in any capitals) is refused, because two files of one name in
one place cannot be told apart.

![A name used twice is refused](../images/gen-file-error.png)

The chip **NO CHECK VALUES** appears when the three choices *Each listed file*, *FILES.LUM itself*
and *Load check value* are all *none*: the set would be written, but the loader could not verify its
files against the header. Choose a check value unless the format you are following requires none.

![The generator with the form filled in and three files chosen](../images/gen-ready.png)

## Generate

Press **Generate media set**. The generator

1. reads every file (the progress bar counts the bytes read; a **Cancel** button is there while it
   does),
2. builds the load: the header file, with the check values,
3. builds the members and the three list files,
4. writes the tree below the output directory, in a new folder named after the media set part
   number (if that folder already holds anything, `_2`, `_3` and so on is added instead, so
   nothing is ever overwritten), and
5. **reads the tree back from disk**, the way the loader does, and verifies every file and check
   value.

When it ends the receipt shows **COMPLETE** and the line "Media set ... written and verified.",
the numbers of data and support files, the bytes, the members and the folder, with an **Open
folder** button. If the read-back finds anything that differs from what the standard
*recommends* (but can still be read), the receipt lists those findings; they do not stop the set
being used. If something is wrong the receipt says **FAILED** and why. A failure before the tree is written
leaves nothing behind; one found by the read-back leaves the folder that was written, so look at
the message before using it.

![A finished generation](../images/gen-complete.png)

The folder you give to the loader's **Choose** button is the one the receipt names, for example
`.../generated/ACM-SET-0001`.

## How the files are placed

For a set of one member (the usual case) the member folder holds `LOADS.LUM`, `FILES.LUM` (and
`BATCHES.LUM` with the batch file) and one folder named after the load's part number without its
hyphens (`ACM4712345678`), which holds the header file, the data files and the support files:

```text
ACM-SET-0001/
  member_001/
    LOADS.LUM
    FILES.LUM
    ACM4712345678/
      ACM47.LUH
      NAVDATA.LUP
      APPCODE.LUP
      CONFIG.LDR
```

With more than one member, the header file and the support files stay on member 1, and the data
files are dealt out round-robin: the first to member 1, the second to member 2, and so on, each
inside a folder of the load's name on its member. Every member has its own `LOADS.LUM` and
`FILES.LUM`.

## The rules the generator enforces

The generator uses the same checks the loader uses to judge a set, so a form with no red text
makes a set the loader reads without complaint. Most are from ARINC 665-3; the section is in
brackets.

### Media set part number

A media set part number (3.1) is at most 15 characters, has no blanks, does not end in a hyphen,
and contains none of the letters `I`, `Q` and `Z` (the standard recommends against them).

### Load and batch part numbers

A load part number (2.1.1, 2.1.3), also used for batches, has the form `MMMCC-SSSS-SSSS`: 15
characters with a hyphen after the fifth and the tenth.

- `MMM` is the manufacturer's code: three upper-case letters or digits.
- `CC` are two **check characters**, two upper-case hexadecimal digits that must be the right ones
  for the rest of the number: the exclusive-or of the ASCII codes of the manufacturer's code and
  the product identifier (hyphens left out), shown as two hexadecimal digits. For `ACM` and
  `1234-5678` they are `47`, giving `ACM47-1234-5678`. If you type the wrong ones the window
  says so.
- `SSSS-SSSS` is the product identifier: upper-case letters and digits, without `I`, `O`, `Q` or
  `Z`.

![Errors shown as the fields are typed](../images/gen-errors.png)

### File names

All names (2.2.2): at most 255 characters, no blank or control character and nothing outside
printable ASCII, none of the characters `~ / : \`, and not `.` or `..`. On top of that:

| Name of | Extra rule |
| --- | --- |
| The header file | Ends in `.LUH`, and begins with a manufacturer's code (three upper-case letters or digits) |
| A data file | Must not use one of the extensions the standard keeps for itself, except `.LUP`, the recommended one |
| A support file | The same, but `.LDR` and `.HDR` are allowed, because `CONFIG.LDR` and a Boeing header file are support files |
| The batch file | Ends in `.LUB` (the window makes it for you) |

The reserved extensions are `CRC`, `DIR`, `HDR`, `LDR`, `LCI`, `LCL`, `LCS`, `LNA`, `LND`, `LNL`,
`LNO`, `LNR`, `LNS`, `LUB`, `LUH`, `LUI`, `LUM`, `LUP`, `LUR` and `LUS`. A data file called
`REPORT.LUM`, for example, is refused.

### Target hardware IDs

A target hardware ID (1.5) is 4 to 15 characters long and either four hexadecimal characters (an
ARINC 429 equipment code such as `2A4F`) or begins with a manufacturer's code (three upper-case
letters or digits, as in `HNPFMS`).

### Pathnames inside the set

You do not type these, but the generator and the loader check them: a file's pathname starts at
the root of the member with a backslash and ends with one, has no empty directory name, and is at
most four directories deep.

## What the generator does not do

- It writes one load per media set. A set with several loads, or a load spread over members in
  another pattern, is not made by this window (the library underneath can; the window does not
  offer it yet).
- It reads each file completely into memory, so the files are limited by the computer's memory.
- It does not change files you give it, and it never writes over anything.
- The window is English only.

## The About box

**About** shows the version, the build it came from, the licence and a **Licences** button that
lists the third-party components inside the program.

Next: [run the mock target](mock-target.md), or go straight to [the demonstration](demonstration.md).
