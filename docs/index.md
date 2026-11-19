---
template: marketing.html
title: ARINC Data Loader
title_tag: "ARINC Data Loader: ARINC 615A and ARINC 665 library and tools"
description: A C++23 library for the ARINC 615A loading protocol and ARINC 665 media sets, and the tools built on it, for your own ground tools, loader hardware and test benches.
---
<!-- OWNER DECISION: the editions, their names and what each includes come from docs/plan/editions.md and docs/plan/licensing.md.
     Pro and Enterprise are not on sale yet. Settle the terms before this page reaches main. -->
<section class="icl-hero icl-hero--app"><div class="icl-wrap"><div class="icl-hero__grid">
<div>
<p class="icl-eyebrow">ARINC 615A-3 and ARINC 665-3</p>
<h1>Build data loading into your own tools</h1>
<p class="icl-lead">ARINC Data Loader is a C++23 library for the ARINC 615A loading protocol and ARINC 665 media sets, with the tools built on it. Put the library in your ground support equipment and loader hardware, and use the tools to package loads, load targets and test on the bench.</p>
<div class="icl-actions"><a class="icl-button" href="downloads/">Download</a><a class="icl-button icl-button--ghost" href="pricing/">Compare editions</a></div>
<ul class="icl-tags"><li>C++23 and CMake</li><li>Windows, Linux, macOS</li><li>FreeRTOS and Zephyr</li><li>Signed releases with a bill of materials</li></ul>
</div>
<figure class="adl-hero-shot"><img src="images/app-upload-complete.png" width="1280" height="900" alt="The Data Loader after an upload: the header says COMPLETE and the load is at 100 percent."></figure>
</div></div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>One library, three tools</h2>
<p>The library does the work. The tools are built on it, so what you test on the bench is the code you ship in your own product.</p>
</div>
<ul class="icl-grid adl-grid-4">
<li><article class="icl-card"><p class="adl-kicker">For developers</p><h3><a href="products/library/">ADL Library</a></h3><p>The ARINC 615A protocol engine, the ARINC 665 media set code and a platform layer, as static libraries and a CMake package.</p><span class="icl-card__tag">Embed it</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="products/data-loader/">Data Loader</a></h3><p>Find targets on the network, read what they hold, upload loads and download files, with a log and a receipt for every operation.</p><span class="icl-card__tag">Load targets</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="products/media-set-generator/">Media Set Generator</a></h3><p>Turn your files into an ARINC 665 load and media set, with its lists and check values, verified by reading it back.</p><span class="icl-card__tag">Package loads</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="products/mock-target/">Mock Target</a></h3><p>A software target that answers a loader over real network sockets, and waits, refuses, aborts or goes silent when you ask it to.</p><span class="icl-card__tag">Test without hardware</span></article></li>
</ul>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>Made to be built into other things</h2>
<p>The ARINC 615A engine holds no socket, thread or clock. It is told the time and the bytes that arrived, and it answers with the bytes to send. It works in memory the caller gives it, and throws nothing. That is what lets the same code run in a desktop tool, in a test with no network, and on a real-time operating system.</p>
<ul class="adl-ticks">
<li>The operating system is chosen by CMake, behind one platform layer. There is no <code>#ifdef</code> in the code.</li>
<li>A fake platform runs whole loading operations with no network and no real time, so your own tests are repeatable.</li>
<li>Every error is a value (<code>std::expected</code>). Nothing throws across a library boundary.</li>
</ul>
<ul class="adl-links"><li><a href="products/library/">The library</a></li><li><a href="developers/architecture/">Architecture</a></li></ul>
</div>
<ol class="adl-stack" aria-label="How the products are layered, from the tools at the top to the operating systems at the bottom">
<li><span class="adl-stack__name">Tools</span><ul class="adl-stack__row"><li>Data Loader</li><li>Media Set Generator</li><li>Mock Target</li><li class="adl-stack__yours">Your loader or tool</li></ul></li>
<li class="adl-stack--libs"><span class="adl-stack__name">ADL Library</span><ul class="adl-stack__row"><li>ARINC 615A<small>codec, engine, runner</small></li><li>ARINC 665<small>loads, media sets, check values</small></li><li>Media tree<small>media sets on disk</small></li></ul></li>
<li><span class="adl-stack__name">Platform layer</span><ul class="adl-stack__row"><li>Files</li><li>Memory</li><li>Threads and clock</li><li>UDP and queues</li></ul></li>
<li><span class="adl-stack__name">Operating systems</span><ul class="adl-stack__row"><li>Windows</li><li>Linux</li><li>macOS</li><li>FreeRTOS</li><li>Zephyr</li><li class="adl-stack__yours">Your RTOS</li></ul></li>
</ol>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>From files to a loaded target</h2>
<p>The three tools do the whole job on one computer. For a target on your network, the Data Loader is given its address in place of the Mock Target's.</p>
</div>
<ol class="adl-steps">
<li><h3>Package</h3><p>The Media Set Generator turns your data and support files into an ARINC 665 media set, then reads it back and verifies it before it is written into place.</p></li>
<li><h3>Load</h3><p>The Data Loader checks the media set, finds the target, uploads the loads you choose and downloads files from the target.</p></li>
<li><h3>Test</h3><p>The Mock Target stands in for the equipment, and can be told to misbehave, so you see how a loading process handles each failure.</p></li>
</ol>
<ul class="adl-links"><li><a href="how-to-use/">How to use the tools</a></li><li><a href="guide/demonstration/">The ten-minute demonstration</a></li></ul>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>Platforms</h2>
<p>The tools run on the three desktop systems. The library runs there too, and on real-time operating systems through the same platform layer.</p>
</div>
<div class="adl-table"><table>
<thead><tr><th scope="col">Platform</th><th scope="col">Library</th><th scope="col">Tools</th></tr></thead>
<tbody>
<tr><th scope="row">Windows, Linux, macOS</th><td><span class="adl-chip adl-chip--ok">Supported</span></td><td><span class="adl-chip adl-chip--ok">Supported</span></td></tr>
<tr><th scope="row">FreeRTOS, Zephyr</th><td><span class="adl-chip adl-chip--exp">Experimental</span></td><td><span class="adl-no"><span class="icl-sr">Not offered</span></span></td></tr>
<tr><th scope="row">QNX, Green Hills INTEGRITY</th><td><span class="adl-chip adl-chip--port">Port written</span></td><td><span class="adl-no"><span class="icl-sr">Not offered</span></span></td></tr>
<tr><th scope="row">VxWorks</th><td><span class="adl-chip adl-chip--plan">Planned</span></td><td><span class="adl-no"><span class="icl-sr">Not offered</span></span></td></tr>
</tbody></table></div>
<ul class="adl-links"><li><a href="platforms/">Every platform, and what has run on it</a></li><li><a href="developers/platform-abstraction/">Porting to your platform</a></li></ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>Where it fits</h2>
<p>Four ways teams use it.</p>
</div>
<ul class="icl-grid adl-grid-4">
<li><article class="icl-card"><h3><a href="solutions/#build-a-data-loader">Build a data loader</a></h3><p>Put the ARINC 615A engine in your ground support equipment or loader hardware, on a desktop system or an RTOS.</p></article></li>
<li><article class="icl-card"><h3><a href="solutions/#package-loadable-software">Package loadable software</a></h3><p>Make ARINC 665 media sets in your build, with the generator or with the library, and check them before they leave.</p></article></li>
<li><article class="icl-card"><h3><a href="solutions/#test-benches-and-ci">Test benches and CI</a></h3><p>Exercise a loading process against a target that fails on request, and run it in continuous integration with no hardware.</p></article></li>
<li><article class="icl-card"><h3><a href="solutions/#certification-projects">Certification projects</a></h3><p>Assess the tools in your own programme, with the evidence of the qualification kit.</p></article></li>
</ul>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>Built to be assessed</h2>
<p>Tools used on aircraft software get looked at closely. The evidence is kept with the code, and every release is signed.</p>
</div>
<ul class="adl-stats">
<li><strong>550+</strong><span>test cases, in unit, integration and system tiers</span></li>
<li><strong>16</strong><span>fuzz harnesses on the code that reads bytes from outside</span></li>
<li><strong>90% / 80%</strong><span>line and branch coverage the build must hold</span></li>
<li><strong>69</strong><span>faults put into the code on purpose to test the tests</span></li>
</ul>
<ul class="adl-ticks">
<li>Each section of ARINC 665-3 and ARINC 615A-3 is set against the tests that hold it, with the places where the standard is unclear and the choice made.</li>
<li>The files the library writes are compared byte for byte with a second, separately written implementation.</li>
<li>Every release is signed, with checksums, build attestations and a software bill of materials.</li>
</ul>
<ul class="adl-links"><li><a href="assurance/">How it is verified</a></li><li><a href="qualification-kit/">The qualification kit</a></li></ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>Editions</h2>
<p>Community is the bench, free and open source. Pro is the rig: many targets, unattended, with the record kept. Enterprise is the programme: the site, and the evidence for certification work.</p>
</div>
<ul class="adl-plans">
<li class="adl-plan"><p class="adl-kicker">The bench</p><h3>Community</h3><p class="adl-plan__price">Free</p><p class="adl-plan__for">The library and the tools, under the GPL, for an engineer at a bench, with help from the community.</p><a class="icl-button icl-button--ghost" href="downloads/">Download</a></li>
<li class="adl-plan adl-plan--featured"><p class="adl-kicker">The rig</p><h3>Pro</h3><p class="adl-plan__price">On request</p><p class="adl-plan__for">For test labs, rigs and production lines: support, supported versions, and tools for loading many targets unattended and keeping the record.</p><a class="icl-button" href="pricing/">See Pro</a></li>
<li class="adl-plan"><p class="adl-kicker">The programme</p><h3>Enterprise</h3><p class="adl-plan__price">On request</p><p class="adl-plan__for">For programmes and companies: everything in Pro for the site, the qualification kit for each supported version, and platform work by agreement.</p><a class="icl-button icl-button--ghost" href="pricing/">See Enterprise</a></li>
</ul>
</div></section>

<section class="icl-section icl-section--tint icl-cta"><div class="icl-wrap">
<h2>Try it on one computer</h2>
<p class="icl-lead">Make a media set, start the Mock Target and load it, in about ten minutes, with no hardware.</p>
<div class="icl-actions"><a class="icl-button" href="downloads/">Download</a><a href="guide/demonstration/">Read the demonstration</a><a href="support/#talk-to-us">Talk to us</a></div>
</div></section>
