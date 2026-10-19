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
| `ci.yml` | `clang-format`, `platform-isolation`, the Windows / Linux / macOS build-and-test legs and a coverage gate | yes |
| `branch-name-check.yml` | gitflow branch naming; the source/target pairing | yes |
| `commit-policy.yml` | no attribution in the PR title, PR body or any commit message of the PR | yes |
| `changelog.yml` | a PR into `develop` adds a changelog fragment, or carries the `no-changelog` label | yes |
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
(`main`). These two workflows live on `develop` until the first release merge, so
nothing runs nightly before then; the per-commit checks above are unaffected.
The same holds for everything GitHub reads only from the default branch: Dependabot
(`.github/dependabot.yml`), `scorecard.yml`, the issue templates and `SECURITY.md`'s
private-reporting link. None of them is active until `main` carries them.

`static-analysis.yml` and `codeql.yml` run on a nightly schedule, not on a
push or a pull request. They are not required checks: a red night is read in
the morning, and a merge is not held for an analyzer. The jobs are clang-tidy
over the libraries (`src/`), MSVC Code Analysis over the libraries, tests and
Qt applications, a Clang ASan/UBSan build and test run, CodeQL's C++ queries,
and an optional SonarQube scan that is skipped until it is configured. The
reason for each, the local presets and the suppression rules are in
[Nightly static and dynamic analysis](static-analysis.md).

## The release workflow

`release.yml` is described in [Releasing](releasing.md) and checked from the outside by
[Verifying a release](verifying-releases.md). Where it sits among the others:

- It is not a required check and carries no `merge_group` trigger. On a pull request it runs only
  when the change touches the packaging (`cmake/`, `packaging/`, `src/platforms/*/platform.cmake`,
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
| `clang-format` | ubuntu-24.04 | Every `.cpp`/`.hpp` (and sibling extensions) under `src/` and `tests/` is exactly what the repository's `.clang-format` produces, with one pinned clang-format release (`.github/requirements/lint.txt`). Any difference fails; the log names file and line. |
| `platform-isolation` | ubuntu-24.04 | `tools/checks/check_platform_isolation.ps1`: no preprocessor conditional, no OS header or OS-querying API outside `src/platforms/`, no OS test in CMake outside the selector, no `#define`, every platform source wired. Fast, and deliberately not behind the build legs. |
| `Plan build legs` | ubuntu-24.04 | The build matrix was computed. It is a required check only so that a failure here cannot turn the three build legs into "skipped" (which counts as a pass). |
| `Windows (MSVC)` | windows-2025-vs2026 | Preset `windows-msvc-debug` configures, builds warning-free (`/W4 /WX`) with ASan, and all of ctest passes. |
| `Linux (GCC)` | ubuntu-26.04 | Preset `linux-gcc-debug`: the image's default `g++` builds warning-free (`-Wall -Wextra -Wpedantic -Werror ...`) with ASan and UBSan, and all of ctest passes. |
| `macOS (AppleClang)` | macos-26 (Apple silicon) | Preset `macos-clang-debug`: AppleClang from Xcode builds warning-free with ASan and UBSan, and all of ctest passes. |
| `Coverage (Linux, GCC)` | ubuntu-26.04 | Preset `linux-gcc-coverage` passes every test and **meets the project minimum of 90% lines and 80% branches of the portable code** (the libraries, the engine, the media tree, the samples, the payload writer and the platform interfaces; the OS fragments are held by the conformance suite instead). `cmake/CoverageReport.cmake` runs gcovr and fails the job below either figure; the `coverage-report` artifact holds the HTML, `summary.json` and `cobertura.xml`, and the job summary carries the table. The preset does not build the Qt applications; their windows are covered by the UI suite on the build legs. |

What a build-and-test leg's ctest contains: the Catch2 suites, the application's `app.tests` (QtTest),
`app.qml` (the window loaded offscreen, with the offscreen platform and the software renderer, so no GPU
or display is needed), the Qt Quick Test suites `ui.tst_main_window` and `ui.tst_dialogs` (the window
and its dialogs driven by mouse and key events on the real `Main.qml`), the system smoke tests that start
the three application programs, and the two repository checks `platform.fragments` (every platform
fragment satisfies the platform contract, on whichever host this is) and `platform.isolation` (the same
script as the `platform-isolation` job; it needs `pwsh`, which every hosted image has). A hung test has
a time-out, so it cannot hold the job.

### Toolchain choices

- **No compiler install.** The code needs `<expected>` and `<format>`, which
  libstdc++ has from GCC 13 and Apple's libc++ has in current Xcode, so each
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
  and it sets `QT_ROOT_DIR`, which `cmake/FindQt6.cmake` looks in first. The Qt version is the action's
  `version` input in `ci.yml` and `release.yml` (the same 6.12.0 as the labs' shared toolchain); the matrix
  has the host and architecture names. The coverage job does not build the application, so it needs no Qt,
  and its figures are of the portable code; the UI suite runs on the three build legs.

  **Moving to another Qt version** is deliberate and reviewed: change the `version` inputs, then, with the
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

### Running some legs by hand

A push, pull request or queue run always runs every leg. A manual dispatch can
name just the ones it needs - useful in a fork or private copy, where hosted
minutes are limited and macOS minutes are billed at a multiple:

```sh
gh workflow run ci.yml --ref <branch> -f legs=macos
gh workflow run ci.yml --ref <branch> -f legs=clang-format,linux
```

`legs` is comma-separated with no spaces: `clang-format`, `platform-isolation`,
`windows`, `linux`, `macos`, `coverage`, or `all` (the default). An unknown
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
| `Changelog fragment` | `changelog.yml` | no - not yet required; see below |
| `Analyze (C++)` | `codeql.yml` | no - informational |

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
| clang-format, gcovr | `==` in `.github/requirements/*.txt` | Dependabot (`pip`), weekly |
| clang-tidy (nightly analysis) | `==` in `.github/requirements/static-analysis.txt` | Dependabot (`pip`), weekly |
| py7zr and its dependencies (unpack Qt) | `==` with every wheel's SHA-256 in `.github/requirements/qt.txt`, compiled from `qt.in`, installed with `--require-hashes --no-build` | Dependabot (`pip`), weekly |
| The Qt archives | The SHA-256 of each, in `.github/requirements/qt-archives.sha256` | By hand: `install_qt.py lock` |
| Qt | `version` input of the `install-qt` step in `ci.yml` and `release.yml` (6.12.0) | By hand, with the labs' shared toolchain |
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
   (`fetch-depth: 0` is set for that reason).
5. **Coverage**: gcovr refusing a counter, or a figure below the minimum. The
   report step passes `--gcov-ignore-parse-errors negative_hits.warn_once_per_file`
   for the known gcov bug, and the coverage preset updates counters atomically.
   The artifact names the file and line under the minimum; add the test that
   covers it, or remove the dead code.

## Not here yet

The Release presets, packaging the Qt application, and the embedded (VxWorks)
target. Each is a new job or a flag flip, not a redesign. clang-tidy is no longer on this list: it is
a nightly job, with the rest of the static analysis, in
[static-analysis.md](static-analysis.md). A Release-only warning
(`-Wnull-dereference`, for one) still first shows up locally.
