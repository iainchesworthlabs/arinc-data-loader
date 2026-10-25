# CI, text policy and local hooks

This page is what runs on every change, what each job proves, and what keeps the rules when GitHub
cannot. The policy checks, the change planner and the setup actions are shared with the organisation's
other repositories and live in
[icl-common-infra](https://github.com/iainchesworthlabs/icl-common-infra) (its usage page
describes them). The settings above the workflows (rulesets, labels, the merge queue) are in
[Repository settings](repository-settings.md).

## What enforces what

GitHub blocks a merge or a push that breaks a rule once the rulesets are applied. The same rules are
also kept before a push, locally.

| Rule | Kept by | Also kept locally by |
| --- | --- | --- |
| Builds, tests, formatting, platform isolation, documentation | `CI Status` (the jobs of `ci.yml`) | `ctest` (includes the isolation and documentation checks) |
| Branch names (`feature/`, `bugfix/`, `docs/`, `chore/`, `agent/`) | the Policy job; the `branch-names` ruleset refuses the push | `tools/hooks/pre-push` |
| No attribution in commit and pull request text | the Policy job | `tools/hooks/commit-msg` |
| A user-visible change has a changelog fragment | the Policy job | `python3 tools/ci/precheck.py` |
| SPDX licence and REUSE, action pinning, preprocessor rule, vcpkg baseline in the fork | the Policy job | `python3 tools/ci/precheck.py` |
| Pull request required, no direct push, no force push | the `develop-protection` ruleset | - |
| Merge queue in front of `develop` | the `merge-queue-develop` ruleset | - |

Install the hooks once per clone with `git config core.hooksPath tools/hooks`. Run
`python3 tools/ci/precheck.py` before a push: it finds `icl-common-infra` next to this repository (or in
`ICL_COMMON_INFRA`) and runs the Policy checks, and tells you what the gate will plan. It reads committed
work only.

## Workflows

| Workflow | Purpose | Runs on a private copy? |
| --- | --- | --- |
| `ci.yml` | The gate: Policy, the planner, `clang-format`, `platform-isolation`, the documentation build, the build legs, coverage, workflow analysis, the dependency scan, and the aggregator `CI Status`; the RTOS simulators run beside it ([The jobs](#the-jobs-in-ciyml)) | yes |
| `fuzz.yml` | Fuzzes each target for 60 seconds on a push to `develop` and for ten minutes each night; a crash fails the run and the nightly one opens an issue ([Fuzzing](fuzzing.md)) | yes |
| `clusterfuzzlite.yml` | Fuzzes the code a pull request changes for ten minutes with ClusterFuzzLite; not a required check ([Fuzzing](fuzzing.md#in-ci)) | yes |
| `docs.yml` | Publishes the documentation site to GitHub Pages when `main` moves ([Documentation site](#documentation-site)) | yes |
| `codeql.yml` | CodeQL static analysis of the C++, Python and workflows, nightly and on a ready pull request that changes them; not a required check | no - skipped |
| `release.yml` | The Release build on the three systems, packages, SBOM, signing, attestations and the draft release; a rehearsal on manual runs and on packaging pull requests ([Releasing](releasing.md)) | yes |
| `static-analysis.yml` | The nightly clang-tidy, MSVC Code Analysis, Clang ASan/UBSan and optional SonarQube runs | yes |
| `scorecard.yml` | OpenSSF Scorecard score, from the default branch | no - skipped |
| `dependabot-auto-merge.yml` | Queues Dependabot's minor and patch updates of the Actions and the CI tooling once the checks pass | yes |
| Dependabot (`.github/dependabot.yml`) | Actions pins, the pinned CI tooling, the ClusterFuzzLite base image, the vcpkg baseline | yes |

Every workflow declares `permissions: {}` at the top and gives each job only what it names; every
checkout sets `persist-credentials: false`; every third-party action, and every reusable workflow and
action of icl-common-infra, is pinned to a full commit SHA with its version in a trailing comment (the
Policy job checks it); pull request data reaches scripts through `env:`, never by interpolating
`${{ }}` into a shell. `zizmor` analyses the workflows on every pull request.

### Public-only workflows

`codeql.yml` and `scorecard.yml` need a public repository (or a paid plan). Each of their jobs carries
`if: ${{ !github.event.repository.private }}`, so they are **skipped, not red**, while the repository is
private and run once it is public. `Scan dependency diff` needs the repository's dependency graph
switched on.

## Nightly static analysis

A schedule, and a manual dispatch, work only for a workflow on the default branch (`develop`). The
same holds for everything GitHub reads only from the default branch: Dependabot
(`.github/dependabot.yml`), `scorecard.yml`, the issue templates and `SECURITY.md`'s
private-reporting link.

`static-analysis.yml` and `codeql.yml` run on a nightly schedule, not on a
push; `codeql.yml` also runs on a ready pull request that changes the code it scans. Neither is a required check: a red night is read in
the morning, and a merge is not held for an analyzer. The jobs are clang-tidy
over the libraries (`libs/` and `apps/`), MSVC Code Analysis over the libraries, tests and
Qt applications, a Clang ASan/UBSan build and test run, CodeQL's C++ queries,
and an optional SonarQube scan that is skipped until it is configured. The
reason for each, the local presets and the suppression rules are in
[Nightly static and dynamic analysis](static-analysis.md).

## Documentation site

The pages in `docs/` are built into a static site with MkDocs and Material (`mkdocs.yml` at the root of the
repository) and, from the first release on, published to GitHub Pages at <https://iainchesworthlabs.github.io/arinc-data-loader/>. The
Markdown stays the source and stays readable on GitHub; nothing in a page is written for the site.

Locally:

```sh
python -m pip install --require-hashes -r .github/requirements/docs.txt
mkdocs serve                                  # read it while editing
mkdocs build --strict                         # what CI runs; writes ./site
cmake --build <build dir> --target docs       # the same, into <build dir>/site
```

**What strict means here.** MkDocs' own strict mode fails on warnings, but it reports a link to a missing
page, a link to a heading that is not there and a page missing from the navigation at a level that strict
mode lets through. The `validation` block of `mkdocs.yml` raises all of them to warnings, so each fails the
build. A page that is added to `docs/` must also be added to `nav` in `mkdocs.yml`.

**Links out of `docs/`.** A page may link to the README, the changelog or a source file with a relative link
(`../README.md`), which works on GitHub. The site has no such file, so `tools/docs/mkdocs_hooks.py` turns the
link into one to the file on GitHub (`blob/main/...`, or `tree/main/...` for a folder). A link whose target
does not exist is left alone, and the build fails on it.

**Icon and navigation.** The logo and the favicon are the loader's icon. The same hook adds it to the site, as
`images/icon.svg`, from `packaging/linux/icons/adl_app.svg`, where the Linux package takes it, so there is one
copy. `navigation.instant` loads a page without reloading the site and `navigation.tracking` keeps the address
bar on the heading being read; both depend on `site_url`, so a built site opened from disk or served from
another address navigates the ordinary way (`mkdocs serve` sets `site_url` to its own address).

**What is not published.** The standards (`docs/specs/*.pdf`) are third-party documents ([Licensing](licensing.md));
`exclude_docs` leaves them out and the build fails if a PDF is in the site.

**In CI.** `.github/actions/docs-site` installs MkDocs from the hash-locked `.github/requirements/docs.txt`
(`uv pip install --require-hashes --no-build`), builds strictly, and checks the result (the home page and the
user's guide exist, no PDF, a minimum number of pages). Two workflows use it, so they cannot disagree:

- `ci.yml`, job `Docs build (strict)`: on every pull request and queue entry. It has no path filter, because
  a required check that does not run blocks the pull request. After the build it runs
  `tools/checks/check_doc_links.ps1` and `tools/checks/check_doc_paths.ps1`, the scripts behind the CTest
  tests `docs.links` and `docs.paths`, over every Markdown file, so that a change to documentation alone,
  which runs no CTest ([above](#the-planner-and-documentation-only-changes)), is still checked for links,
  anchors and paths, and so is `README.md`, which the site does not contain.
- `docs.yml`: when `main` moves (the release bot's fast-forward after a release), it builds the site again,
  packages it, and the job `Publish the site` deploys it with `actions/deploy-pages`.
- `release.yml`, job `Docs (strict)`: the first job after *Prepare*, ahead of the three builds, so a release
  with a broken page stops in a minute and not after an hour.

**Publishing** uses Pages with the *GitHub Actions* source. A `gh-pages` branch is not an option here: the
`branch-names` ruleset refuses a branch with a name outside its list. The deploying job runs in the
`github-pages` environment (deployment branches: `main` only) and waits for the repository variable
`PUBLISH_DOCS` to be `true`, so a repository that has not switched Pages on gets no failed run
([Releasing](releasing.md#setup)). Because `main` moves only with a
release, the published site is the documentation of the latest release. There is one version of the site;
old releases' pages are in the tag's tree on GitHub.

**Not used.** Zensical, the successor to Material for MkDocs, reads the same `mkdocs.yml`. It was tried and
does not run MkDocs hooks, so every link out of `docs/` fails its strict build. MkDocs is held below 2.0,
which removes hooks and the theme API this configuration uses.

## The release workflow

`release.yml` is described in [Releasing](releasing.md) and checked from the outside by
[Verifying a release](verifying-releases.md). Where it sits among the others:

- It is not a required check and carries no `merge_group` trigger. On a pull request it runs only
  when the change touches the packaging (`cmake/`, `packaging/`, `libs/platform/*/platform.cmake`,
  `tools/release/`, the Qt installer, the presets or the workflow itself), builds, tests, packages and
  writes the SBOM on the three systems, and has no secrets. It is the only place the Release
  configuration is compiled, and the only place `system.install_consumer` runs, before a release.
- On a version tag it also signs, attests and verifies, in the shared `release-publish.yml` of
  `icl-common-infra`. That job runs in the environment `release`, which waits for the maintainer and
  holds the signing key.
- Every action in it is pinned like the others, `permissions: {}` is the workflow's default and each job
  names what it needs, and nothing in it runs on `pull_request_target`.

## The jobs in `ci.yml`

A pull request, drafts included, runs the static jobs. A ready pull request also builds the **fast**
legs and measures coverage. The merge queue, a push to `develop`, a manual run and a pull request with
the `deep` label build **every** leg and run the RTOS simulators. A failure that only a later tier can
find is therefore found once, on the merged tree, and not on every pull request.

| Job | Runs on | What a green run proves |
| --- | --- | --- |
| `Policy` | ubuntu-24.04 | The shared static checks of icl-common-infra's `policy.yml`: branch name, commit and pull request text, text of the committed files, the preprocessor rule (configured in `tools/preprocessor-*`), action pinning, the changelog fragment, REUSE, and the vcpkg baseline being in the fork. It runs every check even after one fails, so one run lists all of them. Runs on drafts. |
| `Plan` | ubuntu-24.04 | `cmake/icl` is the copy of the commit the workflows pin (`tools/ci/sync_cmake.py --check`); what the change touches (`.github/ci/plan.json`: documentation only, no build, build, Qt needed, or the gate itself); and which legs to build. |
| `clang-format` | ubuntu-24.04 | Every `.cpp`/`.hpp` (and sibling extensions) under `libs/`, `apps/` and `tests/` is exactly what the repository's `.clang-format` produces, with one pinned clang-format release (`.github/requirements/lint.txt`). Any difference fails; the log names file and line. |
| `platform-isolation` | ubuntu-24.04 | `tools/checks/check_platform_isolation.ps1`: no preprocessor conditional, no OS header or OS-querying API outside `libs/platform/`, no OS test in CMake outside the selector, no `#define`, every platform source wired. Fast, and deliberately not behind the build legs. |
| `Docs build (strict)` | ubuntu-24.04 | The documentation site builds with `mkdocs build --strict` and its checks pass; every relative link and path in the Markdown files resolves ([Documentation site](#documentation-site)). |
| `Linux (GCC, Release)` | ubuntu-26.04 | Preset `linux-release` with GCC 16 builds warning-free (`-Wall -Wextra -Wpedantic -Werror ...`) with the hardening options, and all of ctest passes. **Fast.** |
| `Linux (Clang, ASan and UBSan)` | ubuntu-26.04 | Preset `linux-asan-ubsan` with Clang 23 at `-O1`, and all of ctest passes. **Fast.** |
| `Linux (GCC, Debug)` | ubuntu-26.04 | Preset `linux-debug`, no sanitizers. |
| `Linux (Clang, TSan)` | ubuntu-26.04 | Preset `linux-tsan` with Clang 23: the platform, engine and library tests are free of data races. The tests that run Qt's event loop and threads are left out ([Building](building.md#the-standard-presets)). |
| `macOS (AppleClang, Release)` | macos-26 (Apple silicon) | Preset `macos-release`: AppleClang from Xcode builds warning-free and all of ctest passes. |
| `Windows (MSVC, Release)` | windows-2025-vs2026 | Preset `windows-msvc-release` builds warning-free (`/W4 /WX`) and all of ctest passes. |
| `Windows (MSVC, ASan)` | windows-2025-vs2026 | Preset `windows-msvc-asan`: the same under AddressSanitizer. |
| `Benchmarks (Linux, GCC)` | ubuntu-26.04 | Preset `linux-perf`: the micro-benchmarks of the check values, the header file, the media set and the TFTP packets run, and none is over its limit in `perf-budget.json` ([Performance](performance.md)); the budget reports and does not fail until it has been filled in from this runner. Full tier. |
| `Fuzz regression (Linux, Clang)` | ubuntu-26.04 | Preset `linux-fuzz` builds the libFuzzer harnesses with Clang 23, and every seed input and every past crash replays without a finding ([Fuzzing](fuzzing.md)). **Fast.** |
| `Coverage (Linux, GCC)` | ubuntu-26.04 | Preset `linux-coverage` passes every test and **meets the project minimum of 90% lines and 80% branches of the portable code** (the ARINC 665 and 615A libraries, the second TFTP implementation, the runner, the media tree, the samples, the payload writer and the platform interfaces; the OS fragments are held by the conformance suite instead). `cmake/CoverageReport.cmake` runs gcovr and fails the job below either figure; the `coverage-report` artifact holds the HTML, `summary.json` and `cobertura.xml`, and the job summary carries the table. A **second pass** measures the applications' window-free logic (`apps/app/core`, `apps/mock_target/core`, `apps/media_gen/core`, Qt's generated files left out) at a lower floor, set a little under what the suite measures ([testing](testing.md#coverage)), and fails when it measures too little; the job therefore installs the Qt kit and configures with `-DADL_BUILD_APP=ON`. The windows' QML cannot be measured and is covered by the UI suite on the build legs. **Fast.** |
| `Workflow analysis` | ubuntu-24.04 | `zizmor` finds nothing in the workflows. |
| `Scan dependency diff` | ubuntu-24.04 | The change adds no moderate-or-worse known vulnerability (GitHub Actions and the vcpkg manifest). |
| `CI Status` | ubuntu-24.04 | Every job above that the plan asked for ran and succeeded. |
| `Zephyr (native_sim)` | ubuntu-26.04 | The platform layer for Zephyr builds with the project's warning set and, with Zephyr 4.4.2's kernel, IPv4/UDP stack (over its loopback interface) and littlefs on simulated flash running as a host process, passes the platform conformance suite and the 615A engine tests (`tests/platform/zephyr`, through `tools/ci/zephyr_native_sim.sh`). Full tier. **Not in `CI Status`.** |
| `FreeRTOS (POSIX simulator)` | ubuntu-26.04 | The platform layer for FreeRTOS builds, and with FreeRTOS-Kernel V11.2.0's POSIX simulator port, lwIP 2.2.1's UDP sockets over its loopback interface and littlefs on a RAM block device, passes the same two suites (`tests/platform/freertos`, through `tools/ci/freertos_posix_sim.sh`). Full tier. **Not in `CI Status`.** |
| `Zephyr (QEMU, SDK toolchain)` | ubuntu-26.04 | The platform layer and the 665/615A libraries build with the Zephyr SDK's own GCC 14 and picolibc, without exceptions, for a Cortex-M3, and a smoke program (`tests/platform/zephyr_smoke`, through `tools/ci/zephyr_qemu_smoke.sh`) runs in QEMU: clock, memory, threads at every priority, a mutex, a queue and UDP over Zephyr's loopback interface. Then the whole test program (Catch2, the platform conformance suite and the 615A engine tests: 227 test cases and 36 288 assertions) is built for the Cortex-A9 board `qemu_cortex_a9` with the same toolchain, with exceptions, and runs in the SDK's QEMU (`tools/ci/zephyr_qemu_suite.sh`). Full tier. **Not in `CI Status`.** |

The three simulator jobs are to join `CI Status` after they have been green for a while. They take
their tools as the other legs do, with the pins in [What CI pins](#what-ci-pins): CMake from the
shared `setup-cmake`, west from `.github/requirements/zephyr.txt`, Zephyr from
`tests/platform/zephyr/west.yml` (a shallow `west update` of Zephyr and the three modules the program
needs), and FreeRTOS, lwIP, littlefs and Catch2 fetched by CMake at the versions and hashes in
`tests/platform/freertos/CMakeLists.txt`. They run on `ubuntu-26.04`; the Zephyr job installs
`device-tree-compiler` and `gperf` with apt.

What a build leg's ctest contains: the Catch2 suites (unit, engine, platform, and `mock.engine`), the
application tests (`app.tests`, `mock.model`, `media_gen.tests`, QtTest), the QML loads of the three
windows (`app.qml`, `mock.qml`, `media_gen.qml`: the window loaded offscreen, with the offscreen platform
and the software renderer, so no GPU or display is needed), the Qt Quick Test suites `ui.tst_main_window`,
`ui.tst_dialogs`, `ui.tst_first_run`, `ui.tst_window_close`, `ui.tst_right_to_left`, `ui.tst_keyboard`,
`ui.tst_navigation`, `ui.tst_accessibility`, `ui.tst_layout` and `ui.tst_tones` (the window and its
dialogs driven by mouse and key events on the real `Main.qml`), `ui.window_state` (the window geometry
the three programs share, on a bare window), `loopback.integration` (the loader's runner over real
sockets on 127.0.0.1 against the mock target's engine), the system smoke tests that start the three
application programs and `system.notices`, the `tools.write_payloads` tests, the `translations.current.*`
tests (lupdate over a copy of each set of catalogues; they fail when the catalogues no longer match the
sources), and the repository checks `platform.fragments` (every platform fragment satisfies the platform
contract, on whichever host this is), `platform.selector`, `platform.isolation` (the same script as the
`platform-isolation` job), `docs.links` (every relative link and `#anchor` of the Markdown files
resolves, pictures included) and `docs.paths` (every repository path that a Markdown file names in a code
span exists); the last three need `pwsh`, which every hosted image has. The tests that load or drive a
window carry the label `ui` (`ctest -L ui`). The application, UI, loopback and system tests carry a
time-out each, and the job has its own (60 minutes), so a hung test cannot hold a runner.
`system.install_consumer` is not on this list: it runs only against an optimised build without a
sanitizer, so only in `release.yml`.

A leg whose build or tests fail uploads the CTest log (`Testing/Temporary/LastTest.log`, with every test's
output rather than only the failures') and a JUnit report as the `ctest-logs-<leg>` artifact, for 14 days;
a green leg uploads nothing. A test that fails on CI and not on a person's machine can then be read after
the fact instead of re-run.

### Toolchain choices

- **Compilers.** The floor is GCC 16, Clang 23 and MSVC toolset 14.51 (spec D7), pinned in
  `.github/toolchain-versions.json`. The shared `setup-gcc` and `setup-llvm` install them on Ubuntu 26.04
  (which may carry a pre-release snapshot of a floor compiler until the distribution packages the
  release; the exact version is printed at the top of every leg), `setup-msvc-env` loads the Visual
  Studio environment and checks the toolset, and the macOS leg uses the AppleClang of the image. The
  `Show the toolchain` step puts the version in the log.
- **CMake** is installed from PyPI by `setup-cmake` at the `cmake` pin, so every runner has the same
  4.x. Ninja is the runner image's.
- **ccache** (`setup-ccache`) caches the compilations of every build leg, keyed by leg. MSVC objects
  are cached because the Windows presets set `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`.
- **Release and sanitizer builds.** The Release presets build in the fast and full tiers, so a
  Release-only warning (`-Wnull-dereference`, for one) shows up on the pull request and not on release day.
- **Qt, on the build legs when the change touches the GUI.** The Qt 6.12 applications are built and
  tested (`-DADL_BUILD_APP=ON`); a change that touches no GUI builds the libraries alone, without Qt.
  Qt is the prebuilt kit, not vcpkg, and the composite action `.github/actions/install-qt` installs it
  with `install_qt.py`, a short script that reads the kit's package list from download.qt.io
  (`win64_msvc2022_64`, `linux_gcc_64`, `clang_64`), downloads each archive, **refuses any archive whose
  SHA-256 is not the one pinned in `.github/requirements/qt-archives.sha256`** and only then unpacks it.
  (aqtinstall, which did this before, cannot read the repository's layout for Windows from Qt 6.11 on, and
  like Qt's own installer checks an archive only against a hash fetched from the same server, so whoever
  could replace an archive could replace its hash.) The one Python package the script needs, `py7zr`, is
  installed (into a `uv venv --python 3.13`, since the newest Python on an image can be newer than the
  wheels) with `uv pip install --require-hashes --no-build` from the hash-locked
  `.github/requirements/qt.txt`, so each wheel is the one that was reviewed and nothing is built from a
  source distribution. The install is retried twice, since download.qt.io is a mirror network, but a hash
  mismatch is not retried; the kit is cached under an exact key (version, architecture, and the hashes of
  the lock files and the script). On Linux the action installs the few system libraries a prebuilt Qt needs,
  and it sets `QT_ROOT_DIR`, which `cmake/FindQt6.cmake` looks in first. The Qt version is the `qt` pin of
  `.github/toolchain-versions.json`, which `ci.yml`, `release.yml` and `static-analysis.yml` read.

  **Moving to another Qt version** is deliberate and reviewed: change the `qt` pin, then, with the
  network and Python 3.10 or newer,

  ```sh
  uv venv /tmp/qt-venv && uv pip install --python /tmp/qt-venv/bin/python --require-hashes -r .github/requirements/qt.txt
  /tmp/qt-venv/bin/python .github/actions/install-qt/install_qt.py lock 6.12.1 windows linux mac --pins .github/requirements/qt-archives.sha256
  ```

  which downloads every archive, checks each against the SHA-256 the repository publishes beside it, and
  rewrites the pins. Read the diff of the pins file like a dependency update, and commit it with the
  version bump. Until then CI fails with "do not cover the archives", on purpose.
- **Local composite actions are referenced as `./.github/actions/...`.** They work on every runner and are
  understood by actionlint. Every job that uses one checks the repository out first.

### The planner and documentation-only changes

`Plan` reads the files the pull request or queue entry changes (against the base, or for a queue entry
the commit it is built on) and `.github/ci/plan.json` sorts them: `docs` (documentation, the site's
tooling), `no_build` (templates, ruleset files, licence files), `build` (the sources, tests, CMake and
packaging), `gui` (Qt needed), and `machinery` (the gate's own files: everything runs). A path that none
of the lists names builds everything, because a false skip is silent and a false build costs minutes.

- A change to documentation alone, or to files no build reads, runs the static jobs and no build:
  `build` is skipped and `CI Status` accepts it because the plan did not ask for it.
- A change that touches no GUI builds the libraries without Qt (`gui=false`).
- A push, a manual run and an empty file list plan everything.

`Docs build (strict)` runs for every change, because a change to code can break a link to it.

### Running some legs by hand

A manual dispatch can name just the legs it needs, which is also how to try a change to threading,
the standard library or platform code on macOS and Windows before it is queued. Macos minutes are
billed at a multiple, so name the leg:

```sh
gh workflow run ci.yml --ref <branch> -f legs=macos
gh workflow run ci.yml --ref <branch> -f legs=clang-format,linux
```

`legs` is comma-separated with no spaces: `clang-format`, `platform-isolation`, `docs`, `fuzz`, `perf`,
`windows`, `linux`, `tsan`, `macos`, `coverage`, `zephyr`, `freertos`, or `all` (the default). An unknown name
fails the run rather than running nothing. A dispatched run reports its aggregator as `dispatch - CI
Status`, deliberately not the name of the required check: a run that skips legs would otherwise
satisfy it on the branch's head commit. The `deep` label on a pull request builds every leg on that
pull request.

A branch other than `develop` gets no CI from a push; open a pull request, or dispatch the workflow on it.

## Required checks

Two checks are required on `develop`: `CI Status` and `Scan dependency diff`, both jobs defined in
`ci.yml` under exactly those names, because a check from a called workflow is reported as
`<caller job> / <called job>`. `CI Status` is the aggregator: it fails when any job the plan asked for
failed, was cancelled, or was skipped, and a job that is skipped because the plan did not ask for it
passes. A required check that never reports would hold a pull request for ever, so neither workflow
filters on paths.

`ci.yml` also triggers on `merge_group`, because the queue evaluates the checks on its temporary
`gh-readonly-queue/...` ref and GitHub only runs workflows that opt in there. A workflow that produces
**no** required check must not carry `merge_group`, or every queue entry burns a run of it for nothing:
that is why `codeql.yml` (which has a `pull_request` trigger but no required check) and `static-analysis.yml` do not have it.

## The changelog fragment

A pull request that changes what a user sees adds `changelog.d/<name>.<category>.md`
([changelog.d/README.md](../changelog.d/README.md)); the `no-changelog` label skips the check. The
Policy job runs the check, and a malformed fragment fails even with the label. Pull requests from
`dependabot/*` branches are not checked. A release assembles the fragments into
`CHANGELOG.md` ([Releasing](releasing.md)).

## The vcpkg checkout and its caches

Dependencies come from vcpkg in manifest mode (the only one CI uses is Catch2, behind the `tests`
feature - Tracy is behind the opt-in `profiling` feature, which no CI job turns on). Every job that runs
vcpkg starts with the shared `setup-vcpkg` action of `icl-common-infra`: it clones the labs' fork, checks
out the commit named by `builtin-baseline`, bootstraps it, exports `VCPKG_ROOT` (which
`cmake/icl/IclVcpkgRoot.cmake` picks up), and restores vcpkg's binary and download caches with
`actions/cache`. The last step of the job, `save-vcpkg-cache` (`if: always()`, so a red leg keeps what
it built), saves them again when something was added.

The keys carry the runner OS and architecture, the baseline commit and the manifest, so a baseline bump
starts from the nearest older entry and builds only the packages that changed. An Actions cache entry is
readable from its own ref, the base branch of a pull request and the default branch, nothing else, so
`develop` is where the cache every pull request starts from is made: a push to `develop` (or `main`) is
never cancelled by the next push (`cancel-in-progress` is off for them), and the first run on a new
branch is cold. With Catch2 as the only port CI builds, a cold run costs minutes, not hours; the cache
earns its keep mostly on macOS.

## What the Zephyr jobs cache, and what they never ask the GitHub API

`Zephyr (native_sim)` and `Zephyr (QEMU, SDK toolchain)` keep two things with `actions/cache`, each under a
key made from the file that pins it, so a change to the pin is a new key and nothing stale is ever restored:

| Cached | Key | Where it comes from when there is no entry |
| --- | --- | --- |
| The west workspace (`.west`, `zephyr`, `modules`) | `zephyr-workspace-<os>-` + hash of `tests/platform/zephyr/west.yml` | `west update --narrow --fetch-opt=--depth=1`, a shallow git clone of Zephyr and three modules |
| The Zephyr SDK's Arm toolchain and host tools (the QEMU job only; about 1.2 GB unpacked) | `zephyr-sdk-<os>-` + hash of `tools/ci/zephyr_sdk.sha256` | `tools/ci/install_zephyr_sdk.sh`, which downloads three release files by URL |

To bust a cache, change what it is keyed on (bump the Zephyr `revision` or the module list in `west.yml`; or
the version and hashes in `zephyr_sdk.sha256`), or delete the entry (Actions, Caches, or
`gh cache delete <key>`). A cache that is missing, evicted or unreadable costs a download and nothing else:
neither job needs it to be there.

The SDK is **not** installed with `west sdk install`. That command asks the GitHub releases API which files
a release has, unauthenticated, and the API allows an address a few dozen such requests an hour: a shared
runner's address had used them up when a merge queue run failed with "fetch_releases API rate limit
exceeded ... 403". `install_zephyr_sdk.sh` knows the file names, takes the pinned SHA-256 of each from
`tools/ci/zephyr_sdk.sha256`, downloads them from `github.com/.../releases/download/<tag>/<file>` (a download,
not an API call, and with retries) and refuses a file whose checksum differs. So no token is passed and none
is needed. Nothing else in the Zephyr or FreeRTOS jobs calls `api.github.com` on its own account: `west
update` and the FreeRTOS job's CMake downloads are `git clone` and archive downloads, `lukka/get-cmake` and
`astral-sh/setup-uv` are the actions the other legs use (they authenticate with the workflow token), and the
Python packages come from PyPI.

## What CI pins

| | Pinned to | Kept current by |
| --- | --- | --- |
| Every GitHub Action, and the composite actions in `.github/actions` | A full commit SHA, version in a trailing comment | Dependabot, weekly, grouped, 7-day cooldown |
| clang-format, gcovr, west | `==` in `.github/requirements/*.txt` | Dependabot (`pip`), weekly |
| clang-tidy (nightly analysis) | `==` in `.github/requirements/static-analysis.txt` | Dependabot (`pip`), weekly |
| Zephyr (and its modules littlefs, cmsis and cmsis_6) | `revision` in `tests/platform/zephyr/west.yml` (v4.4.2) | By hand |
| The Zephyr SDK (the Arm toolchain and QEMU of the QEMU job) | File names and SHA-256 in `tools/ci/zephyr_sdk.sha256` (1.0.1, which is what Zephyr v4.4.2's `SDK_VERSION` names; `install_zephyr_sdk.sh` refuses a mismatch) | By hand, with Zephyr |
| FreeRTOS-Kernel, lwIP, littlefs, Catch2 (the simulator program) | URL and SHA-256 in `tests/platform/freertos/CMakeLists.txt` (V11.2.0, 2.2.1, v2.11.3, v3.15.3) | By hand |
| py7zr and its dependencies (unpack Qt) | `==` with every wheel's SHA-256 in `.github/requirements/qt.txt`, compiled from `qt.in`, installed with `--require-hashes --no-build` | Dependabot (`pip`), weekly |
| MkDocs, Material and their dependencies (build the documentation site) | `==` with every wheel's SHA-256 in `.github/requirements/docs.txt`, compiled from `docs.in` with a one-week cooldown (`--exclude-newer`; a security fix is exempt, as it is for Dependabot: `certifi` is held at the release that fixes GHSA-6q48-6c7w-9gw2), installed with `--require-hashes --no-build` | Dependabot (`pip`), weekly |
| The Qt archives | The SHA-256 of each, in `.github/requirements/qt-archives.sha256` | By hand: `install_qt.py lock` |
| Qt | the `qt` pin in `.github/toolchain-versions.json` (6.12.0), read by `ci.yml`, `release.yml` and `static-analysis.yml` | By hand, together with the Qt archive pins |
| icl-common-infra (the workflows and actions `ci.yml` calls, and `cmake/icl/`) | One commit, with the tag in a trailing comment; `cmake/icl/SOURCE` records the same commit and `Plan` checks it | Dependabot bumps the `uses:` lines; `python3 tools/ci/repin.py --infra ../icl-common-infra --version vX.Y.Z` brings `cmake/icl` along |
| The compilers, CMake and the MSVC toolset | `gcc`, `llvm`, `cmake` and `msvc_toolset` in `.github/toolchain-versions.json` | By hand |
| vcpkg and the ports | `builtin-baseline` in `vcpkg.json` (a commit of the fork) | Dependabot (`vcpkg`, monthly); by hand |
| Runner images | The label (`ubuntu-24.04`, `ubuntu-26.04`, `windows-2025-vs2026`, `macos-26`) | By hand |

Not pinned: Ninja, AppleClang, and anything an image provides (`pwsh`, `jq`, `git`).

## Commit and pull request text

**The rule.** Commit messages, pull request titles and pull request bodies
carry *no attribution of any kind*:

- no `Co-authored-by` trailers (or any mention of one);
- no "generated with / by / using ..." lines;
- no mention of a code-generation tool, an assistant, or the vendor behind one
  - its name, a model name, a noreply address, or a phrase crediting it with
  the work.

**Where it is applied.**

- The Policy job (`check_text_policy.py` of icl-common-infra) reads the pull request title, the pull
  request body and *every* commit message in the range. Git's rule applies to a trailer: a final
  paragraph made only of `Word: value` lines is read as trailers, and only `Signed-off-by` is allowed.
  That covers the pull request body too, because the squash merge uses it as the commit message; a
  `Word: value` line anywhere else is fine. To end a body with such a line, put a sentence after it.
  Re-running the job does not re-read an edited body; closing and reopening the pull request does.
- `tools/hooks/commit-msg` applies the attribution rule to the message being committed, with
  `tools/policy/check_text_policy.ps1`. It takes text as `-Text`, `-Path` or stdin and exits `0`
  (clean), `1` (violations) or `2` (bad invocation). Its deny-list is assembled at run time from short
  fragments, so the file itself contains none of the names it forbids and passes its own check
  (`tools/policy/test_check_text_policy.ps1` runs it over itself).

**Rewording.** The generic "generated with/by/using" pattern is deliberately blunt: a sentence such as
"the header is generated by CMake" trips it. Say "CMake writes the header" instead. The check looks at
lines, so a markdown heading is checked like any other line.

## Local hooks

The hooks live in `tools/hooks/` and are plain POSIX `sh` (they run under Git
for Windows' shell) that call `pwsh` where they need the policy script.
Enable them once per clone:

```sh
git config core.hooksPath tools/hooks
```

| Hook | Does | If `pwsh` is missing |
| --- | --- | --- |
| `commit-msg` | Runs the text policy over the message; refuses the commit on a violation. | Says so and lets the commit through - the pull request check applies the same rule. |
| `pre-push` | Refuses to push any branch whose name is not `main`, `develop` or `feature/`, `bugfix/`, `docs/`, `chore/`, `agent/`, `release/` plus a name. Tags and branch *deletions* are not checked (deleting is how a bad name is removed). | Not needed: it is pure `sh`. |

Both can be bypassed with `--no-verify`; they are a faster place to be told,
not the enforcement. The enforcement is the pull request checks and the rulesets.

Two repository details the hooks depend on: they must keep **LF** line endings
(a CRLF checkout breaks the `#!/bin/sh` line), and they must be **executable**
in the index (git ignores a hook that is not, with a warning).

## When the first run goes red

When a leg goes red, the usual suspects, in order of likelihood:

1. **Warnings as errors** on a compiler version nobody has tried: `-Werror`
   with `-Wconversion`, `-Wsign-conversion`, `-Wold-style-cast` and friends
   will say exactly which line.
2. **A timing-dependent test**: the loopback tests run real sockets and a real
   clock on a shared runner, so one that depends on how fast the other end
   answers can fail under load. That is a defect to fix in the test or the
   engine, not a re-run: read the log first.
3. **The sanitizer runtime**: ASan/UBSan on the hosted image (on Windows, the
   ASan runtime DLL that `adl_deploy_sanitizer_runtime` copies next to the
   tests, found next to the compiler). ThreadSanitizer reports of Qt's or glib's own hand-off
   between threads are not this project's races; the TSan test preset leaves those tests out.
4. **The vcpkg checkout**: the baseline commit missing from the fork (the fork's nightly sync normally
   prevents it), or a store directory left half-made by an interrupted configure.
5. **Coverage**: gcovr refusing a counter, or a figure below the minimum. The
   report step passes `--gcov-ignore-parse-errors negative_hits.warn_once_per_file`
   for the known gcov bug, and the coverage preset updates counters atomically.
   The artifact names the file and line under the minimum; add the test that
   covers it, or remove the dead code.

## What CI does not do

- **Run on a board.** The FreeRTOS and Zephyr ports run on simulators only. No job builds the QNX or
  Green Hills INTEGRITY ports (their code has never been compiled), and VxWorks has no code
  ([platform abstraction](platform-abstraction.md) says where each stands).
- **Build the libraries without exceptions and RTTI with a desktop compiler.** The
  `Zephyr (QEMU, SDK toolchain)` job builds them that way for a Cortex-M3, and its smoke program does not
  run them; no job does it with MSVC, GCC or AppleClang on a desktop.
- **Run the program against a real target or another implementation** ([Testing](testing.md#known-gaps)).

The Release presets are built, tested and packaged by `release.yml` (see above), and clang-tidy and the rest
of the static analysis run nightly ([static-analysis.md](static-analysis.md)).
