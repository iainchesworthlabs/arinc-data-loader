**A release process.** `.github/workflows/release.yml` builds the Release configuration on Windows, Linux
and macOS, runs every test tier against it, makes the packages, writes a software bill of materials (SPDX
2.3 and CycloneDX 1.6, with the Qt kit's own SPDX documents), computes SHA-256 sums, signs every file
with a dedicated PGP release key, attaches GitHub build-provenance attestations, checks all of it as a
downloader would, and drafts a GitHub release from `CHANGELOG.md` for the maintainer to publish. Pushing a
`release/*` branch rehearses all of it without publishing. [docs/releasing.md](docs/releasing.md) is the
process and [docs/verifying-releases.md](docs/verifying-releases.md) is how to check a download; the public
key and its fingerprint are in `docs/release-signing-key.asc` and `SECURITY.md`.
