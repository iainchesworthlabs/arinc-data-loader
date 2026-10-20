**The application, first slice** (`src/app`, [docs/app.md](docs/app.md)): a Qt 6.12 (Qt Quick) window
to choose a media set directory (read and shown: its part number, loads and any differences from the
standard), run FIND and take a target from the list or type one, and run an Information operation with an
abort button, the target's status files and the engine's notices in an activity log, and the hardware and
parts it reports. The engine runs on a thread of its own, reporting through queued signals and aborted
through an atomic flag the runner polls. Built with `-DADL_BUILD_APP=ON` (Qt is the prebuilt kit, found
through `QT_ROOT_DIR`); CI installs it on the three desktop legs. Not run against a real target.
