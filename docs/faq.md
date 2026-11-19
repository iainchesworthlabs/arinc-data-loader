---
template: marketing.html
title: FAQ
title_tag: "FAQ: ARINC Data Loader"
description: Short answers about what ARINC Data Loader is, what it has been checked against, what it sends on the network and how it is licensed.
layout: prose
---
# FAQ

Short answers. Each one links to the page that has more. The guide has [a longer list of questions](user-guide/faq.md).

## What it is

**What does it do?**
: It implements two standards for loading software into aircraft equipment. A data loader (`adl_app`) finds a target on an Ethernet network, reads what it holds, sends a load to it and fetches files from it. A generator (`adl_media_gen`) makes the media sets the loader sends. A mock target (`adl_mock_target`) stands in for equipment. See [Features](features.md).

**Which standards, and which editions of them?**
: ARINC Report 665-3, *Loadable Software Standards* (published 12 August 2005), for media sets. ARINC Report 615A-3, *Software Data Loader Using Ethernet Interface* (published 30 June 2007), for the loading protocol. TFTP follows RFC 1350 with the options of RFCs 2347 to 2349. The code cites the standards by section. These are the editions it is written from, and the project makes no claim about any other edition. See [Where the standards go](specs/README.md).

**Do I need the standards to use it?**
: No. The guide explains the words you will meet ([Glossary](glossary.md)). The standards are the publisher's documents and are not in this repository.

**Is it independent of the people who publish the standards?**
: Yes. ARINC is the name of the standards' publisher. This project was written from the standards by Iain Chesworth Labs and is not affiliated with the publisher. <!-- OWNER DECISION: confirm the wording about the name ARINC, and whether a trademark line should be added. -->

## What has been checked

**Has it been used with real equipment?**
: No. Nothing has been checked against a real target, another vendor's loader, or media sets from another tool. The tests use a simulated target and the mock target, and both ends of those tests are built from the same reading of the standards. See [What has been checked](status.md).

**Is there a program that runs on the equipment?**
: No. The libraries build for a target's platform, but nothing answers a loader and writes status files on real equipment.

## Using it

**Can I try it with no hardware?**
: Yes. Make a media set with the generator, start the mock target, and load it from the loader, all on one computer. [How to use](how-to-use.md) has the three steps.

**Which systems does it run on?**
: Windows (x86-64), Linux (x86-64) and macOS (Apple silicon, 14.4 or later). Windows is the system it has been used on by hand. There is no installer: each release is a portable package.

**Why does FIND find nothing?**
: FIND is a broadcast on the local network. It does not reach a target on the same computer, does not cross routers, and cannot work on a network with no broadcast. A firewall can block it. Type the target's address instead. See [Troubleshooting](user-guide/troubleshooting.md#find-finds-nothing).

**How do I check a download?**
: Every release carries checksums, a signature for every file, and a build attestation. [Verifying a release](verifying-releases.md) says how.

## Data and licence

**What does it send on the network?**
: See [Privacy](privacy.md).

**What is the licence?**
: The GNU General Public License, version 3 or later, with no warranty. See [Licensing and editions](editions.md).

**Can I rely on it in certification work?**
: Not on its own. See [Assurance](assurance.md).

**Where do I report a problem?**
: See [Support](support.md). A security problem is reported privately.
