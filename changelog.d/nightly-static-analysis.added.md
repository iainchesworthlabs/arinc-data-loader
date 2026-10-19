**A nightly static-analysis suite** ([docs/static-analysis.md](docs/static-analysis.md)):
`static-analysis.yml` runs the pinned clang-tidy (with the Clang Static
Analyzer, warnings as errors) over `src/`, MSVC Code
Analysis (`/analyze /WX`) over the libraries, tests and the three Qt
applications, a Clang ASan/UBSan build and test run, and an optional
SonarQube scan; CodeQL moves to the same nightly schedule. The analyzers are
not required checks and do not run on every commit. New presets
`linux-gcc-tidy`, `windows-msvc-analyze`, `linux-clang-debug` and
`linux-gcc-sonar` run the same work locally, and `ADL_ENABLE_CLANG_TIDY` /
`ADL_ENABLE_MSVC_ANALYSIS` switch the analyzers in a build. `.clang-tidy` is
now the gate CI holds, with each disabled check carrying its reason.
