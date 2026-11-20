---
template: marketing.html
title: Assurance
title_tag: "Assurance: how ARINC Data Loader is verified"
description: How ARINC Data Loader is verified, what evidence is kept with the code, how releases are signed, and what the qualification kit adds for certification projects.
---
<!-- OWNER DECISION: this page states what the repository shows. It makes no claim of certification, qualification, compliance or approval and must not be changed to make one unless the owner supplies the evidence. -->
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Assurance</p>
<h1>How ARINC Data Loader is verified</h1>
<p class="icl-lead">Every claim on these pages points at a test, a record or a page in the repository. This is the evidence, and how a certification project can use it.</p>
<div class="icl-actions"><a class="icl-button" href="../qualification-kit/">The qualification kit</a><a class="icl-button icl-button--ghost" href="../guide/what-has-been-checked/">What has been checked</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<ul class="adl-stats">
<li><strong>550+</strong><span>test cases in unit, integration and system tiers, and a test suite that drives the windows</span></li>
<li><strong>16</strong><span>fuzz harnesses on every function that reads bytes from outside</span></li>
<li><strong>90% / 80%</strong><span>line and branch coverage of the portable code, or the build fails</span></li>
<li><strong>69</strong><span>faults put in on purpose; each one that changes behaviour is caught</span></li>
</ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>Checked more than one way</h2>
<p>The tests are arranged as levels of evidence, each answering a question the one before it cannot.</p>
</div>
<ul class="icl-points">
<li><h3>Against the standards</h3><p>Unit tests check each piece against the standard's own numbers and examples, the MD5 and SHA-1 test vectors, round trips, and every truncation and bit change of a good file, under the sanitizers.</p></li>
<li><h3>Against a second implementation</h3><p>The ARINC 665 files and the ARINC 615A packets and protocol files are compared byte for byte with the output of separately written scripts that share no code with the library. The test target's TFTP is a second implementation too.</p></li>
<li><h3>Against a simulated target</h3><p>Whole operations run on a fake platform whose clock moves only when told, over a network that loses, repeats and reorders datagrams from fixed seeds, against a target that waits, aborts and goes silent.</p></li>
<li><h3>Over real sockets</h3><p>Whole operations run over the host's UDP sockets against the Mock Target's engine, and the three programs are driven through the whole demonstration.</p></li>
<li><h3>On RTOS simulators</h3><p>The platform conformance suite and the engine tests run on FreeRTOS and Zephyr simulators, and on QEMU with the Zephyr SDK's own toolchain.</p></li>
<li><h3>By breaking the code</h3><p>The ARINC 665 library, the codec and the engine were broken on purpose, one change at a time. Every break that a test missed at first got a test.</p></li>
</ul>
<p class="icl-muted">Next on the roadmap: interoperability, with target equipment and with other vendors' loaders and media sets. <a href="../guide/what-has-been-checked/">What has been checked</a> sets every component against every level.</p>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>Traced to the standards</h2>
<p>The code cites ARINC 665-3 and ARINC 615A-3 by section. Each coverage page goes through its standard section by section and names the test files and test cases that hold each part, and the choice made where the standard is unclear. A check fails the build if a named test does not exist.</p>
<ul class="adl-links"><li><a href="../developers/arinc665-coverage/">ARINC 665-3 coverage</a></li><li><a href="../developers/arinc615a-coverage/">ARINC 615A-3 coverage</a></li></ul>
</div>
<div>
<h2>Built and released under control</h2>
<ul class="adl-ticks">
<li>Every change passes the build on Windows, Linux and macOS, warning-free, under the sanitizers, before it merges.</li>
<li>clang-tidy, MSVC Code Analysis and CodeQL run every night.</li>
<li>The Python tools of the build and the actions of CI are pinned by hash.</li>
<li>Every release file is signed, listed in <code>SHA512SUMS</code>, and has a build attestation. Each release has a software bill of materials in SPDX and CycloneDX.</li>
</ul>
<ul class="adl-links"><li><a href="../guide/verifying-releases/">Verifying a release</a></li><li><a href="../developers/security/">Security</a></li></ul>
</div>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>For certification projects</h2>
<p>A project under DO-178C that uses a tool to prepare, check or load software decides whether the tool needs qualification under DO-330, and at what level. The evidence for that is about the tool. The Enterprise edition's <a href="../qualification-kit/">qualification kit</a> puts that evidence together for each supported version, much of it from the records above.</p>
<p>The qualification itself belongs to your project: your plan, your assessment of the tool in your environment, and your authority's acceptance. The kit supports that work and does not replace it.</p>
</div>
<div>
<h2>Safety of use</h2>
<p>Use any loading tool with your own checks. Unless your programme has assessed the tool for the purpose, do not rely on it alone to find an error in software or data that will be loaded into equipment in service, or to leave that software or data unchanged. <a href="../guide/safety-of-use/">Safety of use</a> has the whole statement.</p>
</div>
</div>
</div></section>

<section class="icl-section icl-section--tint icl-cta"><div class="icl-wrap">
<h2>Assessing the tools for a programme?</h2>
<p class="icl-lead">Tell us the software level, the tools you use and how you use them.</p>
<div class="icl-actions"><a class="icl-button" href="../support/#talk-to-us">Talk to us</a><a href="../qualification-kit/">The qualification kit</a></div>
</div></section>
