# Verifying a release

*For people who download ARINC Data Loader. How the release is built and signed is
[Releasing](releasing.md).*

Every GitHub release carries, beside the packages:

- `SHA256SUMS`, the SHA-256 of every other file in the release;
- a detached PGP signature `<file>.asc` for **every** file, `SHA256SUMS` included, made with the
  project's release key;
- a GitHub **build-provenance attestation** for every file, which says that the file was built by the
  workflow `.github/workflows/release.yml` of this repository, from this commit, on GitHub's runners;
- the software bill of materials, `arinc-data-loader-<version>.spdx.json` (SPDX 2.3) and
  `arinc-data-loader-<version>.cdx.json` (CycloneDX 1.6), with the SPDX documents of the Qt modules it
  refers to.

Each check answers a different question, so use the ones you can:

| Check | What it shows | What it does not |
| --- | --- | --- |
| SHA-256 | The file you have is the file that was listed | Who listed it |
| PGP signature | The holder of the release key signed exactly this file | That the build was sound |
| Attestation | GitHub's runner built this file from this repository's workflow at that commit | Anything about the key |

## The release key

| | |
| --- | --- |
| User ID | `ARINC Data Loader Releases <iain.chesworth@gmail.com>` |
| Type | Ed25519 (EdDSA, curve 25519), for signing and certifying |
| Fingerprint | `C6B4 AD93 067E B501 F450  5BCF 3FF3 6825 5329 8A53` |
| Long key ID | `3FF3682553298A53` |
| Created | 2026-10-19 |
| Expires | 2028-10-18 |

The public key is [`docs/release-signing-key.asc`](release-signing-key.asc) in this repository, and
the same file is in each release as `arinc-data-loader-release-key.pgp`. It is **not** on a
keyserver. Do not trust a key because it came with the download: compare the fingerprint above with
the one in [SECURITY.md](../SECURITY.md) and in [CI](ci.md), which are separate files, and with the
repository on GitHub as you see it.

## 1. Check the signatures

You need [GnuPG](https://gnupg.org/) (`gpg`). Download the release's files into one directory, then:

```sh
gpg --import arinc-data-loader-release-key.pgp     # or docs/release-signing-key.asc from the repository
gpg --fingerprint 3FF3682553298A53       # compare with the table above, character by character

gpg --verify SHA256SUMS.asc SHA256SUMS
gpg --verify arinc-data-loader-0.1.0-linux-x86_64.tar.gz.asc arinc-data-loader-0.1.0-linux-x86_64.tar.gz
```

A good signature says `Good signature from "ARINC Data Loader Releases <...>"`. gpg also prints
`WARNING: This key is not certified with a trusted signature!` unless you have signed the key
yourself. That is expected: the fingerprint comparison above is the check.

## 2. Check the checksums

```sh
sha256sum --check --ignore-missing SHA256SUMS     # Linux; --ignore-missing if you fetched only some files
shasum -a 256 --check SHA256SUMS                  # macOS
```

```powershell
# Windows: compare by eye with the line in SHA256SUMS
Get-FileHash -Algorithm SHA256 .\arinc-data-loader-0.1.0-windows-x86_64.zip
```

Signing `SHA256SUMS` signs every file in it, so a good signature on `SHA256SUMS` plus a matching sum
is as good as a signature on the file.

## 3. Check the attestation

With the [GitHub CLI](https://cli.github.com/) (`gh`), after `gh auth login`:

```sh
gh attestation verify arinc-data-loader-0.1.0-linux-x86_64.tar.gz \
    --repo iainchesworthlabs/arinc-data-loader \
    --signer-workflow iainchesworthlabs/arinc-data-loader/.github/workflows/release.yml
```

It prints the commit and the workflow run the file came from. `--source-ref refs/tags/v0.1.0` also
pins the tag. Without `gh`, the attestation is in the release's
[attestations page](https://github.com/iainchesworthlabs/arinc-data-loader/attestations).

The SPDX SBOM is attested as a statement about the packages: `gh attestation verify <package> --repo
iainchesworthlabs/arinc-data-loader --predicate-type https://spdx.dev/Document/v2.3`.

## All of it at once

[`tools/release/verify_release.sh`](../tools/release/verify_release.sh) does steps 1 to 3 on a
directory of downloaded files (it needs `gpg`, `sha256sum` and `gh`), and is what the release
workflow runs on its own output before it drafts the release:

```sh
tools/release/verify_release.sh ./downloaded-release docs/release-signing-key.asc
```

## What the packages are, and what they are not

- **The packages are not code-signed for an operating system.** There is no Authenticode signature on
  Windows and no Developer ID signature or notarisation on macOS; the PGP signature above is the
  project's own. Expect the system to warn the first time:
  - *Windows* (SmartScreen): "Windows protected your PC": choose **More info**, then **Run anyway**,
    after you have verified the zip.
  - *macOS* (Gatekeeper): the bundles are signed ad hoc (which an Apple-silicon Mac needs to run them
    at all), not by an identified developer. After verifying the disk image, open the application
    with a right click, **Open**, or allow it under *System Settings, Privacy & Security*. A file
    downloaded with a browser carries a quarantine flag; `xattr -dr com.apple.quarantine
    "ARINC Data Loader.app"` removes it from a copy you have verified.
  - *Linux*: nothing asks; the tar.gz unpacks into a directory of its own.
- **Qt is in the packages as separate shared libraries**, under the LGPL, as the
  [notices](../notices/README.md) in `NOTICES.txt` (inside every package, and in each program under
  About, Licences) say. You can replace them with another build of the same Qt version.
- **Reproducibility.** The builds are made on GitHub's hosted runners from a pinned toolchain
  ([CI](ci.md), "What CI pins") but are not bit-for-bit reproducible: the attestation tells you
  where a file came from, not that you would get the same bytes.

## If a check fails

Do not run the file. A mismatch can be a damaged download; download it again and compare. A bad
signature on a file whose sum matches `SHA256SUMS` means that `SHA256SUMS` or the signature is not
what the project published. In either case, report it privately, as [SECURITY.md](../SECURITY.md)
describes. If the release key itself is ever revoked or replaced, the announcement is in `SECURITY.md`
and in the release notes of the first release signed with the new one.
