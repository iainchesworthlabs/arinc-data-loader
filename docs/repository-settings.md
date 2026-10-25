# Repository settings

*For the maintainer. What CI runs is [CI](ci.md); how a release is made is [Releasing](releasing.md).*

The settings that GitHub keeps outside the code (branch rules, the merge queue, labels, repository
options) are files in this repository, applied by one script, `admin/apply.sh` of
[icl-common-infra](https://github.com/iainchesworthlabs/icl-common-infra). Nothing is changed by hand
in the settings pages, so the files are the record.

| File | Holds |
| --- | --- |
| `.github/rulesets/*.json` | The five rulesets below |
| `.github/labels.json` | The label set |
| `.github/repo-settings.json` | This repository's description, homepage and topics. Everything else comes from the script's defaults: squash merge only, auto-merge on, branch deleted on merge, read-only workflow token, SHA pinning required, secret scanning with push protection, Dependabot security updates, private vulnerability reporting, Pages built by Actions, and the `release` environment with the maintainer as reviewer |

## The branch model

`develop` is the trunk and the default branch. Every change reaches it by pull request through the
merge queue. `main` holds published releases: the release workflow moves it to the tagged commit with
the release bot, so `main` is always an ancestor of `develop`. Nobody opens a pull request into `main`.

## The rulesets

| Ruleset | Does |
| --- | --- |
| `develop-protection` | No deletion, no force push, linear history, a pull request that is squash-merged with its conversations resolved, and the required checks below. No bypass |
| `merge-queue-develop` | Merge queue: squash, `ALLGREEN`, up to 4 builds at once |
| `main-release-only` | Only the release bot can move `main`; no deletion, no force push |
| `tags` | A `v*` tag cannot be moved or deleted; it is created by the maintainer or the release bot |
| `branch-names` | A branch may be created only as `develop`, `main`, `feature/*`, `bugfix/*`, `docs/*`, `chore/*`, `agent/*`, `release/*`, `dependabot/*` or the queue's `gh-readonly-queue/*` |

The required checks are `CI Status` and `Scan dependency diff`, both defined in `ci.yml`. A check from a
called workflow is reported as `<caller job> / <called job>`, so the aggregator job `CI Status` is what the
ruleset names; it fails when any job the plan asked for failed, was cancelled or was skipped
([CI](ci.md#required-checks)). Every workflow that produces a required check also triggers on
`merge_group`, because the queue evaluates the checks on its own temporary ref.

## Applying the settings

Run from a checkout of this repository, with `icl-common-infra` next to it and a token with
administration rights for the repository (`gh auth login` is enough):

```sh
export OWNER_USER=iainchesworth RELEASE_BOT_APP_ID=<numeric id of the release app>
../icl-common-infra/admin/apply.sh --repo iainchesworthlabs/arinc-data-loader            # dry run: prints each call
../icl-common-infra/admin/apply.sh --repo iainchesworthlabs/arinc-data-loader --apply
```

It is idempotent. `--only` limits it to some of `settings,actions,security,pages,environments,labels,
rulesets,variables`. The script creates and updates; it does not delete. A ruleset that is no longer
in the files, such as the old `main-protection`, is removed in **Settings, Rules**. Check afterwards
with `gh api repos/iainchesworthlabs/arinc-data-loader/rulesets`.

## Environments and secrets

| Environment | Holds | Who can reach it | Waits for |
| --- | --- | --- | --- |
| `release` | nothing; the shared signing secrets are organisation secrets | jobs on `v*` tags only | the maintainer's approval |
| `github-pages` | nothing | jobs on `main` only (`Publish the site` in `docs.yml`) | nothing |

[Releasing](releasing.md) has the release secrets and variables (`SIGN_WINDOWS`, `SIGN_MACOS`). The
Dependabot secret `MERGE_QUEUE_TOKEN` lets the auto-merge workflow put a Dependabot pull request in the
queue.

## Dependabot pull requests

Dependabot opens its pull requests against `develop` from `dependabot/*` branches. They go through the
same required checks and the same queue as any other pull request; nothing is bypassed. Minor and patch
updates of the Actions and the CI tooling are queued by `dependabot-auto-merge.yml` once the checks
pass. A major update, and every vcpkg baseline update, waits for a person.

## Other scanners

`scorecard.yml` publishes a score from `develop`, the default branch; it is not a check. CodeQL results appear under
**Security, Code scanning**. CodeQL runs nightly, with clang-tidy, MSVC Code Analysis, a Clang ASan/UBSan
build and the optional SonarQube scan ([Nightly static and dynamic analysis](static-analysis.md)). None of
them is a required check, so none of them carries `merge_group`.
