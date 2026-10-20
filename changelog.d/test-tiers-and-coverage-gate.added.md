**The test suite is now three labelled tiers, with a UI suite and a coverage
minimum.** Every CTest test is `unit`, `integration` or `system` (`ctest -L
unit`, and the `check-unit` / `check-integration` / `check-system` targets),
and the system tier includes smoke tests that start the three application
programs as processes. `tests/ui` is a Qt Quick Test suite that loads the real
`Main.qml` and drives its buttons, fields, check boxes and dialogs with Qt
Quick Test's mouse and key events against the real model and the simulated
target; it runs as `ui.tst_main_window` and `ui.tst_dialogs`. Coverage is
gated at **90% of lines and 80% of branches of the portable code** by
`cmake/CoverageReport.cmake` (the `coverage` target and the CI coverage job).
[docs/testing.md](docs/testing.md) describes the tiers, the UI suite and the
report.
