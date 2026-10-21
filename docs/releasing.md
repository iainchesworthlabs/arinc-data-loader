# Releasing

*For the maintainer. How a downloader checks a release is [Verifying a release](verifying-releases.md);
what CI runs on every change is [CI](ci.md).*

A release is a version tag on `main`. Pushing the tag runs `.github/workflows/release.yml`, which first
builds the documentation site strictly, then builds
and tests the Release configuration on Windows, Linux and macOS, packages it, writes the SBOM and the
checksums, signs everything with the release key, attests it, and drafts a GitHub release. Nothing is
published until the maintainer reads the draft and presses the button.

## What a release contains

| File | Holds |
| --- | --- |
| `arinc-data-loader-<v>-windows-x86_64.zip` | A portable folder: `adl_app.exe`, `adl_media_gen.exe`, `adl_mock_target.exe`, `adl_write_payloads.exe`, the Qt DLLs, plugins and QML modules Qt's `windeployqt` places beside them, `README.txt`, `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-<v>-linux-x86_64.tar.gz` | `bin/` (the four programs), `lib/` and `plugins/` and `qml/` (the Qt files), `share/applications/*.desktop`, `share/icons/hicolor/scalable/apps/*.svg`, `share/doc/arinc-data-loader/{LICENSE,NOTICES}.txt` |
| `arinc-data-loader-<v>-macos-arm64.dmg` | The three application bundles (Qt frameworks and QML modules inside, signed ad hoc, each with `LICENSE.txt` and `NOTICES.txt` in `Contents/Resources`), `tools/adl_write_payloads`, an `Applications` link to drag them onto, `README.txt`, `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-dev-<v>-<system>-<arch>.zip` / `.tar.gz` | The development package: headers, static libraries and the `adl` CMake package (below), `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-<v>.spdx.json`, `.cdx.json` | The software bill of materials (SPDX 2.3, CycloneDX 1.6) |
| `<leg>-qt<module>-6.12.0.spdx.json`, `.cdx.json` | The Qt kit's own SBOM documents for the modules the packages hold; the SPDX document refers to them |
| `SHA256SUMS`, and `<file>.asc` for every file | The checksums, and the detached PGP signatures |

Both the runtime and the development archive unpack into a directory named
`arinc-data-loader-<v>-<system>-<arch>`, so extracting both in one place gives one tree.

### The development package

It holds the libraries a program can build on without Qt and without an operating-system header of its
own, and the platform interfaces they run on:

| Target | What it is |
| --- | --- |
| `adl::arinc665` | ARINC 665 media sets, loads, batch files and check values |
| `adl::arinc615a` | The ARINC 615A codec and the data loader's state machines (no sockets, no threads) |
| `adl::arinc615a_runner` | The runner that drives those state machines over `adl::platform`'s sockets, and a file source and sink over a directory |
| `adl::media_tree` | A media set as a directory tree on disk |
| `adl::platform` | The platform interfaces (files, memory, threads, clock, UDP sockets, queues) and this host's implementation of them |

```cmake
find_package(adl 0.1 REQUIRED)
target_link_libraries(my_tool PRIVATE adl::media_tree)
```

They are static libraries, so the package needs nothing at run time. **Not** shipped, on purpose:
`adl::samples` and `adl::arinc615a_mini` are test fixtures (the second, independent TFTP
implementation exists to check the engine against, and is not an interface), and the code behind the
applications (`src/app`, `src/ui`, ...) is written for those programs and for Qt. Before 1.0 the API is not
stable: the package's version file accepts the same minor version only (`SameMinorVersion`).

### What is not shipped, and why

- **No Windows installer, no `.deb`, no AppImage.** An installer that is not signed with a certificate
  the system trusts earns the same warning as the zip it wraps; a distribution package that carries its
  own Qt belongs to someone who keeps it patched for that distribution; an AppImage needs a build tool
  fetched at packaging time that this repository has no pin for. The tar.gz carries `.desktop` files
  and icons for anyone who wants to wrap it.
- **No Authenticode signature, no Developer ID signature, no notarisation.** They need certificates the
  project does not have. The PGP signature and the attestation are what is offered instead.
- **Only the architectures CI builds:** Windows x86-64, Linux x86-64, macOS arm64. The macOS programs
  need macOS 14.4 or later (`CMAKE_OSX_DEPLOYMENT_TARGET`, Qt 6.12's own minimum), and run on Apple
  silicon only.
- **No icons for Windows or macOS** yet. The Linux icons are placeholders (a plain tile with a glyph).
- Tracy profiling is never in a release: the client opens a network listener, and the presets leave it off.

## Versions and tags

A version is Semantic Versioning. The tag is `v` and the version: `v0.1.0`, `v0.2.0-rc.1`. The numbers
in front of any suffix are the project version in `CMakeLists.txt` (`project(... VERSION x.y.z)`), which
`vcpkg.json` repeats; the programs report it, and the packages are named for the whole version. A tag
with a suffix, and any `0.x` version, is published as a **pre-release** (the first release,
`v0.1.0`, is one). The workflow refuses a tag whose version, commit or changelog does not match, and says why:

| The workflow checks | Because |
| --- | --- |
| the tag is `vMAJOR.MINOR.PATCH[-PRERELEASE]` and MAJOR.MINOR.PATCH is the project version | the packages and the programs must say what the tag says |
| `vcpkg.json` has the same version | they are changed together |
| the tagged commit is an ancestor of `origin/main` | gitflow: releases are merged to `main`, which is tagged |
| `CHANGELOG.md` has a section for the version, and `changelog.d` holds no fragment | the release branch assembled the changelog |

## Cutting a release

The branch model is gitflow ([Branch protection](branch-protection.md)): `release/*` branches from
`develop`, merged to `main` by a pull request with a merge commit, and back into `develop`.

1. **Branch.** `git switch -c release/0.1.0 develop`
2. **Version.** Set the version in `CMakeLists.txt` (`project(ArincDataLoader VERSION 0.1.0 ...)`) and in
   `vcpkg.json`. (The applications take theirs from CMake.) Nothing else carries it.
   For the first release also change the two places that say there is no package yet:
   `README.md` ("no installer yet") and `docs/user-guide/installing.md` ("no installer or download
   package yet"), to point at the release's packages and [Verifying a release](verifying-releases.md).
3. **Changelog.** Write `changelog.d/summary.md` if the release needs an introduction, then

   ```sh
   pwsh ./tools/release/Changelog.ps1 -Mode Assemble -Version 0.1.0      # -WhatIf to look first
   ```

   It writes the dated section of `CHANGELOG.md` from the fragments in the Keep a Changelog
   categories, updates the comparison links and deletes the fragments. Read the result and edit it as
   prose if it needs it. Commit.
4. **Push the branch.** A push to `release/*` runs the whole workflow as a **rehearsal**: it builds the
   documentation strictly, builds, tests, packages, writes the SBOM, signs with the real key, attests and verifies, and publishes
   nothing. Download the `release-signed` artifact from the run, check it
   ([Verifying a release](verifying-releases.md); `tools/release/verify_release.sh`), and try the packages
   on a clean machine. Fix on the branch and push again.
5. **Pull request to `main`**, from the `release/*` branch, merged with a merge commit once the required
   checks pass. (The changelog check does not apply to it.) The merge publishes the documentation site
   ([Documentation site](ci.md#documentation-site)), so build the site on the release branch (`mkdocs serve`)
   and read the pages that changed first: the site is the documentation of the latest release. (The
   *Docs build (strict)* run of a pull request checks that the site builds; it does not keep the site.)
6. **Tag.** On the merge commit of `main`:

   ```sh
   git switch main && git pull
   git tag -a v0.1.0 -m "ARINC Data Loader 0.1.0"
   git push origin v0.1.0
   ```

7. **Approve.** The run is the same as the rehearsal, then waits for you at *Draft the release*
   (the `release` environment asks for your approval). Approve it when the *Verify* job is green. The
   workflow creates a **draft** release with the notes from `CHANGELOG.md` and every file above.
8. **Publish the draft** on the Releases page after reading it. Pre-release status is already set.
9. **Back-merge.** Merge `release/0.1.0` (or `main`) into `develop` with a pull request, so the
   assembled changelog and the version reach `develop`.

A hotfix is the same from a `hotfix/*` branch off `main`, with its own fragment, assembled the same way.

### The dry run

Nothing needs to be tagged to see all of it work:

- push a branch named `release/<anything>` (the rehearsal of step 4);
- or, once the workflow is on `main`, `gh workflow run release.yml --ref release/<name>` (the `sign`
  input turns signing and attestation off);
- or open a pull request that touches the packaging, which builds, tests, packages and writes the SBOM
  on the three systems but has no secrets and signs nothing.

A rehearsal's artifacts are kept for 30 days on the run. Its attestations are real, and public as any
attestation of a public repository is, but they name a branch run and not a tag. GitHub starts a
`workflow_dispatch` only for a workflow that exists on the default branch, so `gh workflow run release.yml`
works only once a release has put `release.yml` on `main`; the branch push works from the start.

## One-time setup (done once per repository)

| What | Where | Setting |
| --- | --- | --- |
| Environment `release-signing` | Settings, Environments | Deployment branches and tags: `release/*` (branch) and `v*` (tag) only. Holds the two secrets. No reviewer: the rehearsals sign |
| Secret `RELEASE_GPG_PRIVATE_KEY` | in `release-signing` | The armoured secret key (`gpg --armor --export-secret-keys <fingerprint>`) |
| Secret `RELEASE_GPG_PASSPHRASE` | in `release-signing` | Its passphrase |
| Environment `release` | Settings, Environments | Required reviewer: the maintainer. Deployment tags: `v*` only. Holds no secrets; it gates *Draft the release* |
| Label `no-changelog` | Issues, Labels | for the changelog check |
| Pages | Settings, Pages | Source: *GitHub Actions* |
| Environment `github-pages` | Settings, Environments | Deployment branches: `main` only. No secrets, no reviewer |
| Variable `PUBLISH_DOCS` | Settings, Secrets and variables, Actions, Variables | `true`. Until it is set, `docs.yml` builds the site on `main` but does not publish it |

`tools/repo/apply_release_environments.ps1` makes the two release environments (`release-signing` and `release`; `github-pages` is made by switching Pages on) and, with `-KeyFile` and
`-PassphraseFile`, stores the two secrets (a dry run unless `-Apply` is given; the key and passphrase are
read from files and go to `gh secret set` on its standard input). The label is `gh label create
no-changelog`.

Only the *Sign* job uses the key, and it can run only on a `release/*` branch or a `v*` tag (GitHub
refuses the environment to any other ref, so a pull request or a feature branch never receives the
secrets). A person who can push a `release/*` branch can change the workflow on it and so read the
secrets: that is anyone with write access. If that is too wide, add the maintainer as a required
reviewer on `release-signing` too, at the price of approving every rehearsal.

### The signing key

| | |
| --- | --- |
| Type | Ed25519, signing and certifying (no encryption subkey) |
| User ID | `ARINC Data Loader Releases <iain.chesworth@gmail.com>` (the address in the maintainer's git configuration) |
| Fingerprint | `C6B4 AD93 067E B501 F450  5BCF 3FF3 6825 5329 8A53` |
| Created / expires | 2026-10-19 / 2028-10-18 (two years) |
| Public key | [`release-signing-key.asc`](release-signing-key.asc), also in [SECURITY.md](../SECURITY.md), [CI](ci.md) and every release |
| Keyserver | not uploaded; whether to is the maintainer's decision |

The private key is passphrase-protected and exists in the repository's `release-signing` environment
(write-only: nobody, the maintainer included, can read a secret back) and in the maintainer's own
backup, which must be kept somewhere safe and **not** in the repository, with its revocation certificate.

**Before the key expires** (a month ahead is sensible), extend it:
`gpg --edit-key <fingerprint>`, `expire`, then export the public key again, commit
`docs/release-signing-key.asc`, and re-export the secret key into `RELEASE_GPG_PRIVATE_KEY`. A
downloader who has the old key needs the new public key to see the new expiry; signatures made earlier
stay valid.

**If the key may have been exposed**, revoke it (`gpg --import revocation-certificate.rev`, then
`gpg --armor --export <fingerprint>` into `docs/release-signing-key.asc`), delete the two secrets, and
make a new key the way this one was made, in a fresh keyring on a machine you trust:

```sh
export GNUPGHOME=$(mktemp -d)
gpg --quick-generate-key "ARINC Data Loader Releases <address>" ed25519 sign,cert 2y
gpg --armor --export <fingerprint> > docs/release-signing-key.asc
gpg --armor --export-secret-keys <fingerprint> | gh secret set RELEASE_GPG_PRIVATE_KEY --env release-signing
gh secret set RELEASE_GPG_PASSPHRASE --env release-signing
```

then update the fingerprint in `SECURITY.md`, [CI](ci.md), [Verifying a release](verifying-releases.md) and
this page, and say so in `SECURITY.md` and the next release's notes. Revoking a key does not retract the
releases it signed, so a compromise is also announced on the releases it touches.

## What the workflow trusts, and what it does not

- **Actions** are pinned to full commit SHAs at least a week old, with the version in a comment, and
  Dependabot moves them; no job uses `pull_request_target`.
- **Qt** comes from `download.qt.io`, every archive checked against a SHA-256 pinned in this repository
  before it is unpacked; the Python package that unpacks it is hash-locked ([CI](ci.md)). The kit is
  Qt's own build: this project does not rebuild Qt.
- **The SBOM** lists what the build reported, never a guess: the first-party programs and libraries,
  Qt (by reference to the kit's own SPDX documents), the typeface, the runtime files Qt's deployment
  tool placed in a package, the vcpkg packages used for the tests (Catch2: in no package), and the
  compiler, CMake and Ninja of each leg. Both documents are checked against their schemas with
  hash-locked validators.
- **The attestations and the signature are independent.** A compromised runner could produce a package
  and an honest-looking attestation of it; a compromised key could sign anything. Together they say the
  maintainer's key signed what the maintainer's workflow built.
- **Not reproducible.** Two builds of the same commit differ in bytes (timestamps, the toolchain
  images); the attestation says where a file came from, not that it can be rebuilt identically.

## When something fails

| It says | Look at |
| --- | --- |
| *the tag ... is not ...*, *no section for ...* | The `Prepare` job's message; fix on the release branch and move the tag only if nothing was published |
| A `Release` leg is red where the Debug legs are green | An optimised-build-only problem (a warning like `-Wnull-dereference`, a test that depends on the debug runtime); fix it on the branch |
| *Test the packages* fails | The program that did not stay up, and its messages; usually a Qt file the deploy step did not copy |
| *do not cover the archives* / *SHA-256 ... is not the pinned* | `install_qt.py`: Qt re-published an archive, or the version changed; re-lock deliberately ([CI](ci.md)) |
| *Sign*: *the signing key is X, not Y* | The secret does not hold the key in `docs/release-signing-key.asc` |
| *Verify* fails on an attestation | A new attestation can take a moment to be readable (it retries four times) |

To abandon a release after the tag was pushed and before anything was published: delete the draft, delete
the tag locally and on GitHub (`git push origin :refs/tags/v0.1.0`), fix, and tag again. Once a release is
published, it is not changed: a new patch version is.
