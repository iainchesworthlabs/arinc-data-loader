The first preliminary release. This is a rebuild of an earlier Visual Studio project (a Win32
static library for ARINC 665, an empty one for ARINC 615A, and a C++/CLI unit test project) as a
C++23 CMake monorepo: the ARINC 665 and ARINC 615A libraries, a Qt 6.12 data loader, a media set
generator and a mock target, for Windows, Linux and macOS. It has not been run against a real
target, and the packages are not code-signed for any operating system's store (see
[Verifying a release](docs/verifying-releases.md) for the checks that are available).

### Known limitations

- **Nothing has been checked against a real target, a real loader or another tool's media
  sets.** The protocol and the file formats are tested against simulated targets, the mock target
  and independent writers of the same tables. See
  [the user's guide](docs/user-guide/files-and-limits.md#what-has-not-been-checked).
- Only Windows (MSVC) has been driven by hand; Linux and macOS build and test in CI, and their
  packages are started headless by the release workflow.
- The loader is the only translated window, and only 148 of its 515 messages are translated in
  each of the seven non-English catalogues; the translations are a first, mechanical draft.
- On Windows the Debug presets run under AddressSanitizer, which stops the system file dialogs
  ("Choose...") from working; use a Release build, or the package, for the windows.
