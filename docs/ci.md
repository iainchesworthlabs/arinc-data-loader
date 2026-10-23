# CI, text policy and local hooks

This page is what runs on every change, what each job proves, and what keeps
the rules when GitHub cannot. The settings that sit above the workflows
(rulesets, required checks, the merge queue) are in
[Branch protection](branch-protection.md).

## What enforces what

Once the repository's rulesets are applied ([Branch
protection](branch-protection.md)), GitHub blocks a merge or a push that breaks
a rule. The same rules are also kept before a push, locally, by the hooks in
`tools/hooks`.

| Rule | Kept by | Also kept locally by |
| --- | --- | --- |
| Builds, tests, formatting, platform isolation | the `ci.yml` jobs, as required checks | `ctest` (includes the isolation check) |
| Gitflow branch names | `Validate gitflow branch name`, and the `branch-names` ruleset refuses the push | `tools/hooks/pre-push` |
| No attribution in commits and PR text | `Validate commit and PR text`, as a required check | `tools/hooks/commit-msg` |
| A user-visible change has a changelog fragment | `Changelog fragment` (not yet a required check) | `pwsh tools/release/Changelog.ps1 -Mode PullRequest -Base origin/develop` |
| Pull request required, no direct push, no force push | the `main-protection` and `develop-protection` rulesets | - |
| Merge queue in front of `develop` | the `merge-queue-develop` ruleset | - |

Install the hooks once per clone with `git config core.hooksPath tools/hooks`.

## Workflows

| Workflow | Purpose | Runs on a private copy? |
| --- | --- | --- |
| `ci.yml` | `clang-format`, `platform-isolation`, the Windows / Linux / macOS build-and-test legs, a coverage gate, and the Zephyr and FreeRTOS simulator jobs; a change to documentation alone builds nothing ([Documentation-only changes](#documentation-only-changes)) | yes |
| `branch-name-check.yml` | gitflow branch naming; the source/target pairing | yes |
| `commit-policy.yml` | no attribution in the PR title, PR body or any commit message of the PR | yes |
| `changelog.yml` | a PR into `develop` adds a changelog fragment, or carries the `no-changelog` label | yes |
| `docs.yml` | builds the documentation site strictly on every pull request and queue entry, checks the links and the paths in the Markdown files, and publishes the site to GitHub Pages when `main` is pushed ([Documentation site](#documentation-site)) | yes |
| `codeql.yml` | CodeQL static analysis of the C++, nightly | no - skipped |
| `release.yml` | the Release build on the three systems, packages, SBOM, signing, attestations and the draft release; a rehearsal on `release/*` branches and on packaging pull requests ([Releasing](releasing.md)) | yes |
| `static-analysis.yml` | the nightly clang-tidy, MSVC Code Analysis, Clang ASan/UBSan and optional SonarQube runs | yes |
| `dependency-review.yml` | fails a PR that adds a moderate-or-worse vulnerable dependency | no - skipped |
| `scorecard.yml` | OpenSSF Scorecard score and badge, from `main` | no - skipped |
| Dependabot (`.github/dependabot.yml`) | Actions pins, the pinned CI tooling, the vcpkg baseline plus its submodule | yes |

Every workflow declares `permissions: {}` at the top and gives each job only
what it names; every checkout sets `persist-credentials: false`; every
third-party action is pinned to a full commit SHA with its version in a
trailing comment; pull request data reaches scripts through `env:`, never by
interpolating `${{ }}` into a shell.

### Public-only workflows

`codeql.yml`, `dependency-review.yml` and `scorecard.yml` need a public
repository (or a paid plan). Each of their jobs carries
`if: ${{ !github.event.repository.private }}`, so they are **skipped, not red**,
while the repository is private and run once it is public. `Scan dependency
diff` needs the repository's dependency graph switched on, and was added to the
required checks after its first report (see the staging section in
[Branch protection](branch-protection.md)).

## Nightly static analysis

A schedule, and a manual dispatch, work only for a workflow on the default branch
(`main`). A workflow that is only on `develop` is not run on a schedule, and
`gh workflow run` reports that it is not found. A file reaches `main` with a
release (or a hotfix), so these two workflows start running nightly with the first
release that carries them, and not before; the per-commit checks above do not
depend on it.
The same holds for everything GitHub reads only from the default branch: Dependabot
(`.github/dependabot.yml`), `scorecard.yml`, the issue templates and `SECURITY.md`'s
private-reporting link. None of them is active until `main` carries it.

`static-analysis.yml` and `codeql.yml` run on a nightly schedule, not on a
push or a pull request. They are not required checks: a red night is read in
the morning, and a merge is not held for an analyzer. The jobs are clang-tidy
over the libraries (`src/`), MSVC Code Analysis over the libraries, tests and
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

- `docs.yml`, job `Docs build (strict)`: on every pull request into `main` or `develop` and every merge queue
  entry. It has no path filter, because a required check that does not run blocks the pull request. On a
  push to `main` it packages the site and the job `Publish the site` deploys it with `actions/deploy-pages`.
  After the build it runs `tools/checks/check_doc_links.ps1` and `tools/checks/check_doc_paths.ps1`, the scripts
  behind the CTest tests `docs.links` and `docs.paths`, over every Markdown file, so that a change to
  documentation alone, which runs no CTest ([below](#documentation-only-changes)), is still checked for links,
  anchors and paths, and so is `README.md`, which the site does not contain.
- `release.yml`, job `Docs (strict)`: the first job after *Prepare*, ahead of the three builds, so a release
  with a broken page stops in a minute and not after an hour.

**Publishing** uses Pages with the *GitHub Actions* source. A `gh-pages` branch is not an option here: the
`branch-names` ruleset refuses a branch that is not a gitflow name. The deploying job runs in the
`github-pages` environment (deployment branches: `main` only) and waits for the repository variable
`PUBLISH_DOCS` to be `true`, so a repository that has not switched Pages on gets no failed run
([Releasing](releasing.md#one-time-setup-done-once-per-repository)). Because a merge to `main` is a release
(gitflow), the published site is the documentation of the latest release. There is one version of the site;
old releases' pages are in the tag's tree on GitHub.

**Not used.** Zensical, the successor to Material for MkDocs, reads the same `mkdocs.yml`. It was tried and
does not run MkDocs hooks, so every link out of `docs/` fails its strict build. MkDocs is held below 2.0,
which removes hooks and the theme API this configuration uses.

## The release workflow

`release.yml` is described in [Releasing](releasing.md) and checked from the outside by
[Verifying a release](verifying-releases.md). Where it sits among the others:

- Its first job after *Prepare* is `Docs (strict)` (see above); the builds wait for it.
- It is not a required check and carries no `merge_group` trigger. On a pull request it runs only
  when the change touches the packaging (`cmake/`, `packaging/`, `libs/platform/*/platform.cmake`,
  `tools/release/`, the Qt installer, the presets or the workflow itself), builds, tests, packages and
  writes the SBOM on the three systems, and has no secrets. It is the only place the Release
  configuration is compiled, and the only place `system.install_consumer` runs, before a release.
- On a push to `release/*` or a version tag it also signs, attests and verifies. The job that signs runs
  in the environment `release-signing`, which GitHub gives to those refs only; the job that drafts the
  release runs in `release`, which waits for the maintainer.
- Every action in it is pinned like the others, `permissions: {}` is the workflow's default and each job
  names what it needs (`id-token` and `attestations` for *Attest*, `contents: write` for *Draft the
  release*), and nothing in it runs on `pull_request_target`.

### The release signing key

The public key is [`release-signing-key.asc`](release-signing-key.asc). Its fingerprint, also in
[SECURITY.md](../SECURITY.md) and [Verifying a release](verifying-releases.md), is

```text
C6B4 AD93 067E B501 F450  5BCF 3FF3 6825 5329 8A53    (Ed25519, expires 2028-10-18)
```

The private key and its passphrase are the secrets `RELEASE_GPG_PRIVATE_KEY` and
`RELEASE_GPG_PASSPHRASE` of the environment `release-signing`. `tools/release/sign_release.sh`
refuses to sign unless the secret holds the key with this fingerprint, and verifies every signature
against the public key in the repository before the step passes.

## The jobs in `ci.yml`

The job *display names* are the required-check contexts in
`.github/rulesets/*.json`. They are stable on purpose; rename one only together
with the rulesets.

| Job (display name) | Runs on | What a green run proves |
| --- | --- | --- |
| `clang-format` | ubuntu-24.04 | Every `.cpp`/`.hpp` (and sibling extensions) under `libs/`, `apps/` and `tests/` is exactly what the repository's `.clang-format` produces, with one pinned clang-format release (`.github/requirements/lint.txt`). Any difference fails; the log names file and line. |
| `platform-isolation` | ubuntu-24.04 | `tools/checks/check_platform_isolation.ps1`: no preprocessor conditional, no OS header or OS-querying API outside `libs/platform/`, no OS test in CMake outside the selector, no `#define`, every platform source wired. Fast, and deliberately not behind the build legs. |
| `Plan build legs` | ubuntu-24.04 | The build matrix was computed, and so was whether the change is more than documentation ([Documentation-only changes](#documentation-only-changes)). It is a required check only so that a failure here cannot turn the three build legs into "skipped" (which counts as a pass). |
| `Windows (MSVC)` | windows-2025-vs2026 | Preset `windows-msvc-debug` configures, builds warning-free (`/W4 /WX`) with ASan, and all of ctest passes. |
| `Linux (GCC)` | ubuntu-26.04 | Preset `linux-gcc-debug`: the image's default `g++` builds warning-free (`-Wall -Wextra -Wpedantic -Werror ...`) with ASan and UBSan, and all of ctest passes. |
| `macOS (AppleClang)` | macos-26 (Apple silicon) | Preset `macos-clang-debug`: AppleClang from Xcode builds warning-free with ASan and UBSan, and all of ctest passes. |
| `Coverage (Linux, GCC)` | ubuntu-26.04 | Preset `linux-gcc-coverage` passes every test and **meets the project minimum of 90% lines and 80% branches of the portable code** (the ARINC 665 and 615A libraries, the second TFTP implementation, the runner, the media tree, the samples, the payload writer and the platform interfaces; the OS fragments are held by the conformance suite instead). `cmake/CoverageReport.cmake` runs gcovr and fails the job below either figure; the `coverage-report` artifact holds the HTML, `summary.json` and `cobertura.xml`, and the job summary carries the table. A **second pass** measures the applications' window-free logic (`apps/app/core`, `apps/mock_target/core`, `apps/media_gen/core`, Qt's generated files left out) at a lower floor, set a little under what the suite measures ([testing](testing.md#coverage)), and fails when it measures too little; the job therefore installs the Qt kit and configures with `-DADL_BUILD_APP=ON`. The windows' QML cannot be measured and is covered by the UI suite on the build legs. |

| `Zephyr (native_sim)` | ubuntu-26.04 | The platform layer for Zephyr builds with the project's warning set and, with Zephyr 4.4.2's kernel, IPv4/UDP stack (over its loopback interface) and littlefs on simulated flash running as a host process, passes the platform conformance suite and the 615A engine tests (`tests/platform/zephyr`, through `tools/ci/zephyr_native_sim.sh`). **Not a required check.** |
| `FreeRTOS (POSIX simulator)` | ubuntu-26.04 | The platform layer for FreeRTOS builds, and with FreeRTOS-Kernel V11.2.0's POSIX simulator port, lwIP 2.2.1's UDP sockets over its loopback interface and littlefs on a RAM block device, passes the same two suites (`tests/platform/freertos`, through `tools/ci/freertos_posix_sim.sh`). **Not a required check.** |
| `Zephyr (QEMU, SDK toolchain)` | ubuntu-26.04 | The platform layer and the 665/615A libraries build with the Zephyr SDK's own GCC 14 and picolibc, without exceptions, for a Cortex-M3, and a smoke program (`tests/platform/zephyr_smoke`, through `tools/ci/zephyr_qemu_smoke.sh`) runs in QEMU: clock, memory, threads at every priority, a mutex, a queue and UDP over Zephyr's loopback interface. Then the whole test program (Catch2, the platform conformance suite and the 615A engine tests: 227 test cases and 36 288 assertions) is built for the Cortex-A9 board `qemu_cortex_a9` with the same toolchain, with exceptions, and runs in the SDK's QEMU (`tools/ci/zephyr_qemu_suite.sh`). **Not a required check.** |

The three simulator jobs are not in the rulesets: they are to join them after they have been green for a
while, the way `Scan dependency diff` did ([Branch protection](branch-protection.md#required-checks)). They
take their tools as the other legs do, with the pins in the next section: CMake and Ninja from
`lukka/get-cmake`, west from `.github/requirements/zephyr.txt`, Zephyr from `tests/platform/zephyr/west.yml`
(a shallow `west update` of Zephyr and the three modules the program needs), and FreeRTOS, lwIP,
littlefs and Catch2 fetched by CMake at the versions and hashes in `tests/platform/freertos/CMakeLists.txt`.
They run on `ubuntu-26.04`, the image the Linux leg verifies GCC 15 on; the Zephyr job installs
`device-tree-compiler` and `gperf` with apt.

What a build-and-test leg's ctest contains: the Catch2 suites (unit, engine, platform, and
`mock.engine`), the application tests (`app.tests`, `mock.model`, `media_gen.tests`, QtTest), the QML
loads of the three windows (`app.qml`, `mock.qml`, `media_gen.qml`: the window loaded offscreen, with
the offscreen platform and the software renderer, so no GPU or display is needed), the Qt Quick Test
suites `ui.tst_main_window`, `ui.tst_dialogs`, `ui.tst_first_run`, `ui.tst_window_close`,
`ui.tst_right_to_left`, `ui.tst_keyboard`, `ui.tst_navigation`, `ui.tst_accessibility`, `ui.tst_layout` and
`ui.tst_tones` (the window and its dialogs driven by mouse and key events on the real `Main.qml`),
`ui.window_state` (the window geometry the three programs share, on a bare window), `loopback.integration`
(the loader's runner over real sockets on 127.0.0.1 against the mock target's engine), the system smoke
tests that start the three application programs and `system.notices`, the `tools.write_payloads` tests,
the `translations.current.*` tests (lupdate over a copy of each set of catalogues; they fail when the
catalogues no longer match the sources), and the repository checks `platform.fragments` (every platform
fragment satisfies the platform contract, on whichever host this is), `platform.selector`,
`platform.vcpkg_root` (which vcpkg checkout a configure uses, tried against a fake repository, with no
network), `platform.isolation` (the same script as the `platform-isolation` job), `docs.links` (every
relative link and `#anchor` of the Markdown files resolves, pictures included), `docs.paths` (every
repository path that a Markdown file names in a code span exists) and `ci.changed_code` (the list of files
that make a change documentation-only, [below](#documentation-only-changes)); the last four need `pwsh`,
which every hosted image has. The tests that load or drive a window carry the label `ui`
(`ctest -L ui`). The application, UI, loopback and system tests carry a time-out each, and the job has its
own (45 minutes), so a hung test cannot hold a runner. `system.install_consumer` is not on this list: it
runs only against an optimised build, so only in `release.yml`.

A leg whose build or tests fail uploads the CTest log (`Testing/Temporary/LastTest.log`, with every test's
output rather than only the failures') and a JUnit report as the `ctest-logs-<preset>` artifact, for 14 days;
a green leg uploads nothing. A test that fails on CI and not on a person's machine can then be read after
the fact instead of re-run.

### Toolchain choices

- **No compiler install.** The code is C++23 and uses `std::expected`, so each
  leg uses the compiler its image ships. The `Show the toolchain` step puts
  the exact version in the log.
- **Debug presets.** CI builds the Debug presets because they carry the
  sanitizers. The Release presets are built by `release.yml` (on every
  packaging pull request and every release branch), so a Release-only warning
  (`-Wnull-dereference`, for one) shows up there rather than on release day.
- **Qt, on the three build legs.** The Qt 6.12 application is built and tested (`-DADL_BUILD_APP=ON`).
  Qt is the prebuilt kit, not vcpkg, and the composite action `.github/actions/install-qt` installs it
  with `install_qt.py`, a short script that reads the kit's package list from download.qt.io
  (`win64_msvc2022_64`, `linux_gcc_64`, `clang_64`), downloads each archive, **refuses any archive whose
  SHA-256 is not the one pinned in `.github/requirements/qt-archives.sha256`** and only then unpacks it.
  (aqtinstall, which did this before, cannot read the repository's layout for Windows from Qt 6.11 on, and
  like Qt's own installer checks an archive only against a hash fetched from the same server, so whoever
  could replace an archive could replace its hash.) The one Python package the script needs, `py7zr`, is
  installed (into a `uv venv --python 3.13`, since the newest Python on an image can be newer than the wheels) with `uv pip install --require-hashes --no-build` from the hash-locked
  `.github/requirements/qt.txt`, so each wheel is the one that was reviewed and nothing is built from a
  source distribution. The install is retried twice, since download.qt.io is a mirror network, but a hash
  mismatch is not retried; the kit is cached under an exact key (version, architecture, and the hashes of
  the lock files and the script). On Linux the action installs the few system libraries a prebuilt Qt needs,
  and it sets `QT_ROOT_DIR`, which `cmake/FindQt6.cmake` looks in first. The Qt version (6.12.0) is the action's
  `version` input in `ci.yml` (the build legs and the coverage job), `release.yml` and `static-analysis.yml`
  (the MSVC Code Analysis job); the host and architecture names are given beside it. The coverage job installs
  it too (Linux, `linux_gcc_64`) and builds the applications, because its second pass measures their
  window-free logic; the UI suite runs on the three build legs.

  **Moving to another Qt version** is deliberate and reviewed: change the `version` input in all four places
  (two in `ci.yml`, one each in `release.yml` and `static-analysis.yml`), then, with the
  network and Python 3.10 or newer,

  ```sh
  uv venv /tmp/qt-venv && uv pip install --python /tmp/qt-venv/bin/python --require-hashes -r .github/requirements/qt.txt
  /tmp/qt-venv/bin/python .github/actions/install-qt/install_qt.py lock 6.12.1 windows linux mac --pins .github/requirements/qt-archives.sha256
  ```

  which downloads every archive, checks each against the SHA-256 the repository publishes beside it, and
  rewrites the pins. Read the diff of the pins file like a dependency update, and commit it with the version
  bump. Until then CI fails with "do not cover the archives", on purpose.
- **CMake 4.4.2 and Ninja 1.13.2** from `lukka/get-cmake`; the same pins the
  sibling projects use.
- **Local composite actions are referenced as `./.github/actions/...`.** The
  sibling projects use the `$/.github/actions/...` form (resolved against the
  workflow's own commit, runner 2.336.0 or newer); `./` works on every runner
  and is understood by actionlint. Every job that uses one checks the
  repository out first.

### Documentation-only changes

A pull request or merge queue entry whose changed files are all documentation builds and tests nothing.
`Plan build legs` runs `.github/actions/changed-code`, which runs `tools/ci/changed_code.ps1`. That lists the
files that differ between the merge commit and its first parent (for a queue entry, between the entry and the
commit it is built on, `merge_group.base_sha`), and outputs `code=false` when each of them is

- under `docs/`, except `docs/release-signing-key.asc` (the release workflow and `tools/release/*.sh` read it);
- `mkdocs.yml`, or under `changelog.d/`;
- a Markdown file at the root of the repository (`README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, ...).

Everything else is code. That keeps out of the list whatever CTest or a build reads: `LICENSE` (the notices
test), the Markdown files below the root (they sit among the sources), the workflows, the check scripts of
`tools/` and the pinned requirements. A rename is listed under both names, so moving
a source file into `docs/` is a change to code. An empty diff, a failed fetch and every event that is not a
pull request or a queue entry (a push, a manual run, a schedule) are `code=true`: a push to `develop` or
`main` always builds everything, since it makes the vcpkg cache that pull requests start from. The test
`ci.changed_code` holds the list, so a file cannot be added to it unnoticed. The `Plan build legs` log says what
it found, for example "3 changed file(s), all documentation".

What `code=false` does, and why it is built this way:

- **The required checks still report.** A job skipped by `if:` reports under the raw text of its `name:`
  expression (`${{ matrix.name }}` for a leg), not under the name it would have had, so a required check
  skipped that way never reports and the pull request waits for it. A workflow-level `paths:` filter has the
  same fault. So `Windows (MSVC)`, `Linux (GCC)`, `macOS (AppleClang)` and `Coverage (Linux, GCC)` are not
  skipped: they run, print "Nothing to build or test (documentation only)", skip every other step and finish
  in seconds as successes. The three legs are given an Ubuntu runner for it, not a macOS or a Windows one.
  `clang-format`, `platform-isolation` and `Plan build legs` run as usual.
- **The rest skips as a whole.** `Zephyr (native_sim)`, `Zephyr (QEMU, SDK toolchain)` and `FreeRTOS (POSIX
  simulator)` are not required checks, so `if:` skips them. A job made a required check later needs the
  treatment of the legs above first ([Branch protection](branch-protection.md#the-merge_group-rule)).
- **The checks of documentation still run.** `Docs build (strict)` builds the site and runs the link and path
  checks over every Markdown file ([Documentation site](#documentation-site)); they are the same scripts as the
  CTest tests `docs.links` and `docs.paths`, which a documentation-only change does not reach. The workflow has
  no path filter, so it runs for every change.
- **The merge queue.** Every entry runs `ci.yml` on its own temporary ref, as before, and is judged by its own
  changes. When an entry ahead of it in the queue changes code and it changes documentation, it builds nothing
  and the entry ahead builds the code; if that one fails, the queue builds the later entry again on its new base.

`changelog.yml` and the policy checks are not affected: a documentation-only pull request still carries a
changelog fragment or the `no-changelog` label.

### Running some legs by hand

A push, and every pull request or queue entry that changes more than documentation, runs every leg. A manual dispatch can
name just the ones it needs - useful in a fork or private copy, where hosted
minutes are limited and macOS minutes are billed at a multiple:

```sh
gh workflow run ci.yml --ref <branch> -f legs=macos
gh workflow run ci.yml --ref <branch> -f legs=clang-format,linux
```

`legs` is comma-separated with no spaces: `clang-format`, `platform-isolation`,
`windows`, `linux`, `macos`, `coverage`, `zephyr`, `freertos`, or `all` (the default). An unknown
name fails the run rather than running nothing. Jobs of a dispatched run are
named `dispatch - <leg>`, deliberately not the names of the required checks: a
run that skips legs would otherwise report "skipped" (which counts as a pass)
under a required check's name on the branch's head commit.

A branch other than `main` and `develop` gets no CI from a push; open a pull
request, or dispatch the workflow on it.

## Required checks and their staging

The same list is required on `develop` and `main` once the rulesets apply. The
contexts equal the job display names above and in the two policy workflows.

| Check (context) | Workflow | In the rulesets from day one? |
| --- | --- | --- |
| `Validate gitflow branch name` | `branch-name-check.yml` | yes |
| `Validate commit and PR text` | `commit-policy.yml` | yes |
| `clang-format` | `ci.yml` | yes |
| `platform-isolation` | `ci.yml` | yes |
| `Plan build legs` | `ci.yml` | yes |
| `Windows (MSVC)` | `ci.yml` | yes |
| `Linux (GCC)` | `ci.yml` | yes |
| `macOS (AppleClang)` | `ci.yml` | yes |
| `Coverage (Linux, GCC)` | `ci.yml` | yes |
| `Scan dependency diff` | `dependency-review.yml` | yes - added after it reported on a pull request on the public repository |
| `Docs build (strict)` | `docs.yml` | yes - added with the documentation site |
| `Changelog fragment` | `changelog.yml` | no - not yet required; see below |
| `Analyze (C++)` | `codeql.yml` | no - informational |
| `Zephyr (native_sim)` | `ci.yml` | no - to be required once it has been green for a while |
| `Zephyr (QEMU, SDK toolchain)` | `ci.yml` | no - to be required once it has been green for a while |
| `FreeRTOS (POSIX simulator)` | `ci.yml` | no - to be required once it has been green for a while |

Every workflow that produces a required check also triggers on `merge_group`,
because the merge queue evaluates the required checks on its temporary
`gh-readonly-queue/...` ref and GitHub only runs workflows that opt in there.
`branch-name-check.yml` and `commit-policy.yml` skip their job on a queue entry
(there is no head branch name, title or body of its own; a skipped job counts
as a pass for a required check). A workflow that produces **no** required
check must not carry `merge_group`, or every queue entry burns a run of it for
nothing: that is why `codeql.yml` and `static-analysis.yml` do not have it.

## The changelog fragment check

`changelog.yml` (job `Changelog fragment`) runs on a pull request into
`develop`. It first runs `tools/release/test_changelog.ps1`, which proves the
tool still fails what it must, and then `tools/release/Changelog.ps1 -Mode
PullRequest`: the pull request must add a well-formed file
`changelog.d/<name>.<category>.md`, or carry the `no-changelog` label, or come
from a `release/*`, `hotfix/*` or `dependabot/*` branch. A malformed fragment
fails even with the label. The label and branch name reach the script through
the environment. Pull requests into `main` are release pull requests, where the
fragments are consumed ([Releasing](releasing.md)), and are not checked.

It is a visible check but not a required one yet: requiring it means adding
`Changelog fragment` to the two ruleset files and to `merge_group` in the
workflow in one commit (`docs/branch-protection.md`), which is the maintainer's
call.

## The vcpkg binary cache

Dependencies come from vcpkg (the `deps/vcpkg` submodule, manifest mode; the
only one CI uses is Catch2, behind the `tests` feature - Tracy is behind the
opt-in `profiling` feature, which no CI job turns on). Every job that runs
vcpkg uses the same two composite actions,
`.github/actions/vcpkg-cache/restore` and `/save`, which wrap vcpkg's `files`
binary-cache provider around `actions/cache`:

| Step | What it does |
| --- | --- |
| restore | Points `VCPKG_BINARY_SOURCES` at `$RUNNER_TEMP/vcpkg-cache` and fills it from the newest entry whose key starts `vcpkg-binary-cache-<triplet>-`. |
| *(build)* | vcpkg restores every archive whose ABI hash matches and builds only the rest. |
| save | Runs with `if: always()`, so a red leg keeps what it built. Stores the directory under `vcpkg-binary-cache-<triplet>-<hash of its file listing>`, only if an archive was added since the restore and the key is not already readable from this ref. |

The key carries the **triplet and nothing else** (`x64-windows`, `x64-linux`,
`arm64-osx`): vcpkg's ABI hash already covers what changes the binaries, and
every archive holds both the debug and the release variant. `Linux (GCC)` and
`Coverage (Linux, GCC)` share `x64-linux`.

An Actions cache entry is readable from its own ref, the base branch of a pull
request and the default branch, nothing else. So `develop` is where the cache
every pull request starts from is made: a push to `develop` (or `main`) is
never cancelled by the next push (`cancel-in-progress` is off for them), and
the first run on a new branch is cold. With Catch2 as the only port CI builds, a cold
run costs minutes, not hours; the cache earns its keep mostly on macOS.

`actions/checkout` runs with `fetch-depth: 0` and `submodules: recursive` on
the vcpkg-using jobs: with a `builtin-baseline`, vcpkg reads historical port
versions out of the submodule's git history, and the default depth-1
submodule clone does not have them. The same holds on a developer's machine:
[Building](building.md#clone) clones the submodule in full, not with `--depth 1`.

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
| Qt | `version` input of the `install-qt` step in `ci.yml`, `release.yml` and `static-analysis.yml` (6.12.0) | By hand, together with the Qt archive pins |
| CMake and Ninja (`lukka/get-cmake`) | `cmakeVersion` / `ninjaVersion`, exact, and one the pinned action's catalog lists | By hand, with the action |
| vcpkg and the ports | The submodule commit and `builtin-baseline` | Dependabot (`vcpkg` + `gitsubmodule`, one monthly PR); by hand |
| Runner images | The label (`ubuntu-24.04`, `ubuntu-26.04`, `windows-2025-vs2026`, `macos-26`) | By hand |

Not pinned: the compilers (each is what its image ships), and anything an
image provides (`pwsh`, `jq`, `git`).

## Commit and PR text policy

**The rule.** Commit messages, pull request titles and pull request bodies
carry *no attribution of any kind*:

- no `Co-authored-by` trailers (or any mention of one);
- no "generated with / by / using ..." lines;
- no mention of a code-generation tool, an assistant, or the vendor behind one
  - its name, a model name, a noreply address, or a phrase crediting it with
  the work.

**Where it is applied.**

- `commit-policy.yml` (job `Validate commit and PR text`) runs
  `tools/policy/check_text_policy.ps1` over the PR title, the PR body and
  *every* commit message in the PR range (`base..head`). It first runs
  `tools/policy/test_check_text_policy.ps1`, which proves the script still
  rejects what it must, so a broken pattern cannot quietly pass everything.
- `tools/hooks/commit-msg` applies it to the message being committed.

**The script.** `check_text_policy.ps1` takes text as `-Text`, `-Path` or
stdin and exits `0` (clean), `1` (violations) or `2` (bad invocation). Each
finding is one line, `<label>:<line>: <what> [<rule>]: <the line>`. In
`-GitMessage` mode it ignores `#` comment lines and everything below the
scissors line, as git's own cleanup does. The deny-list is assembled at run
time from short fragments, so the file itself contains none of the names it
forbids and passes its own check (the self-test runs it over itself).

**Rewording.** The generic "generated with/by/using" pattern is deliberately
blunt: a sentence such as "the header is generated by CMake" trips it. Say
"CMake writes the header" instead. The check looks at lines, so a markdown
heading is checked like any other line.

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
| `pre-push` | Refuses to push any branch whose name is not `main`, `develop` or `feature/`, `bugfix/`, `release/`, `hotfix/`, `support/` plus a name. Tags and branch *deletions* are not checked (deleting is how a bad name is removed). | Not needed: it is pure `sh`. |

Both can be bypassed with `--no-verify`; they are a faster place to be told,
not the enforcement. The enforcement is the pull request checks and, once
public, the rulesets.

Two repository details the hooks depend on: they must keep **LF** line endings
(a CRLF checkout breaks the `#!/bin/sh` line), and they must be **executable**
in the index (git ignores a hook that is not, with a warning).

## When the first run goes red

The three legs (Windows MSVC, Linux GCC, macOS AppleClang) have all run on
`develop`. When a leg goes red, the usual suspects, in order of likelihood:

1. **Warnings as errors** on a compiler version nobody has tried: `-Werror`
   with `-Wconversion`, `-Wsign-conversion`, `-Wold-style-cast` and friends
   will say exactly which line.
2. **A timing-dependent test**: the loopback tests run real sockets and a real
   clock on a shared runner, so one that depends on how fast the other end
   answers can fail under load. That is a defect to fix in the test or the
   engine, not a re-run: read the log first.
3. **The sanitizer runtime**: ASan/UBSan on the hosted image (on Windows, the
   ASan runtime DLL that `adl_deploy_sanitizer_runtime` copies next to the
   tests, found next to the compiler).
4. **The vcpkg submodule**: a clone without enough history for the baseline
   (`fetch-depth: 0` is set for that reason; on a developer's machine,
   `git -C deps/vcpkg fetch --unshallow` repairs a submodule made with `--depth 1`).
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
