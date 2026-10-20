**The application: Upload** ([docs/app.md](docs/app.md)): tick loads of the media set that was read and
load them to the target with an `UploadOperation`, with a progress bar and a figure for the list, the
percentage and status of each load from the upload status files, the files the target reads in the log, and
the abort button. The files are read from the media set's directory tree (`MediaSetIndex` from `FILES.LUM`
and a `FileSource` over it, which tells two files of one name apart by the CRC in the target's request).
Not run against a real target.
