---
template: marketing.html
title: Products
title_tag: "Products: ARINC Data Loader"
description: The ADL Library and the three tools built on it, the Data Loader, the Media Set Generator and the Mock Target, and how they fit together.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Products</p>
<h1>A library and the tools built on it</h1>
<p class="icl-lead">The ADL Library implements ARINC 615A and ARINC 665. The three tools are built on it and come in one package for Windows, Linux and macOS. Use the tools as they are, build the library into your own, or both.</p>
<div class="icl-actions"><a class="icl-button" href="../downloads/">Download</a><a class="icl-button icl-button--ghost" href="../pricing/">Compare editions</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2 class="icl-sr">The products</h2>
<ul class="icl-grid adl-grid-4">
<li><article class="icl-card"><p class="adl-kicker">For developers</p><h3><a href="library/">ADL Library</a></h3><p>Static C++23 libraries and a CMake package: the ARINC 615A codec, engine and runner, the ARINC 665 code, the media tree and the platform layer.</p><span class="icl-card__tag">Desktop systems and RTOS ports</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="data-loader/">Data Loader</a></h3><p>Checks a media set, finds targets, and runs Information, Upload and Download, with a log and a receipt.</p><span class="icl-card__tag">Windows, Linux, macOS</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="media-set-generator/">Media Set Generator</a></h3><p>Builds an ARINC 665 load and media set from your files, and verifies it by reading it back.</p><span class="icl-card__tag">Windows, Linux, macOS</span></article></li>
<li><article class="icl-card"><p class="adl-kicker">Desktop tool</p><h3><a href="mock-target/">Mock Target</a></h3><p>A software ARINC 615A target over real sockets, with faults you switch on.</p><span class="icl-card__tag">Windows, Linux, macOS</span></article></li>
</ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>How they fit together</h2>
<p>Every tool is a window on the library. The Media Set Generator writes media sets with the ARINC 665 code; the Data Loader reads and checks them with the same code, and loads them with the ARINC 615A engine; the Mock Target answers it with a separately written transfer layer, so the two ends of a dialogue share no transfer code.</p>
<p>A program of your own can use any part: the ARINC 665 code alone in a build pipeline, the engine alone on an embedded target with its own network stack, or the runner for a whole operation over UDP.</p>
<ul class="adl-links"><li><a href="../developers/architecture/">Architecture</a></li><li><a href="../features/">All features</a></li></ul>
</div>
<ol class="adl-stack" aria-label="How the products are layered, from the tools at the top to the operating systems at the bottom">
<li><span class="adl-stack__name">Tools</span><ul class="adl-stack__row"><li>Data Loader</li><li>Media Set Generator</li><li>Mock Target</li><li class="adl-stack__yours">Your loader or tool</li></ul></li>
<li class="adl-stack--libs"><span class="adl-stack__name">ADL Library</span><ul class="adl-stack__row"><li>ARINC 615A<small>codec, engine, runner</small></li><li>ARINC 665<small>loads, media sets, check values</small></li><li>Media tree<small>media sets on disk</small></li></ul></li>
<li><span class="adl-stack__name">Platform layer</span><ul class="adl-stack__row"><li>Files</li><li>Memory</li><li>Threads and clock</li><li>UDP and queues</li></ul></li>
<li><span class="adl-stack__name">Operating systems</span><ul class="adl-stack__row"><li>Windows</li><li>Linux</li><li>macOS</li><li>FreeRTOS</li><li>Zephyr</li><li class="adl-stack__yours">Your RTOS</li></ul></li>
</ol>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>In every package</h2>
<p>Each release has a package of the programs for each desktop system, and a development package of the library.</p>
</div>
<ul class="adl-ticks">
<li>The Data Loader, the Media Set Generator, the Mock Target, and the command-line programs <code>adl_write_payloads</code> and <code>adl_load</code>.</li>
<li>The development package: the headers, the static libraries and the <code>adl</code> CMake package.</li>
<li>Checksums, a signature for every file, build attestations, and a software bill of materials in SPDX and CycloneDX.</li>
</ul>
<ul class="adl-links"><li><a href="../downloads/">Downloads</a></li><li><a href="../guide/verifying-releases/">Verifying a release</a></li></ul>
</div></section>
