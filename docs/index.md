---
template: marketing.html
title: ARINC Data Loader
title_tag: "ARINC Data Loader: tools for ARINC 615A and ARINC 665"
description: A data loader, a media set generator and a mock target for ARINC 615A and ARINC 665, and the libraries they are built on. For Windows, Linux and macOS.
---
<section class="icl-hero icl-hero--app"><div class="icl-wrap"><div class="icl-hero__grid icl-hero__grid--solo">
<div>
<p class="icl-eyebrow">Windows, Linux and macOS</p>
<h1>ARINC Data Loader</h1>
<p class="icl-lead">A data loader, a media set generator and a mock target for ARINC 615A and ARINC 665.</p>
<ul class="icl-tags"><li>Preliminary release</li><li>Not yet checked against real equipment</li><li>Free software, GPL-3.0-or-later</li></ul>
<div class="icl-actions" id="get"><a class="icl-button" href="https://github.com/iainchesworthlabs/arinc-data-loader/releases">Get it from GitHub</a><a href="guide/">Read the guide</a></div>
</div>
</div></div></section>

<section class="icl-section"><div class="icl-wrap">
<p class="icl-callout"><strong>Status</strong> This is a preliminary release. The programs work together on one machine. Nothing has been checked against a real target, a real loader or another tool's media sets. <a href="guide/what-has-been-checked/">What has been checked</a> says what has been, and what has not.</p>
<h2>What it is</h2>
<p>ARINC 665 describes how loadable software is packaged into a media set: a header file, data files, support files and the lists that say what is on each member. ARINC 615A describes how a data loader moves a load to a target, and fetches files from it, over an Ethernet network. This project implements both, written from the standards.</p>
<ul class="icl-points">
<li><h3>The data loader</h3><p>Finds a target on the network, reads what it holds, sends a load to it (upload) and fetches files from it (download). It reads and checks a media set before it sends anything.</p></li>
<li><h3>The media set generator</h3><p>Turns files from your disk into a media set, as the folder tree the loader reads, and reads it back to check it.</p></li>
<li><h3>The mock target</h3><p>Stands in for a piece of equipment over real network sockets, so you can try the loader without hardware. It can be told to refuse, to go silent, to send a wrong checksum or to abort.</p></li>
<li><h3>The libraries</h3><p>The ARINC 665 and ARINC 615A code, and the platform layer under it, for programs of your own. The protocol engine holds no socket, thread or clock.</p></li>
</ul>
<figure class="adl-shot"><img src="images/app-upload-complete.png" width="1280" height="900" loading="lazy" alt="The data loader after an upload to the mock target: the header says COMPLETE and the one load is at 100 percent."><figcaption>The data loader after an upload to the mock target.</figcaption></figure>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>Who it is for</h2>
<div class="adl-two">
<div>
<h3>It is for</h3>
<ul>
<li>Engineers and technicians who prepare loads and want to see how a loader and a target talk, on a bench, without equipment.</li>
<li>Programmers who build tools for ARINC 665 and ARINC 615A and want a library, with its tests and its list of unclear places in the standards.</li>
</ul>
</div>
<div>
<h3>It is not for</h3>
<ul>
<li>Loading software into equipment in service. It has not been checked against real equipment.</li>
<li>Work that needs evidence about the tool itself. The <a href="assurance/">Assurance</a> page says what exists and what does not.</li>
</ul>
</div>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<h2>How it works</h2>
<ol class="adl-steps">
<li><h3>Make a media set</h3><p>Choose your files in the generator. It writes the media set and reads it back.</p></li>
<li><h3>Start a target</h3><p>Start the mock target on your computer, or have the address of a target on your network.</p></li>
<li><h3>Load and fetch</h3><p>The loader checks the set, uploads it, and downloads files from the target. Every step has a log and a receipt.</p></li>
</ol>
<p><a href="how-to-use/">How to use</a> has the three steps with pictures. <a href="guide/demonstration/">The demonstration</a> in the guide does the whole job on one machine in about ten minutes.</p>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>What it needs</h2>
<ul>
<li>Windows (x86-64), Linux (x86-64) or macOS (Apple silicon, macOS 14.4 or later). Each release has a portable package. There is no installer.</li>
<li>For a target: a network path to it that carries UDP over IPv4. The loader sends its requests to port 59 of the target, and FIND uses port 1001.</li>
<li>For the libraries: a C++23 compiler and CMake. <a href="developers/building/">Building</a> lists the versions that have been tried.</li>
</ul>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<h2>What it does not do</h2>
<ul>
<li>It does not run on the equipment being loaded. There is no target-side program. The mock target stands in for a target in demonstrations and tests, and it is not one.</li>
<li>It has not been checked against real equipment, another vendor's loader or target, or media sets from another tool.</li>
<li>It does not authenticate a target or encrypt anything. ARINC 615A over TFTP has neither.</li>
<li>FIND does not reach a target on the same computer, does not cross routers, and cannot be used on a network with no broadcast. You can type the target's address instead.</li>
<li>The loader does one job at a time. The generator makes one load per media set.</li>
</ul>
<p>The <a href="guide/limits/">limits and known problems</a>, <a href="guide/safety-of-use/">safety of use</a> and the <a href="guide/what-has-been-checked/">list of what has been checked</a> are in the guide.</p>
<p>ARINC Data Loader is free software under the GNU General Public License, version 3 or later, and comes with no warranty. It is made by Iain Chesworth Labs. <a href="licensing/">Licensing and editions</a> has the detail.</p>
</div></section>
