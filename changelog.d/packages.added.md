**Packages.** `cmake --install` and CPack now produce what a release ships, per desktop: a *runtime*
package with the three Qt applications (`adl_app`, `adl_media_gen`, `adl_mock_target`),
`adl_write_payloads`, the Qt files they need to start on a machine with no Qt (placed by Qt's own deploy
support: windeployqt, macdeployqt, or a copy of the shared libraries on Linux), `README.txt`,
`LICENSE.txt` and the generated `NOTICES.txt`; and a *development* package with the libraries other
programs can build on (`adl::arinc665`, `adl::arinc615a`, `adl::arinc615a_runner`, `adl::media_tree`,
`adl::platform`), their headers and a CMake package (`find_package(adl)`). Windows gets a portable folder
as a zip, Linux a tar.gz with desktop entries and icons, macOS a disk image of the three application
bundles (signed ad hoc, which Apple silicon needs to run them), with each bundle carrying the licences
inside it. The macOS presets set a deployment target of 14.4, Qt 6.12's minimum, on purpose.
`tools/release/Test-Package.ps1` unpacks a package and starts every program from it with no Qt on the
path. [docs/releasing.md](docs/releasing.md) says what is shipped and what is not, and why.
