# Getting started

This page takes you from nothing to a first load on one computer, with no hardware. Each step links to the page that has the detail. The demonstration at the end takes about ten minutes, by the guide's own estimate.

## What you need

- A computer running Windows (x86-64), Linux (x86-64) or macOS 14.4 or later (Apple silicon).
- A few files to load. Any files will do: the programs do not look inside them.
- Nothing else. There is no installer, and the packages carry the Qt files they need.

!!! note
    This is a preliminary release. Nothing has been checked against real equipment. Read [Safety of use](safety-of-use.md) before you rely on the programs for anything that matters.

## Steps

1. **Get a package.** Download the package for your system from the [releases page on GitHub](https://github.com/iainchesworthlabs/arinc-data-loader/releases). The names are in [Installing and starting](installing.md#getting-the-programs).
2. **Check it** (optional, and worth doing if your system warns you about the file). [Verifying a release](../verifying-releases.md) says how to check the checksum, the signature and the build attestation. If a check fails, do not run the file.
3. **Unpack it and start a program.** On Windows, unpack the zip and double-click `adl_app.exe`. On Linux, unpack the archive and run `bin/adl_app`. On a Mac, open the disk image and drag the applications to `Applications`. The system may warn the first time, because the packages are not signed for any system's store.
4. **Let the firewall ask, or avoid it.** The programs talk over UDP. To keep the firewall from asking while you try them on one computer, tick **This computer only (127.0.0.1)** in the mock target ([What the firewall asks](installing.md#what-the-firewall-asks)).
5. **Follow the demonstration.** [The demonstration](demonstration.md) makes a media set, starts the mock target, and runs an upload, both kinds of download and a failure, with a picture of every screen.

## Where to go next

| I want to | Read |
| --- | --- |
| Know what the words mean | [What ARINC 615A and 665 are](concepts.md) and the [Glossary](../glossary.md) |
| Make a media set from my own files | [Make a media set](make-a-media-set.md) |
| Load a target on a network | [The loader, screen by screen](loader.md), then [Find a target](find-a-target.md) and [Upload](upload.md) |
| Look something up | [Quick reference](quick-reference.md) |
| Fix something | [Troubleshooting](troubleshooting.md) |
