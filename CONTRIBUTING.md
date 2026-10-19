# Contributing

Thanks for taking a look at ARINC Data Loader. This guide is short: it is meant
to save you a round trip on the handful of things that are not obvious from the
code.

## Before you start

For anything more than a typo fix, it is worth opening an issue first to say
what you are thinking. It avoids duplicated effort and lets us agree on the
approach before you spend time on it. A security problem is the exception:
report it privately, as the [security policy](SECURITY.md) says, not in an
issue. Everyone taking part is held to the [code of conduct](CODE_OF_CONDUCT.md).

Please check [docs/roadmap.md](docs/roadmap.md) and
[docs/architecture.md](docs/architecture.md) for what is planned and what is
deliberately not built yet.

## Branching model

This project follows
[gitflow](https://nvie.com/posts/a-successful-git-branching-model/):

- **`develop`** is the integration branch. Target your pull request here. It is
  protected: nothing is pushed to it directly (see
  [docs/branch-protection.md](docs/branch-protection.md)).
- **`main`** receives releases only: a `develop` to `main` release pull
  request, or a `hotfix/*`. Day-to-day work never targets it.
- Topic branches start with `feature/`, `bugfix/`, `release/`, `hotfix/` or
  `support/`, followed by a short name of letters, digits, `.`, `_` and `-`. A
  pull request from any other name is refused by a CI check, and a `pre-push`
  hook in `tools/hooks` tells you sooner (see below).
- Pull requests are merged with a **merge commit**. Squash and rebase merges
  are not used, so the history you wrote is the history that lands.

## Set up your build

[docs/building.md](docs/building.md) has the requirements, the presets and the
options. The short version, on Windows:

```sh
git clone --recurse-submodules https://github.com/iainchesworthlabs/arinc-data-loader
cd arinc-data-loader
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Use `linux-gcc-debug` or `macos-clang-debug` on those systems. All three are
built and tested by CI; a problem on a compiler or system it does not cover is a
useful report.

## Tests are not optional

New behaviour comes with a test, and a bug fix with the test that failed before
it. [docs/testing.md](docs/testing.md) says where each kind of test lives and
how to add one. Debug builds run under AddressSanitizer, and every first-party
target is compiled with warnings as errors, so a warning is a build failure:
fix it rather than silence it. The nightly suite adds clang-tidy, MSVC Code
Analysis and Clang's sanitizers; a red night is worth a look, but it does not
block a pull request ([docs/static-analysis.md](docs/static-analysis.md)).

## Platform code: the one rule that is not about style

The operating system is chosen by CMake, not by the source. Code that differs
between systems lives in `src/platforms/<name>/`, behind a header in
`src/platforms/interfaces/`, and the platform's `platform.cmake` names the files
it compiles. Outside `src/platforms/` and `tests/platform/` there is:

- no `#if`, `#ifdef`, `#else` or `#endif`, and no `#define` or `#undef`, in any
  first-party C++;
- no operating system header (`<windows.h>`, `<unistd.h>`, `<sys/...>`) and no
  call that asks which system this is (`QSysInfo`, `Q_OS_*`, `Qt.platform`);
- no `WIN32`, `APPLE` or `UNIX` test, and no `CMAKE_SYSTEM_NAME`, in any CMake
  file but `cmake/Platform.cmake`, the platform fragments and the toolchain files
  in `cmake/toolchains/`.

`tools/checks/check_platform_isolation.ps1` enforces it, and it also runs as the
`platform.isolation` test, so you will see a violation locally:

```sh
pwsh ./tools/checks/check_platform_isolation.ps1
```

[docs/platform-isolation.md](docs/platform-isolation.md) explains the reasoning,
what to do instead of an `#ifdef`, and how to add a platform.

## Code style

- C++23, with compiler extensions off. The code uses `std::expected` for
  failures that are expected and does not throw for them; follow the style of
  the code around you.
- Formatting follows the repository's [`.clang-format`](.clang-format): run
  `clang-format` before committing, or have your editor do it on save.
- [`.clang-tidy`](.clang-tidy) is the static-analysis configuration: the
  compiler's own warning set is an error in every build, and clang-tidy runs
  with warnings as errors on the nightly analysis (see
  [docs/static-analysis.md](docs/static-analysis.md)). A disabled check carries
  its reason; add one the same way, and use a `NOLINT` only for a finding from
  a system macro or a known-safe call, with the reason beside it.
- Keep the libraries GUI-free and platform-free. Qt belongs in the three
  applications (`src/app`, `src/media_gen`, `src/mock_target`) and the interface
  code they share (`src/ui`), and nowhere else.

## Commits and pull requests

- Write each commit message as a short summary line in the imperative ("Add
  ...", "Fix ...") and, where the reason is not obvious, a few lines on why.
- **Commit messages, pull request titles and pull request descriptions carry no
  attribution of any kind**: no co-author trailers, no line crediting anything
  or anyone with having produced the text or the change, and no mention of the
  tools used to make it. They describe the change and nothing else. A CI check
  applies this rule to every commit of a pull request (`docs/ci.md` describes
  it), and a `commit-msg` hook runs the same script on your machine, if you
  want to be told before you push.
- To turn the hooks on (`commit-msg` for the rule above, `pre-push` for the
  branch name), run this once per clone. The `commit-msg` hook needs PowerShell
  7; without it, it says so and lets the commit through, and CI still checks:

  ```sh
  git config core.hooksPath tools/hooks
  ```

- Keep pull requests focused. A bug fix does not need an unrelated refactor
  riding along with it.
- If a person using the program would notice the change, add a line under
  `Unreleased` in [CHANGELOG.md](CHANGELOG.md).
- If you change what a window shows or does, change [the user's guide](docs/user-guide/index.md)
  with it, and make the pictures again ([docs/screenshots.md](docs/screenshots.md)). The
  `docs.links` test fails on a link or a picture that is not there.
- Do not commit a load, media set or capture taken from real equipment, or
  build output. The `.gitignore` already keeps build output out; do not work
  around it. Nor the ARINC standards: they are kept in `docs/specs/` for
  reference, ignored by git, and not this project's to publish (see
  [docs/licensing.md](docs/licensing.md)).

## Opening a pull request

1. Push your topic branch and open a pull request against `develop`.
2. Make sure the checks are green. [docs/ci.md](docs/ci.md) says what runs, and
   [docs/branch-protection.md](docs/branch-protection.md) how the protected
   branches are set up.
3. Describe what changed and why, and what you ran to check it, on which system.
4. Once the checks are green it goes through the merge queue and is merged with
   a merge commit. The rulesets require no approving review (the project has one
   maintainer), but review comments are still worth settling first.

## Licence of contributions

Contributions are accepted under the project's licence, GPL-3.0-or-later (see
[LICENSE](LICENSE) and [docs/licensing.md](docs/licensing.md)).

That is all. If anything here is unclear or out of date, saying so in an issue
is itself a welcome contribution.
