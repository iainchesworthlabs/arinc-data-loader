---
template: marketing.html
title: Assurance
title_tag: "Assurance: ARINC Data Loader"
description: What evidence exists about ARINC Data Loader as a tool, what the Community edition does not give, and what data packs are for.
layout: prose
---
<!-- OWNER DECISION: this page is a draft from docs/plan/editions.md. It makes no claim of certification, qualification, compliance or approval and must not be changed to make one unless the owner supplies the evidence. -->

# Assurance

This page says what evidence exists about ARINC Data Loader itself, what the Community edition does not give you, and what the data packs of the proposed Enterprise edition are for. It describes the tool. It says nothing about any aircraft or any programme.

## Where it stands

- This is a preliminary release. Nothing has been checked against a real target, another vendor's loader, or media sets from another tool.
- The project's own tests check that the code does what the project's reading of the two standards says. They cannot show that the reading is right. [What has been checked](status.md) sets out eight levels of evidence, and says which have been reached for each part: the first six are checks the project made itself, and the last two, real equipment and another tool, have not been done.
- The code is tested on Windows, Linux and macOS in continuous integration. The coverage of the portable code is held at 90 percent of lines and 80 percent of branches. The code that reads bytes from outside has sixteen fuzz harnesses. Static analysis runs every night. Releases are signed and come with a bill of materials.
- The places where the standards are unclear were settled by the project's own choices, and each is listed in the coverage pages ([ARINC 665-3](arinc665-coverage.md), [ARINC 615A-3](arinc615a-coverage.md)).

## What the Community edition does not give

The Community edition is free software with no warranty and no support. In particular it does not give you:

- any assurance that the tool finds every error in a load or a media set, or that it leaves what it loads unchanged;
- a statement of the tool's intended use and operating environment, beyond what the guide says;
- requirements for the tool, with the tests and results that show each one is met;
- records of reviews, of problems found and fixed, or of how each release was built and checked, kept as evidence;
- any review of the tool by someone independent of the person who wrote it;
- support, or a promise to keep a version supported.

## Safety of use

Do not rely on ARINC Data Loader to find an error in software or data that will be loaded into equipment in service, or to leave that software or data unchanged, unless your own programme has assessed the tool for that use. Check what you load by a method you trust, as well.

## What the Enterprise data packs are for

A data pack is proposed for the Enterprise edition. It is a set of documents and records that a project can use when it assesses ARINC Data Loader as a tool in its own certification work. It supports your assessment. It does not make the tool certified or qualified, and it does not replace your own plan, your reviews, or your dealings with your authority. Your project decides how the tool is used in its plan, and what level of assessment that needs.

A pack is meant to hold, for one version of the tool:

- what the tool is for and where it is used;
- an analysis of whether the tool could fail to detect an error, or put one into software or data, in your use of it;
- the requirements for the tool's operation, and the tests and results for them;
- how the version was built and how it is controlled;
- the problems found in it and what was done.

<!-- OWNER DECISION: the contents, the price, how a pack is delivered, who reviews and approves it, and what independence is offered. -->

DO-178C and DO-330 are named on this page only because they are standards that your own programme may apply. This page makes no statement that the tool meets either of them.

## What your project adds

Whoever uses the tool in certification work owns that work: the plan that says where the tool is used, the assessment of what could go wrong, the reviews, and the dealings with the authority. A data pack supplies evidence about the tool, and nothing else.
