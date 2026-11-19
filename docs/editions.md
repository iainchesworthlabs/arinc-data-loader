---
template: marketing.html
title: Licensing and editions
title_tag: "Licensing and editions: ARINC Data Loader"
description: ARINC Data Loader is free software under the GNU General Public License, version 3 or later. This page says what that gives you, and what is proposed for paid editions.
layout: prose
---
<!-- OWNER DECISION: this page is a draft from docs/plan/editions.md. Do not publish it before the decisions marked below are made, or before you have agreed that the proposed editions may be shown. -->

# Licensing and editions

ARINC Data Loader is free software. This page says what the licence gives you and what it does not, and sets out the paid editions that are proposed.

## Community

The Community edition is the program as it is in the repository and in every release.

- You may use, study, change and share the programs and the libraries under the GNU General Public License, version 3 or later. The licence text is in the file `LICENSE`, in the repository and in every package.
- It comes with no warranty. Each program's About dialog says so.
- It comes with no assurances. It has not been checked against real equipment, and nothing about it is promised for any purpose. [Assurance](assurance.md) says what exists and what does not.
- It comes with no support. The project is maintained by one person, so there is no guaranteed response time and no promised schedule for a fix. Until version 1.0, only the latest release receives fixes. [Support](support.md) says how to report a problem.

The programs carry Qt 6.12, linked dynamically under the GNU Lesser General Public License, version 3, and the Archivo and Noto Sans Hebrew typefaces under the SIL Open Font License, version 1.1. Each is listed with its terms in About, then Licences, in each program, and in `NOTICES.txt` in every package. The ARINC standards are not under this licence and are not in the repository.

## Paid editions: proposed

Paid editions are proposed. They are not offered yet. Nothing on this page is a price or a promise.

| Edition | What it adds to Community |
| --- | --- |
| Pro and Site | Support, with a stated way to reach it. A list of supported versions. A policy for updates. |
| Enterprise | Everything in Pro and Site, and data packs: documents and records for people who assess ARINC Data Loader for use as a test tool in aerospace certification work. [Assurance](assurance.md) says what a data pack is for and what it is not. |

<!-- OWNER DECISION: the names of the editions; whether Pro and Site are one edition or two, and what a site is; prices and whether any are shown; the support channel, hours and response times; the supported versions and the update policy; the warranty and liability wording; who may redistribute a data pack; how long evidence is kept. -->

The open questions are written down in the project's planning notes, and the decisions are the owner's.

## What does not change

- Everything in the Community edition stays available under the GNU General Public License. A paid edition does not take that away, and does not stop anyone sharing the code under that licence.
- <!-- OWNER DECISION: whether the code is also offered under another licence. That needs the owner to hold the copyright of all of it, and the Qt and typeface licences above still apply. -->

## Questions

Open an issue on [GitHub](https://github.com/iainchesworthlabs/arinc-data-loader/issues), or read [Support](support.md). There is no email address for this yet.
