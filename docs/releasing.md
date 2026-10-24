# Releasing

*For the maintainer. How a downloader checks a release is [Verifying a release](verifying-releases.md);
what CI runs on every change is [CI](ci.md).*

A release is a version tag on `develop`. Pushing the tag runs `.github/workflows/release.yml`, which
builds and tests the Release configuration on Windows, Linux and macOS, packages it and writes the SBOM.
The shared publish workflow of `icl-common-infra` then writes the checksums, signs every file with the
organisation's release key, attests them, runs `tools/release/verify_release.sh` over the result and
drafts a GitHub release. Nothing is published until the maintainer reads the draft and presses the button.

## What a release contains

| File | Holds |
| --- | --- |
| `arinc-data-loader-<v>-windows-x86_64[-unsigned|-testsigned].zip` | A portable folder: `adl_app.exe`, `adl_media_gen.exe`, `adl_mock_target.exe`, `adl_write_payloads.exe`, the Qt DLLs, plugins and QML modules Qt's `windeployqt` places beside them, `README.txt`, `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-<v>-linux-x86_64.tar.gz` | `bin/` (the four programs), `lib/` and `plugins/` and `qml/` (the Qt files), `share/applications/*.desktop`, `share/icons/hicolor/scalable/apps/*.svg`, `share/doc/arinc-data-loader/{LICENSE,NOTICES}.txt` |
| `arinc-data-loader-<v>-macos-arm64.dmg` | The three application bundles (Qt frameworks and QML modules inside, signed ad hoc, each with `LICENSE.txt` and `NOTICES.txt` in `Contents/Resources`), `tools/adl_write_payloads`, an `Applications` link to drag them onto, `README.txt`, `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-dev-<v>-<system>-<arch>.zip` / `.tar.gz` | The development package: headers, static libraries and the `adl` CMake package (below), `LICENSE.txt`, `NOTICES.txt` |
| `arinc-data-loader-<v>.spdx.json`, `.cdx.json` | The software bill of materials (SPDX 2.3, CycloneDX 1.6) |
| `<leg>-qt<module>-6.12.0.spdx.json`, `.cdx.json` | The Qt kit's own SBOM documents for the modules the packages hold; the SPDX document refers to them |
| `arinc-data-loader-release-key.pgp` | The public release key (the same as `docs/signing-key.asc`) |
| `SHA512SUMS`, and `<file>.asc` for every file | The checksums, and the detached PGP signatures |

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
applications (`apps/app`, `libs/ui`, ...) is written for those programs and for Qt. Before 1.0 the API is not
stable: the package's version file accepts the same minor version only (`SameMinorVersion`).

### What is not shipped, and why

- **No Windows installer, no `.deb`, no AppImage.** An installer that is not signed with a certificate
  the system trusts earns the same warning as the zip it wraps; a distribution package that carries its
  own Qt belongs to someone who keeps it patched for that distribution; an AppImage needs a build tool
  fetched at packaging time that this repository has no pin for. The tar.gz carries `.desktop` files
  and icons for anyone who wants to wrap it.
- **No trusted Authenticode signature, no Developer ID signature, no notarisation.** They need
  certificates the project does not have. The PGP signature and the attestation are what is offered
  instead. [Windows signing](#windows-signing) says what `SIGN_WINDOWS` does meanwhile.
- **Only the architectures CI builds:** Windows x86-64, Linux x86-64, macOS arm64. The macOS programs
  need macOS 14.4 or later (`CMAKE_OSX_DEPLOYMENT_TARGET`, Qt 6.12's own minimum), and run on Apple
  silicon only.
- **No icons for Windows or macOS** yet. The Linux icons are placeholders (a plain tile with a glyph).
- Tracy profiling is never in a release: the client opens a network listener, and the presets leave it off.

## Versions and tags

A version is Semantic Versioning. The tag is `v` and the version: `v0.1.1`, `v0.2.0-rc.1`. The numbers
in front of any suffix are the project version in `CMakeLists.txt` (`project(... VERSION x.y.z)`), which
`vcpkg.json` repeats; the programs report it, and the packages are named for the whole version. A
suffix makes a **pre-release**. While the project is 0.x, also tick *Set as a pre-release* on the draft
of a final release (the shared workflow sets it for a suffix only).

The workflow stops, and says why, when:

| The check | Because |
| --- | --- |
| the tag is `vMAJOR.MINOR.PATCH[-suffix]` and the commit is on `develop` | the packages and the programs must say what the tag says |
| `CMakeLists.txt` and `vcpkg.json` carry the version before the suffix | they are changed together |
| `CHANGELOG.md` has a `## [x.y.z]` section (a pre-release may use the fragments still in `changelog.d`) | the release has notes |
| `SIGN_WINDOWS` is `test` for a final release | test-signed files go to pre-releases only |

## Cutting a release

1. **Version and changelog, in one pull request to `develop`.** Set the version in `CMakeLists.txt`
   (`project(ArincDataLoader VERSION 0.1.1 ...)`) and in `vcpkg.json`. In prose, `SECURITY.md`'s table
   of supported versions and the `find_package(adl 0.1 ...)` example above name the minor version:
   change them when it changes. If the release needs an introduction, write it as `changelog.d/summary.md`
   (the assembly takes it into the section). Then assemble the fragments:

   ```sh
   python3 <icl-common-infra>/scripts/changelog_tool.py assemble --version 0.1.1 --date YYYY-MM-DD
   ```

   It writes the dated section of `CHANGELOG.md` and deletes the fragments. Read the result and edit
   it as prose if it needs it. A pre-release can skip this step: `changelog_tool.py notes --version 0.1.1
   --allow-pending` prints the notes it would get.
2. **Rehearse.** `gh workflow run release.yml --ref <branch>` builds, tests, packages and writes the
   SBOM on the three systems and publishes nothing (a pull request that touches the packaging does the
   same). Its packages are kept for 30 days on the run; try them on a clean machine.
3. **Tag** the merge commit, for a trial first (`v0.1.1-rc.1`), then the release:

   ```sh
   git tag -s v0.1.1 -m "ARINC Data Loader 0.1.1"
   git push origin v0.1.1
   ```

4. **Approve.** After the build the run waits at *Sign and publish* for your approval (the `release`
   environment). The job signs, checks the signatures against `docs/signing-key.asc`, runs
   `tools/release/verify_release.sh` over the signed files and drafts the release with the notes from
   `CHANGELOG.md`. For a final release it then moves `main` to the tag, which publishes the
   documentation site ([Documentation site](ci.md#documentation-site)): the site is the documentation of
   the latest release, so build it (`mkdocs serve`) and read the pages that changed before you approve.
5. **Publish the draft** on the Releases page after reading it.

A hotfix is the same, with its own fragment, tagged from `develop`.

Abandoning a release after the tag was pushed and before it was published: delete the draft, delete the
tag locally and on GitHub, fix, and tag again. Once a release is published, it is not changed: a new
patch version is.

## Setup

The repository settings, the `release` environment, the labels and the secrets come from
`icl-common-infra/admin/apply.sh` ([Repository settings](repository-settings.md)). Two more things
belong to this repository:

| What | Where | Setting |
| --- | --- | --- |
| Pages | Settings, Pages | Source: *GitHub Actions* |
| Variable `PUBLISH_DOCS` | Settings, Secrets and variables, Actions, Variables | `true`. Until it is set, `docs.yml` builds the site on `main` but does not publish it |

The secrets are the organisation's `REPO_GPG_PRIVATE_KEY` and `REPO_GPG_PASSPHRASE` (the signing key) and
`RELEASE_BOT_APP_ID` and `RELEASE_BOT_PRIVATE_KEY` (the bot that moves `main`). Only the shared publish
job reads them, in the `release` environment.

### The signing key

The key is the organisation's. Its fingerprint is `FD13 CB8C AC83 1664 8CD9  74D0 8149 254C 4189 4DBE`;
the public key is [`signing-key.asc`](signing-key.asc), also attached to every release, and the
organisation's `SECURITY.md` and [this repository's](../SECURITY.md) publish the fingerprint. Rotation and
revocation are the organisation's to run; when the key changes, replace `docs/signing-key.asc` and the
fingerprint in `SECURITY.md`, [Verifying a release](verifying-releases.md) and here. Releases up to
0.1.0 were signed with an earlier, repository-specific key, `C6B4 AD93 067E B501 F450  5BCF 3FF3 6825 5329 8A53`;
they carry `SHA256SUMS`, not `SHA512SUMS`.

### Windows signing

The repository variable `SIGN_WINDOWS` is `false` (the default), `test` or `production`. The Release
workflow signs the four programs in place before CPack packs them:

- `false`: nothing is signed; the Windows runtime package is named `...-windows-x86_64-unsigned.zip`.
- `test`: signed with the organisation's test certificate (`WIN_TESTSIGN_PFX_BASE64` and
  `WIN_TESTSIGN_PFX_PASSWORD`); the package is `...-testsigned.zip`. Windows trusts it only on a machine
  that has installed the test CA ([`docs/test-signing.md` in `icl-common-infra`](https://github.com/iainchesworthlabs/icl-common-infra/blob/develop/docs/test-signing.md)).
  A final release refuses it.
- `production`: not set up; the run fails until a signing service is chosen.

## What the workflow trusts, and what it does not

- **Actions** are pinned to full commit SHAs at least a week old, with the version in a comment, and
  Dependabot moves them; no job uses `pull_request_target`.
- **Qt** comes from `download.qt.io`, every archive checked against a SHA-256 pinned in this repository
  before it is unpacked; the Python package that unpacks it is hash-locked ([CI](ci.md)). The kit is
  Qt's own build: this project does not rebuild Qt.
- **The SBOM** lists what the build reported, never a guess: the first-party programs and libraries,
  Qt (by reference to the kit's own SPDX documents), the typefaces, the runtime files Qt's deployment
  tool placed in a package, the vcpkg packages used for the tests (Catch2: in no package), and the
  compiler, CMake and Ninja of each leg. Both documents are checked against their schemas with hash-locked validators, and the SPDX document is checked to list every typeface in `libs/ui/fonts` (`tools/release/check_sbom_fonts.py`).
- **The attestations and the signature are independent.** A compromised runner could produce a package
  and an honest-looking attestation of it; a compromised key could sign anything. Together they say the
  maintainer's key signed what the maintainer's workflow built.
- **Not reproducible.** Two builds of the same commit differ in bytes (timestamps, the toolchain
  images); the attestation says where a file came from, not that it can be rebuilt identically.

## When something fails

| It says | Look at |
| --- | --- |
| *releases x, but ... is y* | `CMakeLists.txt` or `vcpkg.json` has another version; change it in a pull request, or tag the version the project declares |
| *has no release notes* | Assemble the changelog in a pull request (step 1) and tag the merge commit |
| A `Release` leg is red where the Debug legs are green | An optimised-build-only problem (a warning like `-Wnull-dereference`, a test that depends on the debug runtime); fix it on `develop` |
| *Test the packages* fails | The program that did not stay up, and its messages; usually a Qt file the deploy step did not copy |
| *do not cover the archives* / *SHA-256 ... is not the pinned* | `install_qt.py`: Qt re-published an archive, or the version changed; re-lock deliberately ([CI](ci.md)) |
| *missing organisation secrets* | The organisation secrets are not visible to this repository |
| *verification failed* on an attestation | A new attestation can take a moment to be readable (the shared job retries four times) |
