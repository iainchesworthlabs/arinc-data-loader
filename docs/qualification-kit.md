---
template: marketing.html
title: Qualification kit
title_tag: "Qualification kit: ARINC Data Loader for DO-178C projects"
description: The qualification kit of the Enterprise edition is the evidence a DO-178C project needs to assess ARINC Data Loader as a tool under DO-330, for each supported version.
---
<!-- OWNER DECISION: the kit is planned (docs/plan/assurance-data-pack.md) and not built. Its contents, the levels it is made for, the
     price, how it is delivered, who reviews and approves it and what independence is offered are not decided. This page never claims certification, qualification, compliance or approval of the tool: the project that uses it qualifies it,
     for its own use. -->
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Enterprise edition</p>
<h1>The qualification kit</h1>
<p class="icl-lead">The evidence a DO-178C project needs to assess ARINC Data Loader as a tool under DO-330, put together for each supported version and laid out the way DO-330 names its data.</p>
<ul class="icl-tags"><li>Enterprise edition</li><li>For each supported version</li><li>In preparation</li></ul>
<div class="icl-actions"><a class="icl-button" href="../support/#talk-to-us">Talk to us about the kit</a><a class="icl-button icl-button--ghost" href="../pricing/">Compare editions</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>Why a project needs it</h2>
<p>DO-178C asks a project to consider every tool that could put an error into airborne software, or fail to detect one, within its intended use. Whether the tool needs qualification, and to which tool qualification level, depends on how the project uses it and on the software level of what it touches. The evidence for that qualification is about the tool, and the tool's maker is the one who has it.</p>
</div>
<ul class="icl-points">
<li><h3>The Media Set Generator</h3><p>Writes the load package: the header file, the lists and the check values. An error in what it writes would be an error in the package.</p></li>
<li><h3>The Data Loader</h3><p>Checks a media set before it sends it, and moves the files of a load to the target. A project that relies on that check, in place of its own, relies on the tool to detect errors.</p></li>
<li><h3>The library</h3><p>When you build it into your own loader or tool, the kit's evidence for the library goes into your own tool's qualification.</p></li>
</ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>What is in the kit</h2>
<p>One kit for each version, as one signed folder. The items follow the data DO-330 names; what each holds depends on the level your project needs.</p>
</div>
<div class="adl-table"><table>
<thead><tr><th scope="col">Item</th><th scope="col">What it holds</th><th scope="col">Made from</th></tr></thead>
<tbody>
<tr><th scope="row">Intended use and operating environment</th><td><p>What each program and library function is for, the platforms and versions, the inputs and outputs, and what is out of scope, written so a project can adopt or narrow it.</p></td><td><p>The user guide, the platform matrix, the limits</p></td></tr>
<tr><th scope="row">Error analysis</th><td><p>For each function, how it could insert or miss an error, what would detect it, and the evidence.</p></td><td><p>The libraries' own checks: read-back, CRCs and check values</p></td></tr>
<tr><th scope="row">Tool qualification plan</th><td><p>A plan for the qualification at your level: what is claimed, the process, and who does what. A template your project completes.</p></td><td><p>Written for the kit</p></td></tr>
<tr><th scope="row">Tool operational requirements</th><td><p>What the tool shall do when used as intended, numbered and each one testable.</p></td><td><p>Written for the kit, from the coverage pages</p></td></tr>
<tr><th scope="row">Verification cases and procedures</th><td><p>The tests, how each runs and what it shows, each linked to a requirement.</p></td><td><p>The test suite, the golden files, the fuzz harnesses</p></td></tr>
<tr><th scope="row">Verification results</th><td><p>The results for the kit's version, with the build that made them, and the coverage.</p></td><td><p>The release build's test runs</p></td></tr>
<tr><th scope="row">Configuration index</th><td><p>The baseline: the commit and tag, the hash of every file, the compiler and tool versions, the bill of materials and the attestations.</p></td><td><p>The release records</p></td></tr>
<tr><th scope="row">Problem reports</th><td><p>The open and closed problems of the version, each classified by its effect.</p></td><td><p>The issue tracker</p></td></tr>
<tr><th scope="row">Quality assurance records</th><td><p>The reviews of the version, and who did them.</p></td><td><p>Review records</p></td></tr>
<tr><th scope="row">Accomplishment summary</th><td><p>What was done, what was found and what remains open at the version.</p></td><td><p>Written for the kit</p></td></tr>
</tbody></table></div>
<p class="icl-muted"><!-- OWNER DECISION: the levels the kit is made for. -->The levels the kit is made for will be stated with the first kit.</p>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>Built from the records the project already keeps</h2>
<p>Much of a kit comes from what every release has already: the tests and their results, the coverage pages that set each section of the standards against its tests, the signed and attested release files, the bill of materials and the change history. A generator will put them into the kit for a version, so the evidence matches the code it describes.</p>
<ul class="adl-ticks">
<li>More than 550 test cases, golden files from a second implementation, and 16 fuzz harnesses.</li>
<li>Coverage held at 90 percent of lines and 80 percent of branches.</li>
<li>Releases signed, with checksums, attestations and a bill of materials.</li>
</ul>
<ul class="adl-links"><li><a href="../assurance/">How it is verified</a></li></ul>
</div>
<div>
<h2>Who does what</h2>
<p><strong>We supply</strong> the evidence about the tool, for the version you use.</p>
<p><strong>Your project</strong> decides where the tool is used in its plan, the software level of what it touches and so the tool qualification level, and whether to qualify at all. It verifies the tool in its own environment, keeps its own checks that do not rely on the tool, and deals with its authority.</p>
<p>Tool qualification belongs to a project and its own use of the tool, and the project's authority accepts it. The kit is evidence for that; it does not replace your plan, your reviews or your authority's acceptance.</p>
</div>
</div>
</div></section>

<section class="icl-section icl-cta"><div class="icl-wrap">
<h2>Planning a programme that uses these tools?</h2>
<p class="icl-lead">Tell us the software level, which tools you use and how, and when you need the evidence.</p>
<div class="icl-actions"><a class="icl-button" href="../support/#talk-to-us">Talk to us</a><a href="../pricing/">Enterprise</a></div>
</div></section>
