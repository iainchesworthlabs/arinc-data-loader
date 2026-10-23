# Changelog fragments

A change that a person using the program would notice is recorded here, as one small file
in the pull request that makes it. At release time `tools/release/Changelog.ps1` assembles
the files into a dated section of `CHANGELOG.md` and removes them
([docs/releasing.md](../docs/releasing.md)).

## Writing one

Create `changelog.d/<name>.<category>.md`:

- `<name>` is lower case letters, digits, `.`, `_` and `-`: a short topic, or the number of the
  issue or pull request (`1234`, `rtos-ports`, `crc32-reflection`);
- `<category>` is one of the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) headings:
  `added`, `changed`, `deprecated`, `removed`, `fixed` or `security`;
- the file holds the entry's text and nothing else: one paragraph that opens with a bold summary
  sentence (`**Summary.** The rest.`), wrapped as you like, with no leading `- ` and no heading. Say what a user will notice, in the user's words, and link the
  page in `docs/` that explains it. Two entries are two files.

```text
changelog.d/mock-target-stop-crash.fixed.md
changelog.d/rtos-ports.added.md
```

Several categories in one pull request are several files. `summary.md` is a free paragraph that
opens a release's section; it is written once, by the person preparing the release.

## When there is nothing to write

A pull request that nobody using the program would notice (a test, a refactor, a CI change,
a documentation fix) needs no fragment. Give it the `no-changelog` label instead: the check
`Changelog fragment` passes on the label, and the maintainer sees that the omission was
deliberate. Pull requests from `release/*`, `hotfix/*` and `dependabot/*` branches are exempt.

## Checking

```sh
pwsh ./tools/release/Changelog.ps1 -Mode Check
pwsh ./tools/release/Changelog.ps1 -Mode PullRequest -Base origin/develop
```
