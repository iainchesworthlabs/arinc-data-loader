# Upload

Upload sends the ticked loads of a media set to the target. The target reads the files it needs
from the loader, one at a time, checks them, and reports its progress in status files.

## Before you start

The **Upload** view of the workbench says what is still needed. It has three **preflight tiles**:

| Tile | In place when |
| --- | --- |
| **Target** | an address and `THW_ID_POS` are set (chip **SET**), or **Find the target automatically** is on (**AUTO**). Otherwise **MISSING** |
| **Media set** | a set has been chosen and **CHECKED** |
| **Loads** | at least one load is ticked (**SET**), otherwise **NONE** |

A banner under the tiles says what to do next ("Choose a media set first", "The media set is not
valid", "Tick at least one load" with **Tick all loads**, "Choose or find a target" with **Find targets**), each with a button that does it, and, when
everything is in place, "Ready to upload". Only then is **Upload** (bottom right) enabled. The
plan line above says what will happen: "Upload 1 load to HNPFMS_L".

![Ready to upload](../images/app-workbench-ready.png)

Below the tiles the **payload** lists the ticked loads: part number, contents (header file name,
number of data and support files, the hardware it is for) and status (**TICKED**).

## While it runs

Press **Upload**. The window locks the things you could change (the ticks, the target, the switch),
the button says "Uploading...", and the action bar shows a progress bar and a percentage: how far the
target's list of loads has got, as the target reports it. Each ticked load shows its own
percentage and status in the rail ("waiting", then what the target says), and, if the target gives
up on a load, why. The log fills with what the target said and with each file the target reads
("The target read ACM47.LUH (220 bytes)").

![An upload in progress](../images/app-upload-running.png)

**Abort** asks the target to stop. The target is told, and the operation ends when it confirms
(or after 13 seconds without hearing from it). The result says whether the target confirmed.

## When it ends

The strip says **COMPLETE** or **FAILED**, and a **receipt** appears under the payload:

![A finished upload and its receipt](../images/app-upload-complete.png)

- the outcome: for example "completed", or "completed, 1 load failed" if the target accepted the
  operation but gave up on a load (that is a failure, and is shown as one), or "aborted by the
  operator";
- the detail: "The target read 3 files." (and whether an abort was confirmed);
- a line with the target, the media set and how many loads were ticked.

A failure also opens a dialog once. See [The log, the receipt and failures](log-and-receipt.md).

## Which files the target gets

A media set can hold several files of the same name, in different folders or on different members.
The target names the file it wants and, in its request, the check value of the one it means; the
loader serves the file of that name whose check value matches. If only one file has the name, it
is served whatever check value the request carries. If several have it and none matches, the
target is told the file is not found and ends the operation.

The files are read from the media set folder when the target asks, not copied beforehand.
Changing a file after the check is not noticed by the loader.

Next: [download](download.md).
