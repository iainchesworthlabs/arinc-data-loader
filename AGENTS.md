# Instructions for coding agents

This file applies to any automated contributor. Read it, and [CONTRIBUTING.md](CONTRIBUTING.md), before
changing the repository.

## Working rules

- Work on a branch named `<prefix>/<slug>` with the prefix `feature`, `bugfix`, `docs`, `chore` or `agent`, and a
  lower-case slug with hyphens. Never commit to `develop` or `main` directly.
- Commit with `git commit -s`. Write the subject in the imperative, 72 characters or fewer, and the body as a plain
  description of what changed and why.
- Open pull requests against `develop`, as drafts until the work is complete. Fill in the pull request template.
  Squash merge is the only method.
- Run `python3 tools/ci/precheck.py` before pushing. It runs the checks of the Policy job on committed work.
- Build and test with the presets in `CMakePresets.json` ([docs/building.md](docs/building.md)). A change to code
  runs `linux-debug` and `linux-asan-ubsan` locally before the pull request. A change that touches threads, the
  standard library or platform code is tried on macOS and Windows with `gh workflow run ci.yml --ref <branch>
  -f legs=macos,windows` before it is queued.
- Do not create or edit secrets, tags, releases, repository settings or rulesets. Those are done by the owner.
- Do not edit `cmake/icl/`. Change `icl-common-infra` and run `tools/ci/repin.py`.
- Do not change a public interface (the `adl` CMake package and its targets, the command-line options, the file
  formats, what the programs write) as part of another change. Say so and ask.

## Code rules

- C++23. There are no preprocessor conditionals or definitions in the code, and none may be added: the operating
  system is chosen by CMake, in `libs/platform/<name>/`, behind the interfaces in `libs/platform/interfaces/`
  ([docs/platform-isolation.md](docs/platform-isolation.md)). The count is held at zero by
  `tools/preprocessor-baseline.json`.
- Names are this repository's own, not the template's: `snake_case` for namespaces, functions, methods, variables,
  constants and enumerators; `PascalCase` for types; a trailing underscore on private members (`name_`). The classes
  QML reads use `camelCase` for what QML sees. `.clang-tidy` states it and the nightly run checks it
  ([docs/architecture.md](docs/architecture.md#names-in-the-code)). The properties, signals, slots and invokable functions that QML
  sees are `camelCase`. Allman braces; run `clang-format` on changed files (`.clang-format` is the template's).
- Return `std::expected` for recoverable errors in library code. Do not throw across a library boundary.
- Every new behaviour has a test with a label (`unit`, `integration`, `system`, `ui`). Every function that parses
  bytes from outside has a fuzz harness ([docs/fuzzing.md](docs/fuzzing.md)).
- A warning is an error. Fix it; do not silence it.

## If you change X, update Y

| You change | Also update |
|---|---|
| A user-visible behaviour | A fragment in `changelog.d/` ([changelog.d/README.md](changelog.d/README.md)) |
| A public header in `libs/*/include` | The tests, and [docs/architecture.md](docs/architecture.md) when the layout changes |
| What a parser accepts | Its fuzz seeds if they no longer decode (`python3 tools/fuzz/make_seeds.py`) |
| A window's text | The catalogues in each `translations/` directory (`update_translations`), and the screenshots if the picture changes |
| A dependency in `vcpkg.json` | Nothing else; Dependabot and the baseline check cover the fork |
| A CMake option or preset | [docs/building.md](docs/building.md) |
| Test labels | [docs/testing.md](docs/testing.md) |
| A workflow in `.github/workflows` | [docs/ci.md](docs/ci.md); pin every `uses:` to a full SHA with a version comment |
| Which paths need a build | `.github/ci/plan.json` |
| A benchmark | `perf-budget.json` through `tools/ci/perf_compare.py --update` ([docs/performance.md](docs/performance.md)) |
| A platform | `libs/platform/<name>/platform.cmake` and [docs/platform-abstraction.md](docs/platform-abstraction.md) |
| A message the code shows (a `tr()` in C++) or an exit code | The guide: [docs/user-guide/messages.md](docs/user-guide/messages.md) or the page that describes it, and [docs/user-guide/command-line.md](docs/user-guide/command-line.md). `python3 tools/ci/check_messages.py` fails until it is there |
| A page's address, or whether a page is published (`mkdocs.yml`, `exclude_docs`, a file name in `docs/`) | `tools/docs/site_layout.json`: the old address must keep leading somewhere ([docs/ci.md](docs/ci.md#the-site-layout)) |
| A front page of the site (a page with `template: marketing.html`) | Nothing else: it is script-free, has no inline style, uses relative links and has no `mailto:` ([docs/ci.md](docs/ci.md#the-rules-for-the-pages)); say only what the code, a test or a golden file shows |
| A colour of the site | `docs/theme.css` only, then `python3 tools/ci/check_site_kit.py` |
| The company kit copy in `tools/docs/site-kit/` | Do not edit it. Copy the newer kit over it and name its version in the pull request |
| The site's address | Do not: it is fixed at `https://iainchesworthlabs.com/adl/`. Ask |
| A new source file | The SPDX line, copied from an existing file |
