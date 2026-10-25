# Nightly static and dynamic analysis

The per-commit CI ([CI](ci.md)) builds and tests the tree on three compilers and
runs its tests under AddressSanitizer, and under UndefinedBehaviorSanitizer where
the compiler has it (GCC and AppleClang, not MSVC). It is not
the place for a whole-program analyzer: clang-tidy over the libraries,
MSVC's Code Analysis and a CodeQL build each cost more than the rest of the
matrix together, and a defect of the kind they find does not get worse by
waiting for the night.

So the analyzers run on a **nightly schedule**, together, in
`.github/workflows/static-analysis.yml` (and CodeQL in `codeql.yml` at the same
hour). None of them is a required check. A red night is a thing to look at in
the morning, not a blocked pull request; the required checks stay the ones in
[repository settings](repository-settings.md).

GitHub starts a scheduled workflow, and registers a manually dispatched one, only
from the repository's default branch, which is `develop`. A workflow that is not on
`develop` does not run, and `gh workflow run` reports that it is not found; see
[CI](ci.md#nightly-static-analysis).

## Schedule

| Workflow | When |
| --- | --- |
| `static-analysis.yml` | `17 13 * * *` UTC - just after midnight in Sydney in daylight saving, late evening in standard time |
| `codeql.yml` | the same hour; CodeQL has its own workflow for its `security-events: write` permission |

Both also run on `workflow_dispatch`, and a manual run is a full run:

```sh
gh workflow run static-analysis.yml --ref develop
gh workflow run codeql.yml --ref develop
```

A manual run supersedes an older one of the same workflow (`cancel-in-progress`):
the tree that finishes must be the newest tree.

## The jobs

| Job (display name) | Workflow | Runs on | What a green run proves |
| --- | --- | --- | --- |
| `clang-tidy (Linux, GCC)` | `static-analysis.yml` | ubuntu-26.04 | Every translation unit under `src/` was compiled through the pinned clang-tidy with `.clang-tidy`, the Clang Static Analyzer checks included, and `--warnings-as-errors=*`: no finding survived. |
| `MSVC Code Analysis (Windows)` | `static-analysis.yml` | windows-2025-vs2026 | The libraries, the Catch2 tests and the three Qt applications were compiled with `/analyze /WX`, so MSVC's path-sensitive analysis and its Windows-specific rules found nothing in first-party code. |
| `Clang ASan/UBSan (Linux)` | `static-analysis.yml` | ubuntu-26.04 | The libraries and tests build with `clang++` and run under clang's AddressSanitizer and UndefinedBehaviorSanitizer. The per-commit Linux leg uses GCC; this one is the Clang UBSan run MSVC cannot provide and AppleClang is not asked for nightly. |
| `Analyze (cpp)`, `Analyze (python)`, `Analyze (actions)` | `codeql.yml` | ubuntu-26.04 | CodeQL's queries ran over the build (C++) or the files (the others). Alerts appear under **Security -> Code scanning**; the job itself does not fail for an alert. |
| `SonarQube` | `static-analysis.yml` | ubuntu-26.04 | Optional and skipped until configured: the Sonar C++ analyzer scanned `src/` from the compile database and published to the configured server. Report only; it does not wait for the quality gate. |

### clang-tidy

`cmake/StaticAnalysis.cmake` puts the pinned clang-tidy in
`CMAKE_CXX_CLANG_TIDY`, so it is the compiler launcher for every first-party
target under `src/`: the build is the analysis. `linux-gcc-tidy` selects it,
turns the sanitizers off (they are not needed to parse), leaves the tests and
the Qt applications out, and turns CMake's C++ module scanning off so the
compile database carries no `-fmodules-ts` arguments clang-tidy does not
understand. Only `src/` is tidied: Catch2's `REQUIRE`/`CHECK` macros expand to
code the checks cannot see through (a `REQUIRE(has_value())` followed by
`*value` is the house style), and the sanitizer legs run the tests. The Qt
application is covered by MSVC Code Analysis and by the per-commit Qt tests;
adding Qt to this leg would install a desktop kit on Linux for code that is
already built under sanitizers on every commit.

The checks are in `.clang-tidy`, and the list is the families that find defects -
the Clang Static Analyzer, bug, cert, concurrency, performance and portability.
Style is not there: `.clang-format` and the compiler warning set own it. Each
disabled check has its reason in the comment block above the list.
`WarningsAsErrors` is empty there (a local editor should show a hint, not fail a
save); the CMake option adds `--warnings-as-errors=*`, which is what makes the
nightly build red.

The version is pinned in `.github/requirements/static-analysis.txt`
(`clang-tidy==23.1.3`, the PyPI wheel), not taken from the runner image: a newer
clang-tidy adds checks, and the gate must not change under an unchanged tree.

### MSVC Code Analysis

`-DADL_ENABLE_MSVC_ANALYSIS=ON` (the `windows-msvc-analyze` preset) adds
`/analyze /analyze:external-` to the warning interface every first-party target
links, and the warnings are already errors (`/WX`). It analyzes the libraries,
the tests and the three Qt applications; Qt's headers are external, so their
warnings are not this project's. Two known non-findings are handled at their
source rather than by turning the warning off everywhere:

- C6101 from the Windows SDK's inline `WSAGetIPUserMtu` (the analyzer does not
  model `getsockopt` writing its out parameter) is disabled for
  `network_windows.cpp`, the one file that includes that header.
- C6258 on `TerminateThread` in `threads_windows.cpp` carries a
  `#pragma warning(suppress : 6258)` with the reason: the thread was created
  suspended and `ResumeThread` failed, so it has never run.

The big test fixture stack frame the analyzer reported (C6262) was moved to the
heap in the test, rather than raising the analyzer's threshold.

### Clang ASan/UBSan

`linux-clang-debug` is a preset of its own, not a change to the per-commit legs: it
selects `clang++`, keeps the Debug sanitizer flags from
`cmake/Sanitizers.cmake` (`-fsanitize=address,undefined
-fno-sanitize-recover=all`) and runs the libraries' and tests' `ctest` suite.
`ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=print_stacktrace=1` are set on
the test step. The applications are off; Qt is not installed on this leg.

### CodeQL

CodeQL runs through the shared workflow of `icl-common-infra` for C++, Python and the GitHub Actions
workflows. The C++ build is the lean `linux-codeql` preset: the libraries and the platform layer, with
no tests, no applications, no sanitizers and no module scanning. CodeQL traces the compiler itself, and
its build tracer sets an `LD_PRELOAD` that collides with ASan. Results are triaged in the repository's
code-scanning alerts. It is not a required check: an alert is triaged, not merged. It runs nightly and on a
ready pull request that changes `libs/`, `apps/`, `cmake/`, `tools/`, the CMake files or the workflows
(not on Dependabot's pull requests, whose token cannot upload results), so that the OpenSSF Scorecard SAST
check sees an analysis on each change. A failed nightly analysis opens or updates the `nightly-analysis`
issue; a failed analysis on a pull request does not.

### SonarQube (optional)

The Sonar job is off until an administrator turns it on. It is skipped, not red,
while `SONAR_HOST_URL` is unset. Set the variable and the secret together: the
variable alone turns the job on, and the scan then fails for want of a token.

1. Create the project on SonarQube Cloud (free for public projects) or a
   self-hosted SonarQube with the C++ analyzer (a commercial edition asks for
   it), and copy its token.
2. Add the repository variable `SONAR_HOST_URL` - `https://sonarcloud.io` for
   the cloud - and the secret `SONAR_TOKEN`.
3. Edit `sonar-project.properties` for the project key, organization and server
   layout. The cloud key is `iainchesworthlabs_arinc-data-loader`; a
   self-hosted server uses its own key and removes `sonar.organization`.
4. Run the workflow by hand once and read the scan. The job does not wait for
   the quality gate (`sonar.qualitygate.wait` is not set): the first scan of a
   tree publishes its findings rather than failing the night. Make the gate a
   required check only after it has reported and the backlog is understood,
   the way `Scan dependency diff` was staged.

The C++ analyzer reads `compile_commands.json` from the configure-only
`linux-gcc-sonar` preset; it does not need the objects or a build. The scan is
of `src/`; tests are not scanned.

## Running the analyzers by hand

The pinned clang-tidy and the CMake presets are the same ones CI uses.

```sh
# clang-tidy, Linux (or WSL), the same configure/build as the nightly job
python -m venv .venv-tidy
. .venv-tidy/bin/activate
pip install -r .github/requirements/static-analysis.txt
cmake --preset linux-gcc-tidy
cmake --build --preset linux-gcc-tidy

# Clang ASan/UBSan, Linux
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug

# MSVC Code Analysis, Windows Developer PowerShell
cmake --preset windows-msvc-analyze
cmake --build --preset windows-msvc-analyze

# Sonar compile database, no build
cmake --preset linux-gcc-sonar
```

A local `clang-tidy` of a single file, against a configured tree, is the fast
loop; it uses the same `.clang-tidy`:

```sh
clang-tidy -p build/linux-gcc-tidy libs/arinc665/src/load.cpp
```

On Windows, `clang-tidy -p build/windows-msvc-debug ...` parses an MSVC compile
database, but the Debug build carries ASan flags that clang-tidy's driver
rejects (`-MDd` with `-fsanitize=address`); configure a sanitizer-off tree, as
the `linux-gcc-tidy` preset does, or use the analyzer build above.

## Findings and suppressions

The rule is the same as for a compiler warning: **fix it rather than silence
it**. When the finding is wrong, the exception is narrow and says why:

- clang-tidy: a check is disabled in `.clang-tidy` with a comment naming the
  style or the invariant; a single expression can carry
  `// NOLINT(check-name)` with the reason. Three do: the Windows SDK's
  `FIONBIO` macro changes signedness inside `_IOW` (a local Windows run sees a
  `clang-diagnostic-sign-conversion` the Linux job never compiles), `readdir`
  is safe on a `DIR` stream local to the call, and value-initialising the slot
  array zero-fills before the member initialisers run.
- MSVC: `#pragma warning(suppress : <id>)` on the expression, or
  `/wd<id>` for one source file where its target is added (the C6101 case is in
  `libs/platform/CMakeLists.txt`) - not for the project.
- Never a global `/wd` or a new disabled check without the reason in the file
  and a line here.

The `clang-tidy` log is uploaded as the `clang-tidy-log` artifact when the job
fails, and the MSVC log as `msvc-analysis-log`; otherwise the build log is the
job's log.

## What the night does not cover

- **clang-tidy on the tests.** They are not tidied: Catch2's macros expand to
  code the checks cannot see through, and the sanitizer legs run them. A test
  with a real defect is still caught by ASan/UBSan.
- **clang-tidy on Windows and macOS platform code.** The Linux job builds the
  Linux platform section; the Windows section is covered by MSVC Code Analysis,
  and the macOS section by the per-commit AppleClang build with warnings as
  errors. A second clang-tidy matrix would cover both, at a macOS-billed job per
  night; it can be added when the macOS platform layer changes often.
- **CodeQL on the applications.** The lean CodeQL build has no Qt. The C++ that
  carries the protocol and the formats is the libraries and platform section,
  which it does build.
- **Release-only warnings.** They are found by `release.yml`, which builds the
  Release presets ([CI](ci.md#toolchain-choices)), not by a nightly analyzer. The
  coverage minimum is enforced by the coverage job, not by a nightly analyzer.
- **The Qt application under ASan on Linux.** The per-commit Linux leg builds it
  in Debug with GCC's sanitizers; the nightly Clang leg does not install Qt.
