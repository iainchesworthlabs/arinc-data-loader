# The media set generator

`src/media_gen` is the third Qt application: the preparation side of the workflow, where an
operator turns files from disk into an ARINC 665 media set — the load header, the data and
support files, and the media set's members — written as the folder tree (one folder per
member) that the data loader application opens and uploads to a target. It is the "second user
interface": one window runs the data loader, the other makes what the data loader loads.

Everything the format needs is worked out by `adl::arinc665` (`build_load`, `build_media_set`)
and `adl::media_tree` (`write_media_set_tree`): the generator decides the parts of the standard
that are a person's choice, and checks its own work by reading the set back before it reports
it done.

## What the window does

| What | How |
| --- | --- |
| Name the media set | The media set part number, the number of members (1 to 255), the output folder, and two check value types for the lists: the one `FILES.LUM` records for **each file it lists**, and the one for **`FILES.LUM` itself** (`adl::arinc665::CheckValueType`: none, 8-bit CRC, 16-bit CRC, 32-bit CRC, MD5, SHA-1). |
| Describe the load | The load part number, the header file name (defaulted to the part number's first three characters and `.LUH`, and following the part number until edited), the load type (none, or one of operational software, navigation database, aeronautical database and test software, with their codes), the load check value type, and the target hardware IDs the header file names (added and removed one at a time). |
| Choose the files | A multi-select file dialog fills a table, in the order they were added (Move up and Move down change it; Clear and Remove take files out). Each row is a data file or a support file, with its name on the media set (editable, defaulting to the file's own name), the check value the header file records for it and its size. Names are checked against the standard's rules (`adl::arinc665`'s naming checks, which differ for data and support files) and against the folder tree's stricter portable name rule (`is_portable_file_name`, which also refuses `* ? " < > |`, a trailing dot or space and device names) as they are typed, and a name used twice (in any case) is refused. The header file name and the batch file name are checked against both rules too. At least one data file is needed. |
| A batch, optionally | A batch file naming the loads of the set (one target per hardware ID, each naming the set's load), with its own part number; its file name follows the same first-three-characters rule with `.LUB`. |
| Generate | Reads the files, builds the load, places it (header and support files on the first member; with more than one member the data files are spread round-robin, each in a subfolder named after the load's part root directory), builds the members and writes the tree under the output folder in a fresh folder named after the media set part number (a counter is appended rather than overwrite). |
| Verify | Reads the set back from the disk it was written to (`read_media_set_tree`, `read_media_set`): an error fails the generation; findings (where the set departs from the standard's recommendations) do not fail it. A set that reads back without an error is reported done. When there are findings, the result says how many differences from the standard's recommendations were found ("N difference(s) from the standard's recommendations"); the list itself is not shown in the window. |
| The outcome | The folder the loader's "Choose folder..." opens (with an Open folder button), the file counts and sizes, and the number of findings, if any. The progress bar is the bytes read from the source files; writing and reading back are not measured. |
| The language | A language box in the header, beside About, offers the eight languages of the family (English, French, Spanish, German, Yiddish, Hebrew, Simplified Chinese, Japanese); the choice applies at once and is kept. Hebrew and Yiddish mirror the window ([Hebrew picture](images/gen-hebrew.png)). The catalogues are `src/media_gen/translations` and the shared `src/ui/translations`; the translations are machine drafts no speaker has read ([translations](translations.md)). The findings of the ARINC 665 library stay in English. |

## Layout

```text
src/media_gen/
  main.cpp                creates the QGuiApplication, one GeneratorModel and the QML engine
  qml/GeneratorWindow.qml the window
  core/                   adl_gen_core: everything but the window, Qt Core only
    source_file_model     the table of files the operator chose, with its validation
    generator_worker      the job on its own thread: read, build, place, write, verify
    generator_model       what the QML talks to: properties and a few invokable functions
tests/media_gen/          QtTest headless (including reading a generated set back through the
                          loader's own media set summary), and the window loaded offscreen
src/ui/                   the shared design system (Theme, controls, header, footer, log)
```

The window (1280×900, at least 1120×760) is a fixed left rail of numbered blocks (01 media set,
02 load, 03 batch) beside the artefact: the file table, the result banner and the fixed action bar.
The definition of the set does not scroll away with the file table; the file table is the part that
changes (the rail scrolls itself when the window is short). The action is always the filled button at
the bottom right (`Generate media set`, and `Cancel` beside it while a job runs), and the check-value
choice is visible as its own `NO CHECK VALUES` chip, once there are files, when the three pickers (each
listed file, `FILES.LUM` itself and the load) are all `none` and no row of the table has a check
value (`SourceFileModel::anyCheckValue`). The window follows the desktop's light or dark scheme, and remembers its size
and place.

### States and messages

The header chip is the one place the state is named: `NOT STARTED` (nothing entered), `INCOMPLETE`
(something is missing), `READY`, `GENERATING`, and then how the last job ended, `GENERATED`, `FAILED`
or `CANCELLED`. None of the pre-job states is an error, so none is red. The action bar and the footer
say other things: what is still needed (or how many files will be written), and what is left or what
happened. A fresh form shows no field message: a field's message appears once the operator has been
in it, or for all the required fields once `Generate media set` has been pressed on an incomplete form
(the button is then not dead; it marks what is missing and a warning banner lists it). The outcome of
a job is an `AppBanner` above the action bar: success (with `Open folder`), failure, or the neutral
note that the job was cancelled.

`GeneratorModel::resultState` is 0 (no job yet), 1 (written and verified), 2 (failed) or 3 (the
operator cancelled it; nothing was written). The window reads the state, not the text. Any later change to the form (a field, a file, the output folder) clears the outcome back to 0, so the
header and banner never describe a form that is no longer on screen. Changing the language is not a change to the form: the outcome stays, in the new language, with its `Open folder` button.

The file table's header and rows share one column model (the name takes the room that is left; kind,
check value, size and `Remove` are fixed widths), so they stay aligned at any window width.

## How it is put together

- **One job at a time, on a worker thread.** Generation can be slow (files are read whole), so
  it runs on a `QThread` like the loader's engine: one queued call, one `finished` signal,
  progress reported as bytes read, and a cancel that is an atomic flag polled between the steps.
- **Inputs are validated before anything is written.** The part numbers, header file name,
  hardware IDs and file names are checked against the same functions the library uses
  to report where a set departs from the standard (and, for a file name, against
  `is_portable_file_name`, the rule `adl::media_tree` applies when it writes), and Generate does not start
  while any of them is wrong. Nothing is written until every input is valid, and nothing overwrites
  anything (`write_media_set_tree` refuses a non-empty member folder; the generator picks a
  fresh folder instead).
- **The set appears under its name in one step.** The tree is written to `<part number>.writing` beside
  the final folder, read back and verified there, and only then renamed to its name (the first of
  `<part number>`, `<part number>_2`... that is free; an empty folder is taken over, a non-empty one
  never touched). Until then nothing is under the set's name, and whatever goes wrong (a full disk, a name
  the file system refuses, a set that does not verify, a cancel, running out of memory) removes the
  temporary folder, and the receipt says so ("Nothing was kept: what had been written was removed", or,
  if the removal itself failed, where the remains are). A file too large to be held in memory ends the job
  with its name, not a crash. `write_media_set_tree` itself also removes what it made when a write fails
  part of the way.
- **Generation is verification.** The write step is followed by reading the set back with the
  same functions the data loader reads it with (`read_media_set_tree`, `read_media_set`, and
  the loader's own summary in the tests), so a set the generator calls good is a set the loader
  can read. The number of findings is reported in the result; the findings themselves are held by
  the model (`validationFindings`) but the window does not show them.

## What has and has not been checked

Checked: a generated set reads back with no findings and its load verifies; the generated
folder is accepted by the loader's own media set summary (the test links `adl_app_core`);
invalid part numbers, bad file names and duplicates are refused with named errors and nothing
is written; multi-member sets place the data files as promised; a job cancelled while the files are
being read, or before the tree is written, leaves nothing behind and ends as `CANCELLED`, not `FAILED`; a batch, when asked for, is on the set.
The window loads offscreen with no QML complaints and generates through the model.

A cancel at each place the job can stop is tested through a hook (`GeneratorWorker::set_checkpoint_hook`):
before anything is written the receipt says nothing was written, and the job ends as CANCELLED; after the tree is written, and after it has
been verified, it says what was removed. A failing write is made with a file name no file system takes; a
failing allocation by throwing `std::bad_alloc` from the hook.

The window was also driven on a real Windows desktop by a script ([the screenshots](screenshots.md)):
the form filled in, files added through the system file dialog, a set generated and then opened and
uploaded by the loader against the mock target ([the demonstration](user-guide/demonstration.md)).

Not checked: media sets of many gigabytes (files are read whole; demo sizes were the target);
the window at scales other than 100%; keyboard use and screen readers beyond the accessible
names.
