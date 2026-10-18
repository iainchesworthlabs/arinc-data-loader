# The media set generator

`src/media_gen` is the third Qt application: the preparation side of the workflow, where an
operator turns files from disk into an ARINC 665 media set — the load header, the data and
support files, and the media set's members — written as the directory tree (one directory per
member) that the data loader application opens and uploads to a target. It is the "second user
interface": one window runs the data loader, the other makes what the data loader loads.

Everything the format needs is worked out by `adl::arinc665` (`build_load`, `build_media_set`)
and `adl::media_tree` (`write_media_set_tree`): the generator decides the parts of the standard
that are a person's choice, and checks its own work by reading the set back before it reports
it done.

## What the window does

| What | How |
| --- | --- |
| Name the media set | The media set part number, the number of members (1 to 255), and the check value types recorded in the header file, in `FILES.LUM` and for the load (`adl::arinc665::CheckValueType`). |
| Describe the load | The load part number, the header file name (defaulted to the part number's first three characters and `.LUH`, and following the part number until edited), the load type, and the target hardware identifiers the header file names. |
| Choose the files | A multi-select file dialog fills a table; each row is a data file or a support file, with its name on the media set (editable) and the check value to record for it. Names are checked against the standard's rules (`adl::arinc665`'s naming checks) as they are typed, and duplicates are refused. |
| A batch, optionally | A batch file naming the loads of the set (one target per hardware identifier, each naming the set's load), with its own part number; its file name follows the same first-three-characters rule with `.LUB`. |
| Generate | Reads the files, builds the load, places it (header and support files on the first member; with more than one member the data files are spread round-robin, each in a subdirectory named after the load's part root directory), builds the members and writes the tree under the output directory in a fresh directory named after the media set part number (a counter is appended rather than overwrite). |
| Verify | Reads the set back from the disk it was written to (`read_media_set_tree`, `read_media_set`) and shows what that found: an error fails the generation; findings — where the set departs from the standard's recommendations — are listed as warnings. Only a set that read back clean is reported done. |
| The outcome | The directory the loader's "Choose directory…" opens, the file counts, the progress of the read-back verification, and the findings. |

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
```

## How it is put together

- **One job at a time, on a worker thread.** Generation can be slow (files are read whole), so
  it runs on a `QThread` like the loader's engine: one queued call, one `finished` signal,
  progress reported as bytes read, and a cancel that is an atomic flag polled between files.
- **Inputs are validated before anything is written.** The part numbers, header file name,
  hardware identifiers and file names are checked against the same functions the library uses
  to report where a set departs from the standard, and the Generate button stays disabled while
  any of them is wrong. Nothing is written until every input is valid, and nothing overwrites
  anything (`write_media_set_tree` refuses a non-empty member directory; the generator picks a
  fresh directory instead).
- **Generation is verification.** The write step is followed by reading the set back with the
  same functions the data loader reads it with (`read_media_set_tree`, `read_media_set`, and
  the loader's own summary in the tests), so a set the generator calls good is a set the loader
  can read. The standard's findings are surfaced, not hidden.

## What has and has not been checked

Checked: a generated set reads back with no findings and its load verifies; the generated
directory is accepted by the loader's own media set summary (the test links `adl_app_core`);
invalid part numbers, bad file names and duplicates are refused with named errors and nothing
is written; multi-member sets place the data files as promised; a cancelled job leaves nothing
behind; a batch, when asked for, is on the set. The window loads offscreen with no QML
complaints and generates through the model.

Not checked: media sets of many gigabytes (files are read whole; demo sizes were the target);
the window on a real screen at other scales; keyboard use and screen readers beyond the
accessible names.
