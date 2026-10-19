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
| Pull request required, no direct push, no force push | the `main-protection` and `develop-protection` rulesets | - |
| Merge queue in front of `develop` | the `merge-queue-develop` ruleset | - |

Install the hooks once per clone with `git config core.hooksPath tools/hooks`.

## Workflows

| Workflow | Purpose | Runs on a private copy? |
| --- | --- | --- |
| `ci.yml` | `clang-format`, `platform-isolation`, the Windows / Linux / macOS build-and-test legs, a coverage report, and the Zephyr and FreeRTOS simulator jobs | yes |
| `branch-name-check.yml` | gitflow branch naming; the source/target pairing | yes |
| `commit-policy.yml` | no attribution in the PR title, PR body or any commit message of the PR | yes |
| `codeql.yml` | CodeQL static analysis of the C++ | no - skipped |
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

## The jobs in `ci.yml`

The job *display names* are the required-check contexts in
`.github/rulesets/*.json`. They are stable on purpose; rename one only together
with the rulesets.

| Job (display name) | Runs on | What a green run proves |
| --- | --- | --- |
| `clang-format` | ubuntu-24.04 | Every `.cpp`/`.hpp` (and sibling extensions) under `src/` and `tests/` is exactly what the repository's `.clang-format` produces, with one pinned clang-format release (`.github/requirements/lint.txt`). Any difference fails; the log names file and line. |
| `platform-isolation` | ubuntu-24.04 | `tools/checks/check_platform_isolation.ps1`: no preprocessor conditional, no OS header or OS-querying API outside `src/platforms/`, no OS test in CMake outside the selector, no `#define`, every platform source wired. Fast, and deliberately not behind the build legs. |
| `Plan build legs` | ubuntu-24.04 | The build matrix was computed. It is a required check only so that a failure here cannot turn the three build legs into "skipped" (which counts as a pass). |
| `Windows (MSVC)` | windows-2025-vs2026 | Preset `windows-msvc-debug` configures, builds warning-free (`/W4 /WX`) with ASan, and all of ctest passes. |
| `Linux (GCC)` | ubuntu-26.04 | Preset `linux-gcc-debug`: the image's default `g++` builds warning-free (`-Wall -Wextra -Wpedantic -Werror ...`) with ASan and UBSan, and all of ctest passes. |
| `macOS (AppleClang)` | macos-26 (Apple silicon) | Preset `macos-clang-debug`: AppleClang from Xcode builds warning-free with ASan and UBSan, and all of ctest passes. |
| `Coverage (Linux, GCC)` | ubuntu-26.04 | Preset `linux-gcc-coverage` builds and passes its tests, and a gcovr report of `src/` is produced as the `coverage-report` artifact (HTML, plus `summary.json` and a table in the job summary). **Report only: there is no threshold.** |

| `Zephyr (native_sim)` | ubuntu-26.04 | The platform layer for Zephyr builds with the project's warning set and, with Zephyr 4.4.2's kernel, IPv4/UDP stack (over its loopback interface) and littlefs on simulated flash running as a host process, passes the platform conformance suite and the 615A engine tests (`tests/platform/zephyr`, through `tools/ci/zephyr_native_sim.sh`). **Not a required check.** |
| `FreeRTOS (POSIX simulator)` | ubuntu-26.04 | The platform layer for FreeRTOS builds, and with FreeRTOS-Kernel V11.2.0's POSIX simulator port, lwIP 2.2.1's UDP sockets over its loopback interface and littlefs on a RAM block device, passes the same two suites (`tests/platform/freertos`, through `tools/ci/freertos_posix_sim.sh`). **Not a required check.** |

The two simulator jobs are new and are not in the rulesets: they join them after they have been green for a
while, the way `Scan dependency diff` did ([Branch protection](branch-protection.md#required-checks)). They
take their tools as the other legs do, with the pins in the next section: CMake and Ninja from
`lukka/get-cmake`, west from `.github/requirements/zephyr.txt`, Zephyr from `tests/platform/zephyr/west.yml`
(a shallow `west update` of Zephyr and the two modules the program needs), and FreeRTOS, lwIP,
littlefs and Catch2 fetched by CMake at the versions and hashes in `tests/platform/freertos/CMakeLists.txt`.
They run on `ubuntu-26.04`, the image the Linux leg verifies GCC 15 on; the Zephyr job installs
`device-tree-compiler` and `gperf` with apt.

What a build-and-test leg's ctest contains: the Catch2 suites, the application's `app.tests` (QtTest) and
`app.qml` (the window loaded offscreen, with the offscreen platform and the software renderer, so no GPU
or display is needed; both have a two minute time-out so that a hung test cannot hold the job), and the two
repository checks `platform.fragments` (every platform fragment satisfies the
platform contract, on whichever host this is) and `platform.isolation` (the
same script as the `platform-isolation` job; it needs `pwsh`, which every
hosted image has).

### Toolchain choices

- **No compiler install.** The code needs `<expected>` and `<format>`, which
  libstdc++ has from GCC 13 and Apple's libc++ has in current Xcode, so each
  leg uses the compiler its image ships. The `Show the toolchain` step puts
  the exact version in the log.
- **Debug presets.** CI builds the Debug presets because they carry the
  sanitizers. The Release presets are not built in CI yet; a Release-only
  warning (`-Wnull-dereference`, for one) would first show up locally.
- **Qt, on the three build legs.** The Qt 6.12 application is built and tested (`-DADL_BUILD_APP=ON`).
  Qt is the prebuilt kit, not vcpkg, and the composite action `.github/actions/install-qt` installs it:
  with the aqtinstall pinned in `.github/requirements/qt.txt` on Linux (`linux_gcc_64`) and macOS
  (`clang_64`), and on Windows with `install_qt_msvc.py`, a short script that does what aqtinstall does for
  one package (the `win64_msvc2022_64` kit, each archive checked against its published SHA-1). aqtinstall
  3.3.0 does not know the layout Qt's repository has for Windows from Qt 6.11 on (one directory per
  architecture) and fails with "Failed to locate XML data"; drop the script when a release of it does. The
  install is retried twice, since download.qt.io is a mirror network, and the kit is cached under an exact
  key (version, architecture and the installer's pin). On Linux the action installs the few system
  libraries a prebuilt Qt needs, and it sets `QT_ROOT_DIR`, which `cmake/FindQt6.cmake` looks in first. The
  Qt version is the action's `version` input in `ci.yml` (the same 6.12.0 as the labs' shared toolchain);
  the matrix has the host and architecture names. The coverage job does not build the application, so it
  needs no Qt, and its figures are of the libraries only.
- **CMake 4.4.2 and Ninja 1.13.2** from `lukka/get-cmake`; the same pins the
  sibling projects use.
- **Local composite actions are referenced as `./.github/actions/...`.** The
  sibling projects use the `$/.github/actions/...` form (resolved against the
  workflow's own commit, runner 2.336.0 or newer); `./` works on every runner
  and is understood by actionlint. Every job that uses one checks the
  repository out first.

### Running some legs by hand

A push, pull request or queue run always runs every leg. A manual dispatch can
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
| `Analyze (C++)` | `codeql.yml` | no - informational |
| `Zephyr (native_sim)` | `ci.yml` | no - new, not required until it has been green for a while |
| `FreeRTOS (POSIX simulator)` | `ci.yml` | no - new, not required until it has been green for a while |

Every workflow that produces a required check also triggers on `merge_group`,
because the merge queue evaluates the required checks on its temporary
`gh-readonly-queue/...` ref and GitHub only runs workflows that opt in there.
`branch-name-check.yml` and `commit-policy.yml` skip their job on a queue entry
(there is no head branch name, title or body of its own; a skipped job counts
as a pass for a required check). A workflow that produces **no** required
check must not carry `merge_group`, or every queue entry burns a run of it for
nothing: that is why `codeql.yml` does not have it.

## The vcpkg binary cache

Dependencies come from vcpkg (the `deps/vcpkg` submodule, manifest mode; the
only one today is Catch2, behind the `tests` feature). Every job that runs
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
the first run on a new branch is cold. With Catch2 as the only port a cold
run costs minutes, not hours; the cache earns its keep mostly on macOS.

`actions/checkout` runs with `fetch-depth: 0` and `submodules: recursive` on
the vcpkg-using jobs: with a `builtin-baseline`, vcpkg reads historical port
versions out of the submodule's git history, and the default depth-1
submodule clone does not have them.

## What CI pins

| | Pinned to | Kept current by |
| --- | --- | --- |
| Every GitHub Action, and the composite actions in `.github/actions` | A full commit SHA, version in a trailing comment | Dependabot, weekly, grouped, 7-day cooldown |
| clang-format, gcovr, west | `==` in `.github/requirements/*.txt` | Dependabot (`pip`), weekly |
| Zephyr (and its modules littlefs and cmsis_6) | `revision` in `tests/platform/zephyr/west.yml` (v4.4.2) | By hand |
| FreeRTOS-Kernel, lwIP, littlefs, Catch2 (the simulator program) | URL and SHA-256 in `tests/platform/freertos/CMakeLists.txt` (V11.2.0, 2.2.1, v2.11.3, v3.15.3) | By hand |
| aqtinstall (installs Qt) | `==` in `.github/requirements/qt.txt` | Dependabot (`pip`), weekly |
| Qt | `version` input of the `install-qt` step in `ci.yml` (6.12.0) | By hand, with the labs' shared toolchain |
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

The build has been run locally on Windows (MSVC 19.51) only; the Linux and macOS
legs have not run yet, and the first run of each is their first compile of
`src/platforms/linux` and `src/platforms/macos`. When a leg goes red, the usual
suspects, in order of likelihood:

1. **Warnings as errors** on a compiler version nobody has tried: `-Werror`
   with `-Wconversion`, `-Wsign-conversion`, `-Wold-style-cast` and friends
   will say exactly which line.
2. **A platform source that has never compiled** (`src/platforms/linux`,
   `src/platforms/macos`): the leg's log is the first compile of it.
3. **The sanitizer runtime**: ASan/UBSan on the hosted image (on Windows, the
   ASan runtime DLL that `adl_deploy_sanitizer_runtime` copies next to the
   tests, found next to the compiler).
4. **The vcpkg submodule**: a clone without enough history for the baseline
   (`fetch-depth: 0` is set for that reason).
5. **Coverage**: gcovr refusing a counter; the report step passes
   `--gcov-ignore-parse-errors negative_hits.warn_once_per_file` for the known
   gcov bug, and the coverage preset updates counters atomically.

## Not here yet

clang-tidy (the `.clang-tidy` file exists; there is no job), the Release
presets, packaging the Qt application, a coverage threshold (and coverage of the application), and the
embedded (VxWorks) target. Each is a new job or a flag flip, not a redesign.
