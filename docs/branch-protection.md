# Branch protection, rulesets and the merge queue

GitHub's repository rules cannot be expressed as a workflow file. Here they are
kept as code, in `.github/rulesets/*.json` (the JSON body of the REST API's
"Create a repository ruleset"), and `tools/repo/apply_rulesets.ps1` is the one
way they reach GitHub. This page records what they say and why, so the settings
can be rebuilt or audited from the page alone.

## Status: applied

The repository is public, and the rulesets in `.github/rulesets` were applied with
`tools/repo/apply_rulesets.ps1 -Apply` when the repository was created. They could
not have been while it was private: on GitHub Free, rulesets, branch protection
and the merge queue are not available for a private repository, and the API
answers

```text
GET /repos/{owner}/{repo}/rulesets  ->  403
"Upgrade to GitHub Pro or make this repository public to enable this feature."
```

The repository settings that go with them match the other labs projects: merge
commits only (no squash or rebase), auto-merge allowed, branches deleted after
merge, and no wiki.

To see what is applied now, or to change a rule, edit the JSON and run

```sh
./tools/repo/apply_rulesets.ps1          # dry run: prints what it would send
./tools/repo/apply_rulesets.ps1 -Apply   # create or update, by ruleset name
```

The script checks that rulesets are available first and stops with an
explanation when they are not. The local hooks in `tools/hooks` and the
workflows ([CI, text policy and local hooks](ci.md)) keep the same rules
before a push reaches GitHub.

## The flow

```text
feature/* bugfix/* ──PR──▶ merge queue ──▶ develop ──▶ release/* ──PR──▶ main
                                                                          ▲
                                                      hotfix/* support/* ─┘
```

- **Topic branches** (`feature/*`, `bugfix/*`, `release/*`, `hotfix/*`,
  `support/*`, plus Dependabot's `dependabot/*`) open a pull request against
  `develop`; a ready PR goes into the **merge queue**. Nobody pushes to
  `develop` directly.
- **`release/*` → `main`** is a release step: the release branch is cut from
  `develop` ([Releasing](releasing.md)) and merged with a merge commit, never a
  squash or rebase (or the two branches diverge and the next release PR re-lists
  every commit), and then back into `develop`. Hotfixes go to `main` from
  `hotfix/*` and are merged back into `develop` through a PR.
  `branch-name-check.yml` accepts `release/*`, `hotfix/*`, `support/*` and
  `develop` itself as the source of a pull request into `main`; the release
  process uses the first two.
- **Head branches are deleted** when their PR merges.

## The rulesets

| File | Ruleset (name) | Applies to | What it does |
| --- | --- | --- | --- |
| `main-protection.json` | `main-protection` | `refs/heads/main` | Pull request required; required checks with "up to date" **on**; conversations resolved; no deletion; no force push. |
| `develop-protection.json` | `develop-protection` | `refs/heads/develop` | Pull request required; required checks with "up to date" **off** (the queue makes each entry current); no deletion; no force push. |
| `merge-queue-develop.json` | `merge-queue-develop` | `refs/heads/develop` | The merge queue: merge commits, `ALLGREEN`, build up to 4 entries and merge up to 5, wait 1 minute, 180-minute check timeout. |
| `branch-names.json` | `branch-names` | every branch except the gitflow names (`~ALL` minus `main`, `develop`, and the namespaces `feature/`, `bugfix/`, `release/`, `hotfix/`, `support/`, `dependabot/` and `gh-readonly-queue/` to the depths listed below) | Creating any other branch is refused. No bypass actors. |

### `main-protection` and `develop-protection`

- **Require a pull request**, with **0 required approvals** and stale
  approvals dismissed on push. Zero, not one: this is a solo-maintainer
  repository and GitHub does not count an author's own approval, so a required
  approval could only ever be met by bypassing it. Merge method: **merge
  commit only**.
- **Require status checks**, the list in the table below. `main` also requires
  the branch to be up to date and review conversations to be resolved.
- **Block deletion and non-fast-forward** (force) pushes.
- The repository **admin role may bypass through a pull request only**
  (`bypass_mode: pull_request`): a direct push stays blocked even for an admin,
  and skipping a requirement is a deliberate "merge without waiting" on a PR.

### `merge-queue-develop`

One `merge_queue` rule. A PR enters the queue once it is ready (or earlier,
with auto-merge: `gh pr merge --auto --merge <n>`). GitHub then builds a
temporary `gh-readonly-queue/develop/pr-<n>-<sha>` branch - the PR merged onto
the queue's tip - re-runs the required checks on *that* tree, and merges only
if they pass. This is why every workflow that produces a required check also
triggers on `merge_group` (see [Required checks](#required-checks)).

### `branch-names`

A **creation** restriction on every branch except the gitflow namespaces (the
`exclude` list in the file). A branch named anything else - a tool-named session
branch, a typo, a bare `fix-thing`, `feature-x` - is refused when it is
created, at push time, before any workflow runs:

```text
 ! [remote rejected] main -> wip/ruleset-check (push declined due to repository rule violations)
```

There are **no bypass actors**: not even an admin can push a non-matching name.
The same allow-list is applied earlier and softer by `tools/hooks/pre-push`
(locally) and `Validate gitflow branch name` (on the pull request).

**What the allowed names are.** One `*` per path segment, so the depth of each
namespace is explicit:

| Namespace | Levels allowed | Why that many |
| --- | --- | --- |
| `feature/`, `bugfix/`, `release/`, `hotfix/`, `support/` | 1 to 3 (`feature/area/sub/name`) | a name and a couple of groupings |
| `dependabot/` | 1 to 5 | its branches are `dependabot/<ecosystem>/<dependency or directory>/...` |
| `gh-readonly-queue/` | 1 to 4 | the merge queue names its branch `gh-readonly-queue/<base>/pr-<n>-<sha>` |

**Why a creation rule and not a name pattern.** GitHub also offers a
`branch_name_pattern` rule ("restrict branch names"). On this plan the API
accepts it and creates it, and then **does not enforce it**: a branch that
broke the pattern was pushed without complaint, with the `regex` operator and
with `starts_with` alike (checked live on 2026-10-15). A rule that is created and
silently ignored is worse than none, so the allow-list is written as include and
exclude globs on a `creation` rule, which is enforced. If this repository moves
to a plan where metadata rules are enforced, the pattern rule could replace it.

**Why `*` per level and not `**`.** In a ruleset glob, `**` stands for a single
path segment, not for "any depth". The first version of this rule used
`refs/heads/gh-readonly-queue/**`, which does not match the two-segment name the
merge queue creates; the queue therefore could not create its temporary branch
and removed every entry about 16 seconds after it was added, with the reason
`branch_protection_failure`, before a single check ran. The same version would
have refused every Dependabot branch. Both were found by a pull request that
could not merge, not by reading the documentation, and the rule is now tested
against the exact shapes above, including names that must be refused
(`feature-x`, `wip/a/b`, a fifth level).

Consequences worth knowing:

- **The queue prefix is excluded on purpose.** Without it, the merge queue
  cannot create its branch and no pull request can be merged into `develop`.
- **GitHub's "Revert" button on a merged PR** creates a `revert-<n>-...`
  branch, which is refused. Revert with `git revert` on a gitflow-named branch
  instead.
- **A ref name cannot have a component that starts with a dot** (git itself
  refuses it), so a directory such as `.github` never appears in a branch name.
- Branches created in the web UI must follow the same names.
- Existing non-conforming branches are not renamed by the rule; delete or
  rename them.

## Required checks

The required contexts equal the job display names of the five workflows that
produce them:

| Check (context) | Workflow | Required on `develop` | Required on `main` |
| --- | --- | --- | --- |
| `Validate gitflow branch name` | `branch-name-check.yml` | yes | yes |
| `Validate commit and PR text` | `commit-policy.yml` | yes | yes |
| `clang-format` | `ci.yml` | yes | yes |
| `platform-isolation` | `ci.yml` | yes | yes |
| `Plan build legs` | `ci.yml` | yes | yes |
| `Windows (MSVC)` | `ci.yml` | yes | yes |
| `Linux (GCC)` | `ci.yml` | yes | yes |
| `macOS (AppleClang)` | `ci.yml` | yes | yes |
| `Coverage (Linux, GCC)` | `ci.yml` | yes | yes |
| `Scan dependency diff` | `dependency-review.yml` | yes | yes |
| `Docs build (strict)` | `docs.yml` | yes | yes |

`Docs build (strict)` reports only on a branch that has `docs.yml`. A release branch cut before `docs.yml`
existed has to merge `develop` first, or the required check never appears on its pull request into `main`.

### Staging of `Scan dependency diff`

`dependency-review.yml` is public-only, and it needs the dependency graph switched
on in the repository's security settings (Settings, Advanced Security); without it
the job fails with "Dependency review is not supported on this repository".
Requiring a check that has never reported leaves every pull request waiting on
it, so it was added to the JSON files only after it had passed on a pull request:

1. Make the repository public and apply the rulesets (above).
2. Switch on the dependency graph, open a pull request, and let `Scan dependency
   diff` report.
3. Add `{ "context": "Scan dependency diff" }` to the `required_status_checks`
   list in `develop-protection.json` and `main-protection.json`, commit, and
   run `apply_rulesets.ps1 -Apply` again (it updates by name).

This was done for the first pull request. Do it in this order for any other
repository set up from this one.

`Analyze (C++)` (CodeQL) is informational and is not required.

### The simulator jobs are not required yet

`Zephyr (native_sim)`, `Zephyr (QEMU, SDK toolchain)` and `FreeRTOS (POSIX simulator)` (`ci.yml`)
run on every pull request and are **not** in `required_status_checks`: they download Zephyr, FreeRTOS, lwIP and littlefs at run
time, and a required check that fails because a mirror was down blocks every pull request. Add
`{ "context": "Zephyr (native_sim)" }`, `{ "context": "Zephyr (QEMU, SDK toolchain)" }` and
`{ "context": "FreeRTOS (POSIX simulator)" }` to both ruleset files, as for `Scan dependency diff`, once each has been green on a run of pull requests and a merge
queue entry (they also run on `merge_group`, as every job of `ci.yml` does), and run `apply_rulesets.ps1
-Apply` again. A manual dispatch names them `dispatch - Zephyr (native_sim)`, `dispatch - Zephyr (QEMU, SDK toolchain)` and
`dispatch - FreeRTOS (POSIX simulator)`, so a run that skips them cannot report "skipped" under a required name.

### The `merge_group` rule

Two rules keep the queue honest:

- **Every workflow that produces a required check must also trigger on
  `merge_group`.** GitHub only runs workflows that opt in on the queue's
  temporary ref; a required check from one that does not never reports, and
  the entry sits until `check_response_timeout_minutes` expires.
  `ci.yml`, `branch-name-check.yml`, `commit-policy.yml`,
  `dependency-review.yml` and `docs.yml` carry it.
- **A workflow that produces no required check must not carry
  `merge_group`**, or every queue entry burns a full run of it for nothing.
  `codeql.yml`, `static-analysis.yml` and `scorecard.yml` do not.

A queue entry has no head branch name, PR title or PR body of its own, so
`Validate gitflow branch name` and `Validate commit and PR text` skip their job
there (checked when the PR was opened or edited); a skipped job counts as a
pass for a required check. Changing a required check's name in a workflow
without changing the rulesets leaves a context that never reports: change both
in one commit.

## Applying the rulesets

`tools/repo/apply_rulesets.ps1` creates or updates each ruleset **by name**
through the `gh` CLI, so it is idempotent. It is a **dry run by default**: it
reads (GET only), prints the exact payload it would send for each file and
whether that would be a create, an update or "unchanged", and sends nothing.

```sh
./tools/repo/apply_rulesets.ps1                    # dry run, prints what it would send
./tools/repo/apply_rulesets.ps1 -Offline           # just print the payloads; no API call
./tools/repo/apply_rulesets.ps1 -Apply             # really create / update
./tools/repo/apply_rulesets.ps1 -Only branch-names -Apply
./tools/repo/apply_rulesets.ps1 -Repo owner/name   # not the current directory's remote
```

It first asks whether the repository can have rulesets (public, or a paid
plan) and stops with an explanation - exit code 3 - when it cannot. It needs
the GitHub CLI authenticated as a repository admin.

First-time setup of a repository, in order:

1. Create `main` and `develop`; make `main` the default branch.
2. Make the repository public.
3. `./tools/repo/apply_rulesets.ps1` (dry run), read it, then `-Apply`.
4. Set the repository settings below.
5. Switch on the dependency graph, and after the first pull request stage in
   `Scan dependency diff` (above).

## Repository settings

Set by hand in the repository's settings (not expressible as a ruleset):

- **Allow auto-merge**: on - it is how a ready PR goes into the queue.
- **Automatically delete head branches**: on.
- **Allow merge commits**: on. Squash and rebase merging: off (the rulesets
  already restrict `main` and `develop` to merge commits).
- **Actions → Workflow permissions**: read repository contents only; the
  workflows request anything more per job.
- **Actions → Fork pull request workflows**: require approval for first-time
  contributors.

## Environments and tags

Releases add two repository **environments**, and publishing the documentation site a third. They are
settings and not rulesets (`tools/repo/apply_rulesets.ps1` does not touch them; [Releasing](releasing.md)
has the table):

| Environment | Holds | Who can reach it | Waits for |
| --- | --- | --- | --- |
| `release-signing` | the release key and its passphrase | jobs on `release/*` branches and `v*` tags only | nothing |
| `release` | nothing | jobs on `v*` tags only | the maintainer's approval |
| `github-pages` | nothing | jobs on `main` only (`Publish the site` in `docs.yml`) | nothing |

No ruleset protects tags yet: a `v*` tag can be created and moved by anyone with write access, and a
tag run publishes only a **draft** release after the maintainer approves it, which is the safeguard
today. A tag ruleset (creation by the maintainer only, no deletion or update of `v*`) is the next
hardening, and is a decision for the maintainer: it changes who can release. The changelog check
(`Changelog fragment`) is a visible check and not yet a required one.

## Dependabot pull requests

Dependabot opens its pull requests against `develop` from `dependabot/*`
branches (its prefix is not configurable, so `branch-name-check.yml` and the
`branch-names` ruleset allow it). They go through the same required checks and
the same queue as any other pull request; nothing is bypassed. Commit messages
and titles Dependabot writes carry no attribution, so
`Validate commit and PR text` passes them.

## Other scanners (visible-only)

`scorecard.yml` publishes a score and a badge from `main` and re-runs when the
rulesets change; it scores more completely with a read-only fine-grained token
in the `SCORECARD_READ_TOKEN` secret (see the comments in the workflow). It is
not a check. CodeQL results appear under **Security -> Code scanning** and are
triaged there; CodeQL runs nightly, with clang-tidy, MSVC Code Analysis, a
Clang ASan/UBSan build and the optional SonarQube scan
([Nightly static and dynamic analysis](static-analysis.md)). None of those is a
required check, so none of them carries `merge_group`.
