**The three programs can keep their settings somewhere else.** Set `ADL_SETTINGS_DIR` to a
directory and the loader, the media set generator and the mock target read and write an ini file
there instead of the registry key, the property list or the configuration file of their platform
(nothing is written to the usual place). `--smoke-test` makes a program draw its window once and
exit with code 0, which the system tests use to prove that each program shows its window.
[docs/app.md](docs/app.md#where-the-settings-are-kept) and [docs/testing.md](docs/testing.md)
describe both.
