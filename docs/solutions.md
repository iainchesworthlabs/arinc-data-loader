---
template: marketing.html
title: Solutions
title_tag: "Solutions: ARINC Data Loader"
description: How makers of loader hardware, suppliers of loadable software, test labs and certification projects use the ADL Library and the tools.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Solutions</p>
<h1>Where ARINC Data Loader fits in your work</h1>
<p class="icl-lead">Four jobs, and the parts of the product each one uses.</p>
<ul class="adl-uses"><li><a href="#build-a-data-loader">Build a data loader</a></li><li><a href="#package-loadable-software">Package loadable software</a></li><li><a href="#test-benches-and-ci">Test benches and CI</a></li><li><a href="#certification-projects">Certification projects</a></li></ul>
</div></section>

<section class="icl-section icl-section--tint adl-solution" id="build-a-data-loader"><div class="icl-wrap">
<div class="adl-two">
<div>
<p class="adl-kicker">For makers of ground support equipment and loader hardware</p>
<h2>Build a data loader</h2>
<p>Put the ARINC 615A side of the loader in your own product, so your team writes the user interface, the hardware support and the workflow, and not the protocol.</p>
<ul class="adl-ticks">
<li>The engine runs FIND, Information, Upload and both Downloads, with the time-outs, retries and aborts of the standard.</li>
<li>It holds no socket, thread or clock, so it fits a desktop program, a portable loader on an RTOS, or a network stack of your own.</li>
<li>The runner drives a whole operation over UDP when you want it to.</li>
<li>The ARINC 665 code checks every media set before your product sends it.</li>
<li>Enterprise adds platform and target-side work on your system, by agreement.</li>
</ul>
<ul class="adl-uses"><li><a href="../products/library/">ADL Library</a></li><li><a href="../platforms/">Platforms</a></li><li><a href="../products/mock-target/">Mock Target</a></li></ul>
</div>
<figure class="adl-shot"><img src="../images/app-workbench-ready.png" width="1280" height="900" loading="lazy" alt="The Data Loader with a media set checked and a target named, ready to upload."><figcaption>The Data Loader is one loader built on the library.</figcaption></figure>
</div>
</div></section>

<section class="icl-section adl-solution" id="package-loadable-software"><div class="icl-wrap">
<div class="adl-two">
<div>
<p class="adl-kicker">For suppliers of avionics software</p>
<h2>Package loadable software</h2>
<p>Deliver each software part as an ARINC 665 media set: the header file, the data and support files, the lists and the check values, laid out the way a loader expects.</p>
<ul class="adl-ticks">
<li>The Media Set Generator builds a set from a form, and verifies it by reading it back.</li>
<li>The library does the same in your build pipeline, with no window, from a description of the loads.</li>
<li>The check reports each place a set departs from the standard's recommendations, with the clause, before the set leaves you.</li>
<li>Sample sets, including the ARINC 615 and Boeing 777 layouts of Appendix F, for trying a loader.</li>
<li>Pro adds a media set pipeline: batches of sets from one table, comparison and manifests (planned).</li>
</ul>
<ul class="adl-uses"><li><a href="../products/media-set-generator/">Media Set Generator</a></li><li><a href="../products/library/">ADL Library</a></li></ul>
</div>
<figure class="adl-shot"><img src="../images/gen-complete.png" width="1280" height="1200" loading="lazy" alt="The Media Set Generator after it wrote and verified a media set."><figcaption>A media set, written and verified.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint adl-solution" id="test-benches-and-ci"><div class="icl-wrap">
<div class="adl-two">
<div>
<p class="adl-kicker">For test labs and integrators</p>
<h2>Test benches and CI</h2>
<p>Exercise a loading process without the equipment, against a target that fails on demand in eight ways, and run the same operations again in continuous integration.</p>
<ul class="adl-ticks">
<li>The Mock Target answers over real sockets, and waits, refuses, drops, corrupts, aborts or goes silent when told to.</li>
<li>The Data Loader gives a log and a receipt for each operation, to compare runs.</li>
<li>In your own tests, the library's fake platform runs whole operations with no network and with time that moves only when told, so a failure repeats every time.</li>
<li>Pro adds rig automation, kept records and a rack of simulated targets with repeatable fault scenarios (planned).</li>
</ul>
<ul class="adl-uses"><li><a href="../products/mock-target/">Mock Target</a></li><li><a href="../products/data-loader/">Data Loader</a></li><li><a href="../developers/testing/">Testing</a></li></ul>
</div>
<figure class="adl-shot"><img src="../images/mock-faults.png" width="1280" height="1320" loading="lazy" alt="The Mock Target with one fault switched on: Refuse Upload."><figcaption>A fault switched on in the Mock Target.</figcaption></figure>
</div>
</div></section>

<section class="icl-section adl-solution" id="certification-projects"><div class="icl-wrap">
<div class="adl-two">
<div>
<p class="adl-kicker">For programmes that assess their tools</p>
<h2>Certification projects</h2>
<p>A project that uses a tool to prepare, check or load software decides whether the tool needs to be assessed, and at what level. The evidence for that assessment is about the tool: what it is for, how it was made and checked, and what it could get wrong.</p>
<ul class="adl-ticks">
<li>The qualification kit of the Enterprise edition collects that evidence for each supported version.</li>
<li>Much of it comes from records the project keeps already: the tests and their results, the coverage pages, the build records, the signed releases.</li>
<li>Your project keeps its own part: the plan, the level, the assessment in your environment, and the dealings with your authority.</li>
</ul>
<ul class="adl-uses"><li><a href="../qualification-kit/">Qualification kit</a></li><li><a href="../assurance/">How it is verified</a></li><li><a href="../pricing/">Enterprise</a></li></ul>
</div>
<ul class="adl-stats">
<li><strong>550+</strong><span>test cases</span></li>
<li><strong>16</strong><span>fuzz harnesses</span></li>
<li><strong>2</strong><span>standards traced section by section</span></li>
</ul>
</div>
</div></section>

<section class="icl-section icl-section--tint icl-cta"><div class="icl-wrap">
<h2>Not sure which fits?</h2>
<p class="icl-lead">Tell us what you load, and onto what.</p>
<div class="icl-actions"><a class="icl-button" href="../support/#talk-to-us">Talk to us</a><a href="../pricing/">Compare editions</a></div>
</div></section>
