---
template: marketing.html
title: ADL Library
title_tag: "ADL Library: ARINC 615A and ARINC 665 in C++23"
description: The ARINC 615A protocol engine, the ARINC 665 media set code and a platform layer, as static C++23 libraries and a CMake package, for your own loaders and tools.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">ADL Library</p>
<h1>ARINC 615A and ARINC 665 for your own programs</h1>
<p class="icl-lead">The code under the Data Loader, the Media Set Generator and the Mock Target, as static libraries and a CMake package. Build it into a data loader, a ground tool, a test bench or a build pipeline.</p>
<div class="icl-actions"><a class="icl-button" href="../../downloads/#the-development-package">Get the development package</a><a class="icl-button icl-button--ghost" href="../../developers/libraries/">Read the developer guide</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>What is in it</h2>
<p>Five libraries, each a CMake target. A program links the ones it needs.</p>
</div>
<div class="adl-table"><table>
<thead><tr><th scope="col">Target</th><th scope="col">What it does</th></tr></thead>
<tbody>
<tr><th scope="row"><code>adl::arinc665</code></th><td><p>Writes and reads everything ARINC 665-3 defines: loads with their header, data and support files, batch files, media sets with their load, file and batch lists, media labels, part numbers, and the CRC-8, CRC-16, CRC-32, MD5 and SHA-1 check values. It checks a load or a media set end to end and reports each place it departs from the standard's recommendations, with the clause.</p></td></tr>
<tr><th scope="row"><code>adl::arinc615a</code></th><td><p>The data loader's side of ARINC 615A-3. The codec writes and reads the TFTP packets with their options, the WAIT and ABORT messages, FIND, and the twelve protocol files. The engine runs FIND and the Information, Upload and Download operations, with the time-outs, retries and abort paths of the standard, as state machines.</p></td></tr>
<tr><th scope="row"><code>adl::arinc615a_runner</code></th><td><p>Drives the engine over the platform layer's UDP sockets, with a file source and sink over a folder. One call runs a whole operation.</p></td></tr>
<tr><th scope="row"><code>adl::media_tree</code></th><td><p>Writes the members of a media set to a folder tree, one folder per member, and reads one back.</p></td></tr>
<tr><th scope="row"><code>adl::platform</code></th><td><p>The interfaces to the operating system (files, memory, threads and their priorities, a clock, UDP sockets and message queues) and the host's implementation of them.</p></td></tr>
</tbody></table></div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>Use it from CMake</h2>
<p>Point CMake at the unpacked development package and link a target. The libraries are static, so a program needs nothing beside it when it runs, and the package does not need Qt.</p>
<pre class="adl-code"><code>find_package(adl 0.1 REQUIRED)

add_executable(my_tool main.cpp)
target_compile_features(my_tool PRIVATE cxx_std_23)
target_link_libraries(my_tool PRIVATE adl::media_tree)</code></pre>
<p>Continuous integration installs the package into an empty folder and builds a program against it, so a header or a library missing from the package fails the release.</p>
</div>
<div>
<h2>Check a media set in a few lines</h2>
<p>Reading is in two steps: the media tree reads the members from disk, and the ARINC 665 library parses and checks them. Every call that can fail returns a <code>std::expected</code>.</p>
<pre class="adl-code"><code>auto members = adl::media_tree::read_media_set_tree(folder);
if (!members) { /* the folder could not be read */ }

auto set = adl::arinc665::read_media_set(*members);
if (!set) { /* not a media set */ }

for (const auto&amp; finding : set-&gt;findings)
    std::cout &lt;&lt; finding.clause &lt;&lt; ": " &lt;&lt; finding.message &lt;&lt; '\n';</code></pre>
<p><a href="../../developers/libraries/">Using the libraries</a> has the whole program, which continuous integration builds against the installed package.</p>
</div>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>Made for embedded targets as well as desktops</h2>
<p>The same engine runs in the desktop tools and on a real-time operating system.</p>
</div>
<ul class="icl-points">
<li><h3>No I/O in the engine</h3><p>The codec and the engine hold no socket, thread, clock or file system. The engine is told the time and the bytes that arrived, and answers with the bytes to send.</p></li>
<li><h3>Memory the caller gives it</h3><p>The engine works in a workspace the caller gives it (about 270 KB with the defaults) and throws nothing. Its one allocation is a check value, when a file that has one ends. Every failure is a value.</p></li>
<li><h3>One platform layer</h3><p>Files, memory, threads and clock, and networking are interfaces, with an implementation for each system. The system is chosen by CMake; the code has no <code>#ifdef</code>.</p></li>
<li><h3>Repeatable tests</h3><p>A fake platform runs whole operations with no network and with time that moves only when told, and a conformance suite checks a new port against the same rules.</p></li>
</ul>
<ul class="adl-links"><li><a href="../../platforms/">Platforms</a></li><li><a href="../../developers/arinc615a-engine/">The ARINC 615A engine</a></li><li><a href="../../developers/platform-abstraction/">Porting to a platform</a></li></ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-intro">
<h2>Written from the standards, section by section</h2>
<p>The code cites ARINC 665-3 and ARINC 615A-3 by section. Two coverage pages go through each standard and give, for every section, what the library does, the tests that hold it, and the choice made where the standard is unclear.</p>
</div>
<ul class="icl-grid">
<li><article class="icl-card"><h3><a href="../../developers/arinc665-coverage/">ARINC 665-3 coverage</a></h3><p>Loads, batches, media sets, lists and check values.</p></article></li>
<li><article class="icl-card"><h3><a href="../../developers/arinc615a-coverage/">ARINC 615A-3 coverage</a></h3><p>TFTP and its options, FIND, the protocol files and the operations.</p></article></li>
<li><article class="icl-card"><h3><a href="../../developers/testing/">Testing</a></h3><p>The test tiers, the golden files, the fuzz harnesses and the coverage gate.</p></article></li>
</ul>
</div></section>

<section class="icl-section icl-section--tint icl-cta"><div class="icl-wrap">
<h2>The library in every edition</h2>
<p class="icl-lead">The library is open source under the GPL in every edition. Pro adds support for your integration. Enterprise adds the qualification kit.</p>
<div class="icl-actions"><a class="icl-button" href="../../pricing/">Compare editions</a><a href="../../developers/">Developer documentation</a></div>
</div></section>
