# Messages and errors

This page lists the messages the programs show, with what each means and what to do. The texts are the English ones. `%1`, `%2` and so on stand for a name, a number or a reason that the program fills in, and `%n` for a count. In another language the words differ and the meaning does not.

Other pages hold the rest:

- The lines of the loader's activity log, the notices, the outcomes and the failure dialogs: [The log, the receipt and failures](log-and-receipt.md).
- The messages of the media set check, the *findings*: [Findings of the media set check](findings.md), with [Choose and check a media set](choose-a-media-set.md) and the coverage page for [ARINC 665-3](../arinc665-coverage.md).
- The command line and exit codes: [Command line and exit codes](command-line.md). The messages of `adl_make_set`, the command that makes a media set from a description file and says what one holds, are in [its section of that page](command-line.md#messages-of-adl_make_set): the sentences about a description, with the place in the file, and the ones about a source file, an output folder and a set that does not verify.
- Causes and cures by symptom: [Troubleshooting](troubleshooting.md).

A test (`docs.messages`) checks that every message of the programs' code that reports a problem or a condition appears in this guide, so a new message cannot be added without being described.

## The data loader

### Fields and checks before a job starts

| Message | When | What to do |
| --- | --- | --- |
| `"%1" is not an IPv4 address.` | The **Address** field holds something other than four numbers of 0 to 255 separated by dots. The chip says NOT SET and the buttons are dimmed. | Type the address as four numbers, for example `10.0.0.2`. The hint says: `Enter the target's IPv4 address as four numbers of 0 to 255 separated by dots, for example 10.0.0.2.` |
| `A THW_ID_POS is 1 to 240 characters, none of them a control character, a slash or a backslash.` | The **THW_ID_POS** field is empty, too long, or holds a character it may not. | Type the identifier as the target names it. |
| `A download goes into a folder named for the THW_ID_POS, so the name cannot hold a colon, an asterisk, a question mark, quotes, angle brackets or a bar.` | You chose Download, and the identifier has a character that a folder name cannot hold. | Use the target's identifier as it names itself. |
| `"%1" is not a THW_ID_POS a download can be made for.` | A download was started with an identifier that cannot name its folder. | The hint says: `Use the target's THW_ID_POS as it names itself: 1 to 240 characters, none of them a control character, a slash, a backslash or a character a folder name cannot hold (a colon, an asterisk, a question mark, quotes, angle brackets or a bar).` |
| `No media set has been read, so there is nothing to upload.` | You pressed **Upload** with no media set chosen and checked. | Choose a media set ([Choose and check a media set](choose-a-media-set.md)). |
| `No load was chosen.` | You pressed **Upload** and no load is ticked. | Tick at least one load. |
| `No file was named.` | You pressed **Download** in the media defined mode with no file name in the list. | Type a file name and press **Add**. |
| `"%1" is not a safe file name.` | A file name for a media defined download is not a plain name. | Name the file as the target knows it: a plain name of at most 254 characters, without a path. |
| `Choose the target from the list, then run the operation again.` | **Find the target automatically** is on, and more than one target answered, or none. | Choose a target from the list under **FOUND**, or type one, and start the operation again. |
| `FIND could not run (%1).` | The loader could not open the network for FIND. `%1` says why. | Check that no other program uses UDP port 1001 and that the system lets the loader use the network ([Troubleshooting](troubleshooting.md#a-port-is-in-use-or-not-allowed)). |

### While a job runs

| Message | When |
| --- | --- |
| `Looking for a target at %1 (three seconds)...` | A FIND started with an address typed. With none typed it goes to the broadcast address, and the log line is the one in [The log, the receipt and failures](log-and-receipt.md#lines-you-will-see). |
| `Reading the target's configuration...` | The header while an Information operation runs. |
| `Uploading to the target...` | The header while an upload runs. |
| `Downloading from the target...` | The header while a download runs. |
| `Stopping the operation...` | You pressed **Abort**. The loader is waiting for the target to confirm. |
| `Files received: %1 of %2 requested.` | A download ended. It is part of the receipt: how many files arrived of how many were asked for. |
| `%1, %2, all check values verified` | The summary of a media set that read cleanly: the loads and the files, and that every check value was checked. |
| `; %n warning(s) against the standard's recommendations` | Added to that summary when the check found departures from the standard's recommendations (see **Properties...**). They do not stop an upload. |
| `no target ID` | In a media set's list of loads: a load that names no target hardware ID. |

### When a job cannot start or fails

| Message | When | What to do |
| --- | --- | --- |
| `The operation could not run: %1.` | The loader could not start the operation. `%1` is one of the two reasons below, or a system reason. | See the reason. |
| `the request was refused (%1)` | The reason in the message above: the target or the system refused the request. `%1` says which. | Check the address, the identifier and the target. |
| `waiting for the network failed (%1)` | The reason in the message above: the loader could not wait for the network. | Check the network and the firewall, then try again. |
| `The media set changed after it was checked: %1. (The upload ended: %2.)` | A file of the media set was edited, replaced or removed after the check, and the upload was refused or stopped. `%1` names the file. | Check the media set again (**Re-check**), then upload ([Upload](upload.md#which-files-the-target-gets)). |
| `The media set could not be checked: %1.` | The loader could not read the media set. `%1` says why. | Choose the folder that holds the `member_001` folders, and make sure every file of the set is still there. |
| `The system services could not be started (%1).` | The loader could not start the layer that gives it files, the clock and the network. | Close the loader and start it again. If it happens again, report it ([Support](../support.md)). |
| `%1 is not a folder` | The download folder in Preferences is a file. | Choose a folder. |
| `cannot look at %1 (%2)` | The loader could not look at the download folder. `%2` says why. | Choose a folder that you can read. |
| `cannot create %1 (%2)` | The loader could not make the download folder or the folder for a download. | Choose a folder that exists or can be made, and that you can write to. |
| `there are already %1 download folders for %2 in %3` | The loader looked for a free `DNLD_DATA_<THW_ID_POS>_<number>` name and used up its numbers. | Move or delete old download folders, or choose another download folder. |

### Words for what the target or a transfer reported

These words appear in the log and in the detail of a failure, for example `Status 3: completed` or `transfer failed: HNPFMS_L.LCI (timed out)`.

The words for a status code the target sent:

| Code | Word | Meaning |
| --- | --- | --- |
| 0x0001 | accepted | The target accepted the operation and has not started |
| 0x0002 | in progress | |
| 0x0003 | completed | Finished without error |
| 0x0004 | in progress, with details | In progress, and the target's description says more |
| 0x1000 | denied | The target did not accept the operation, and says why |
| 0x1002 | not supported | The target does not do the operation |
| 0x1003 | aborted by the target | The target stopped, and says why |
| 0x1004 | aborted by the data loader | The target received an ABORT from the protocol |
| 0x1005 | aborted by the operator | The target received an ABORT from the operator |
| 0x1007 | failed | One load, or one file, failed, and the description says why |
| any other | unknown status code | A code this version does not know |

The words for how one file transfer ended:

| Word | Meaning |
| --- | --- |
| succeeded | The transfer finished |
| timed out | No answer after the time-out and the retry |
| the peer sent an error | The other end answered with an error |
| the peer aborted | The other end aborted the transfer |
| the peer asked to wait | The other end answered WAIT. The loader asks again |
| options refused | The other end did not take the transfer options (block size and so on) |
| protocol error | A packet that did not belong |
| transfer size mismatch | The size the other end announced and the size that arrived differ |
| local file error | The loader could not read or write a file it needed |

A notice may end with a number. `target asked to wait` and `file not available` end with ` (%1 s)`, the seconds the target asked for. `port option not taken by the target` ends with ` (port %1)`, the port the loader asked the target to use. `transfer failed` and `status file transfer failed` end with one of the words above in brackets.

## The media set generator

### In the form

| Message | When | What to do |
| --- | --- | --- |
| `Enter the media set part number, for example ACM-SET-0001.` | The media set part number is empty or not valid. | Enter it. |
| `Enter the load part number, for example ACM47-1234-5678.` | The load part number is empty or not valid. | Enter it in the form `MMMCC-SSSS-SSSS` ([the rules](make-a-media-set.md#the-rules-the-generator-enforces)). |
| `Enter the header file name, for example ACM47.LUH.` | The header file name is empty or not valid. | Enter a name that begins with the manufacturer's code and ends in `.LUH`. |
| `Enter the batch part number, for example ACM4C-0100-0200.` | **Include a batch file** is ticked and its part number is empty or not valid. | Enter it, or untick the box. |
| `Choose an output folder.` | No output folder is chosen. | Press **Choose folder...**. |
| `The output folder does not exist.` | The folder you chose is not there. | Make the folder, or choose another. |
| `Choose at least one data file.` | No data file is in the list. | Press **Add files...**. A load needs at least one data file. |
| `Fix the marked files.` | A file in the list has a problem, shown on its row. | Correct the row. |
| `Cannot generate: fix the marked fields first.` | You pressed **Generate media set** while a field is marked. | Correct the marked fields. |
| `The name is empty.` | A file's name in the list is empty. | Give it a name. |
| `Another file has this name.` | Two files in the list have one name. | Rename one of them. |

### When it generates

| Message | When | What to do |
| --- | --- | --- |
| `No data files chosen.` | The job started with no data file. | Add a data file. |
| `The load could not be built: %1.` | The load could not be made from the files and fields. `%1` says why. | Correct what `%1` names. If it is not clear, report it. |
| `The media set could not be built: %1.` | The media set could not be made from the load. | As above. |
| `The media set could not be written below %1.` | The folder could not be written. | Check that the folder is writable and the disk is not full. |
| `Nothing was kept: what had been written was removed.` | The generator stopped part way and removed what it had written. | Fix the cause above, and generate again. |
| `What had been written could not be removed, and is still in %1.` | The generator stopped and could not remove its temporary folder, which is named `<part number>.writing`. | Delete that folder yourself. |
| `Too many unfinished sets (%1.writing...) are there already.` | The output folder holds many folders left by earlier runs that could not finish. | Delete the folders named `<part number>.writing` and `.writing_2` and so on. |
| `%1 is too large to be read into memory.` | A source file is larger than the generator can read whole. | Choose smaller files. |
| `The computer ran out of memory while the media set was being made.` | There was not enough memory. | Choose smaller files, or close other programs and try again. |
| `Nothing was kept. Choose smaller files, or close other programs and try again.` | The hint that goes with the two messages above. | As it says. |
| `The media set could not be made.` | The generator failed for a reason it could not name. | Try again, and report it if it happens again. |
| `Nothing was written. Choose smaller files, or close other programs and try again.` | The hint that goes with it. | As it says. |
| `The written media set could not be read back: %1 (%2).` | The generator wrote the set and could not read it back. | Check the disk. If it happens again, report it. |
| `The written media set does not verify: %1.` | The set was written and does not pass the check. | A defect in the program: report it, with the files that caused it if you can. |
| `%1, %2, %3 bytes in %4, in %5.` | The line that says the set was written and verified: the counts, the bytes, the members and the folder. | Nothing: it is the success banner. |

## The mock target

| Message | When | What to do |
| --- | --- | --- |
| `The storage folder is not set, so the upload is not stored.` | A load was uploaded and no storage folder is chosen. The mock target still answers. | Choose a storage folder before you start it. |
| `The storage folder could not be made: %1.` | The folder cannot be created. | Choose a folder that exists or can be made. |
| `The load's folder could not be made: %1.` | The folder for the load, inside the storage folder, cannot be created. | Check that the storage folder is writable. |
| `%1 was not stored: not a safe file name.` | The uploaded file's name is not a plain name, so it was not kept. | None: the mock target keeps only letters, digits, `.`, `_` and `-` in a name. |
| `%1 could not be written to the storage folder.` | A file could not be written. | Check that the folder is writable and the disk is not full. |
| `Scenario %1: %n step(s)` | A scenario file was loaded ([the window](mock-target.md#a-scenario-file)): its name, or the file's name when it has none, and the number of its steps. | Nothing: the faults above it are not used while it is loaded. |
| `The scenario file was refused: %1` | The file chosen could not be used. `%1` is the sentence that says why. | Read the sentence in [A file that is refused](mock-scenarios.md#a-file-that-is-refused), correct the file and choose it again. |
