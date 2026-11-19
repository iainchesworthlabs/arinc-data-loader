---
template: marketing.html
title: Release notes
title_tag: "Release notes: ARINC Data Loader"
description: What changed in each release of ARINC Data Loader, from the changelog.
layout: prose
---
# Release notes

What changed in each release, in short. The dates are those in the [changelog](../CHANGELOG.md), which has the full list. The packages, their checksums and their signatures are on the [releases page on GitHub](https://github.com/iainchesworthlabs/arinc-data-loader/releases). [Downloads](downloads.md) says which package to choose.

## 0.1.2 (19 November 2026)

A maintenance release. The programs and the libraries are unchanged, and so is how they load and read data.

- The documentation is now part of the company site, at {{ site_url }}. The addresses of the first site forward to the new ones.
- The guide's first pages are rewritten, with new reference pages for every message the programs show, the findings of the media set check and the files the programs read and write.
- The site describes the library, the tools, the platforms they run on and the editions. Its Support page has a form that sends a question to the maintainer privately.

## 0.1.1 (24 October 2026)

A maintenance release. How the programs load and read data is unchanged.

- Simplified Chinese is now `zh_CN`, the name of its catalogue and of the choice kept in the settings. A choice of Chinese saved by 0.1.0 still selects it. A computer set to Traditional Chinese now starts the programs in English, as it does for any language without a catalogue.
- A release carries `SHA512SUMS` in place of `SHA256SUMS`, and every file is signed with the organisation's release key, a different key from 0.1.0's. [Verifying a release](verifying-releases.md) has the fingerprint and the checks.
- The source tree moved to `libs/` and `apps/`, and the parsers are fuzzed and benchmarked. None of that changes what is in a package.

## 0.1.0 (22 October 2026)

The first release. It rebuilds an earlier Visual Studio project as a C++23 CMake project.

- The data loader, the media set generator and the mock target, for Windows, Linux and macOS, in eight languages.
- The libraries for ARINC 665-3 media sets and the ARINC 615A-3 protocol, and the platform layer under them.
- A portable package for each system, with checksums, signatures, build attestations and a bill of materials.
- A user's guide, with a ten-minute demonstration on one machine.

The 0.1.0 entry in the changelog lists the known limitations, and [What has been checked](status.md) says what each part has been tested against.

## What comes next

[The roadmap](roadmap.md) says what is planned. It is not a promise of dates.
