**The applications build by default on the desktops** ([docs/building.md](docs/building.md)):
`ADL_BUILD_APP` defaults to on for Windows, Linux and macOS (the source side, and the only place
Qt exists) when the Qt kit can be found, and off when `ADL_TARGET_PLATFORM` names a target or the
kit is absent — so a desktop without Qt configures the libraries alone instead of failing, and
`-DADL_BUILD_APP=ON` with no kit is still an error. The desktop presets therefore contain
`adl_app`, `adl_media_gen`, `adl_mock_target` and their tests without a flag, and an IDE that
does not run the labs' environment script finds the shared kit under `C:/Repos/common/qt` (the
Windows fragment's search roots) rather than failing to generate. The configure output names the
three programs, and the building page has the source-side/target-side matrix: the libraries, the
host tools, the source-side applications and the (still planned) target-side application.
