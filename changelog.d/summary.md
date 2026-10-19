The first preliminary release. This is a rebuild of an earlier Visual Studio project (a Win32
static library for ARINC 665, an empty one for ARINC 615A, and a C++/CLI unit test project) as a
C++23 CMake monorepo: the ARINC 665 and ARINC 615A libraries, a Qt 6.12 data loader, a media set
generator and a mock target, for Windows, Linux and macOS. It has not been run against a real
target, and the packages are not code-signed for any operating system's store (see
[Verifying a release](docs/verifying-releases.md) for the checks that are available).
