---
template: marketing.html
title: Licensing
title_tag: "Licensing: ARINC Data Loader"
description: Every edition of ARINC Data Loader is free software under the GNU General Public License, version 3 or later. What that gives you, what the paid editions add, and the licences of the parts it carries.
layout: prose
---
<!-- OWNER DECISION: the paid editions' terms (seats, support, supported versions, updates, warranty and liability) are not decided.
     docs/plan/editions.md and docs/plan/licensing.md hold the open questions. -->

# Licensing

Every edition of ARINC Data Loader is free software under the GNU General Public License, version 3 or later. The paid editions add support and evidence. They do not change the licence of the code, and they take nothing away from it. [Pricing](pricing.md) compares the editions.

## What the licence gives you

- You may use, study, change and share the programs and the libraries. The licence text is in the file `LICENSE`, in the repository and in every package.
- If you distribute a program that includes the libraries, or a changed version of them, the licence's terms apply to what you distribute: among them, that you offer its source under the same licence. Take your own advice for your case.
- The programs and the libraries come with no warranty under the licence. Each program's About dialog says so.

## The editions

| Edition | What it adds |
| --- | --- |
| Community | The programs, the libraries and the source, under the licence. Help from the community on GitHub. Fixes go into the latest release; there is no guaranteed response time. |
| Pro | For the rig. Support with a private channel and response targets, supported versions with security fixes, an update policy, and the Pro tools (rig automation, load records, a target lab, a media set pipeline; in development). Licensed per seat, for a year. |
| Enterprise | For the programme. Everything in Pro for a whole site, the [qualification kit](qualification-kit.md) for each supported version, a conformance kit, and platform and target-side work by agreement. A site licence, on agreed terms. |

The licence file of a paid edition is a record of what was bought. The Community programs do the same with or without one: nothing in them is locked or hidden. The Pro tools are programs of their own, in a Pro package, and each checks the licence itself.

## The parts it carries

The programs carry Qt 6.12, linked dynamically under the GNU Lesser General Public License, version 3, and the Archivo and Noto Sans Hebrew typefaces under the SIL Open Font License, version 1.1. Each is listed with its terms in About, then Licences, in each program, and in `NOTICES.txt` in every package.

## The standards

ARINC 665 and ARINC 615A are the publisher's documents. They are not under this licence and are not in the repository, and no edition contains their text. ARINC is the name of the standards' publisher; this project is not affiliated with it. <!-- OWNER DECISION: confirm the wording about the name ARINC, and whether a trademark line should be added. --> [Where the standards go](specs/README.md) says how to get them.

## Questions

[Talk to us](support.md#talk-to-us) about the paid editions. For the licence itself, the [FAQ](faq.md) has short answers.
