---
template: marketing.html
title: FAQ
title_tag: "FAQ: ARINC Data Loader"
description: Short answers about what ARINC Data Loader is, the standards it implements, what it has been tested against, the platforms, the editions and the licence.
layout: prose
---
# FAQ

Short answers. Each one links to the page that has more. The guide has [a longer list of questions](user-guide/faq.md).

## The product

**What is it?**
: A C++23 library for the ARINC 615A loading protocol and ARINC 665 media sets, the [ADL Library](products/library.md), and three tools built on it: the [Data Loader](products/data-loader.md), the [Media Set Generator](products/media-set-generator.md) and the [Mock Target](products/mock-target.md). [Products](products/index.md) shows how they fit.

**Which standards, and which editions of them?**
: ARINC Report 665-3, *Loadable Software Standards* (published 12 August 2005), for media sets. ARINC Report 615A-3, *Software Data Loader Using Ethernet Interface* (published 30 June 2007), for the loading protocol. TFTP follows RFC 1350 with the options of RFCs 2347 to 2349. The code cites the standards by section. These are the editions it is written from. See [Where the standards go](specs/README.md).

**Do I need the standards to use it?**
: No. The guide explains the words you will meet ([Glossary](glossary.md)). The standards are the publisher's documents and are not in this repository.

**Is it independent of the people who publish the standards?**
: Yes. ARINC is the name of the standards' publisher. This project was written from the standards by Iain Chesworth Labs and is not affiliated with the publisher. <!-- OWNER DECISION: confirm the wording about the name ARINC, and whether a trademark line should be added. -->

**Does it include software for the target?**
: No. ADL is the loader's side of ARINC 615A. The Mock Target stands in for a target on the bench and in tests.

## Testing and evidence

**What has it been tested against?**
: The project's own test targets: a simulated target on a fake platform, and the Mock Target over real sockets, on Windows, Linux and macOS, and the engine on FreeRTOS and Zephyr simulators. The test target's file transfer is a separate implementation, and the files the library writes are compared with a second implementation. Testing against target equipment and other vendors' tools is next on the roadmap. [What has been checked](status.md) has the detail.

**Can I use it in a certification project?**
: Your project decides whether a tool needs qualification, and at what level. The [qualification kit](qualification-kit.md) of the Enterprise edition is being prepared as the evidence for that. [Assurance](assurance.md) says how the tools are verified.

## Using it

**Can I try it with no hardware?**
: Yes. Make a media set with the generator, start the Mock Target, and load it from the Data Loader, all on one computer. [How to use](how-to-use.md) has the three steps.

**Which systems does it run on?**
: The tools run on Windows (x86-64), Linux (x86-64) and macOS (Apple silicon, 14.4 or later). The library runs there too, and on FreeRTOS and Zephyr as experimental ports. [Platforms](platforms.md) lists every port and what has run on it.

**Why does FIND find nothing?**
: FIND is a broadcast on the local network. It does not reach a target on the same computer, does not cross routers, and cannot work on a network with no broadcast. A firewall can block it. Type the target's address instead. See [Troubleshooting](user-guide/troubleshooting.md#find-finds-nothing).

**How do I check a download?**
: Every release carries checksums, a signature for every file, and a build attestation. [Verifying a release](verifying-releases.md) says how.

## Editions and licence

**What does each edition include?**
: Community is the bench: free, with the programs, the libraries and the source. Pro is the rig: support, supported versions, and tools for loading many targets unattended and keeping the record. Enterprise is the programme: a site licence, the qualification kit and platform work. See [Pricing](pricing.md).

**What is the licence?**
: The GNU General Public License, version 3 or later, in every edition. See [Licensing](editions.md).

**What does it send on the network?**
: See [Privacy](privacy.md).

**Where do I report a problem?**
: See [Support](support.md). A security problem is reported privately.
