# Choose and check a media set

A media set, to the loader, is a **folder that holds one folder per member** (`member_001`, ...):
exactly what the generator writes and what `adl_write_payloads` writes for its samples (see
[Sample media sets](#sample-media-sets)). The chosen folder may hold only the `member_NNN`
folders: any other file or folder directly in it, such as `desktop.ini`, `Thumbs.db` or
`.DS_Store`, makes the whole set unreadable. If you choose the wrong level (a member folder, or a
folder above the set) the chip says **INVALID**; see
[Troubleshooting](troubleshooting.md#the-media-set-will-not-open-or-says-invalid).

Press **Choose folder...** in the **02 Media set** block, pick the folder, and the loader **reads and
checks** it straight away (unless you turned that off in [Preferences](preferences.md)). Reading
**is** checking: every file is opened, its length and its check values are verified, the three
list files of every member are compared with each other and with what is on the disk. The check
reads every file of the set whole into memory, so it needs memory for the whole set (see
[the limits](files-and-limits.md#limits)). The chip goes **CHECKING**, then **CHECKED** or
**INVALID**.

![A media set that has been checked](../images/app-workbench-ready.png)

## A set that passes

The rail shows the set's part number, the number of members and loads, "Every file and check value
was read and verified.", and each load with a check box. Tick the loads you want to upload
(**All** ticks every one).

**Warnings.** Findings are places where the set differs from what the standard *recommends* but
is still readable: for example a data file whose size is not a whole number of 16-bit words. They
are listed under a **WARNING** chip (the first three; **Show all warnings** shows the rest) and in
the set's properties. They do not stop an upload.

![A set with a warning](../images/app-set-warnings.png)

## A set that fails

If a file is missing, has the wrong length, or does not match its check value, or a list file is
damaged, the chip says **INVALID**, the rail and the Upload view show the reason (for example "CRC
mismatch" for a data file with a changed byte; that reason does not say which file), a dialog
opens once with that reason and the hint
"Choose the folder the media set was written to, and make sure every file of the set is still
in it", and **Upload stays dimmed**: nothing can be sent from a set that does not check.

![A set whose check failed](../images/app-set-invalid.png)

Fix the set (or choose another) and press **Re-check**.

## Re-checking

**Re-check** reads the whole set again, so changes made on disk after it was first read are
caught. What was checked is what is sent: when you press **Upload** the loader first reads the whole
set once more and compares it with what the check saw, and again as the target asks for each file.
A file edited, replaced or removed *after* the check is not sent. If it is found before the target
is contacted, nothing goes to the target; if it is found as the target asks for the file, that file
is refused and the upload ends. Either way the result says "the media set changed after it was
checked" and names the file, and the set counts as not checked again: press **Re-check**, then
upload.

## Sample media sets

`adl_write_payloads` is a command-line program, kept in the same folder as the three programs, that
writes sample media sets to a folder, so that the loader (or another loader) has something to open.
It has two forms:

```text
adl_write_payloads --list
adl_write_payloads [--scenario <name>]... <output-folder>
```

`--list` prints the name of each sample and a line about it. With no `--scenario`, every sample is
written; each `--scenario <name>` writes only the samples you name. A sample goes into
`<output-folder>/<name>/member_NNN`, is read back from the disk and verified, and the program
prints one line for it (members, files, loads, batches and findings, then the folder). It never
writes into a sample folder that already has files in it: that sample fails and the program stops.

## Properties

**Properties...** lists everything: see [Media set properties](preferences.md#media-set-properties).

Next: [upload](upload.md).
