---
template: marketing.html
title: Downloads
title_tag: "Downloads: ARINC Data Loader"
description: Download the ARINC Data Loader programs for Windows, Linux and macOS, the development package of the ADL Library, and the source, and check what you downloaded.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Downloads</p>
<h1>Download ARINC Data Loader</h1>
<p class="icl-lead">Each release has a portable package of the programs for each desktop system, a development package of the library, and the files to check them with. The Community edition is these packages, free.</p>
<div class="icl-actions"><a class="icl-button" href="https://github.com/iainchesworthlabs/arinc-data-loader/releases/latest">Get the latest release</a><a class="icl-button icl-button--ghost" href="../whats-new/">Release notes</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>The programs</h2>
<p>Each package holds the Data Loader, the Media Set Generator, the Mock Target and the command-line program <code>adl_write_payloads</code>, with the Qt files they need. There is nothing to install first: unpack it and start a program.</p>
<div class="adl-table"><table>
<thead><tr><th scope="col">System</th><th scope="col">Package</th><th scope="col">To start</th></tr></thead>
<tbody>
<tr><th scope="row">Windows (x86-64)</th><td><code>arinc-data-loader-&lt;version&gt;-windows-x86_64.zip</code></td><td><p>Unpack the folder and start <code>adl_app.exe</code>.</p></td></tr>
<tr><th scope="row">Linux (x86-64)</th><td><code>arinc-data-loader-&lt;version&gt;-linux-x86_64.tar.gz</code></td><td><p>Unpack it and run <code>bin/adl_app</code>. The archive has launcher files and icons.</p></td></tr>
<tr><th scope="row">macOS 14.4 or later (Apple silicon)</th><td><code>arinc-data-loader-&lt;version&gt;-macos-arm64.dmg</code></td><td><p>Open the disk image and drag the applications to Applications.</p></td></tr>
</tbody></table></div>
<ul class="adl-links"><li><a href="../guide/installing/">Installing and starting</a></li><li><a href="../guide/demonstration/">The ten-minute demonstration</a></li></ul>
</div></section>

<section class="icl-section" id="the-development-package"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>The development package</h2>
<p>The headers, the static libraries and the <code>adl</code> CMake package of the ADL Library, for Windows, Linux and macOS. It needs no Qt, and a program built with it needs nothing beside it when it runs.</p>
<p>The package is named <code>arinc-data-loader-dev-&lt;version&gt;-&lt;system&gt;-&lt;arch&gt;</code>. Give its folder to CMake as the prefix path.</p>
<ul class="adl-links"><li><a href="../products/library/">ADL Library</a></li><li><a href="../developers/libraries/">Using the libraries</a></li></ul>
</div>
<div>
<h2>The source</h2>
<p>Every edition is built from the same source, on GitHub, under the GPL. Build it with CMake and a C++23 compiler; the FreeRTOS and Zephyr ports are built by projects of their own.</p>
<pre class="adl-code"><code>git clone https://github.com/iainchesworthlabs/arinc-data-loader
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release</code></pre>
<ul class="adl-links"><li><a href="https://github.com/iainchesworthlabs/arinc-data-loader">Source on GitHub</a></li><li><a href="../developers/building/">Building</a></li></ul>
</div>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>Check what you downloaded</h2>
<p>Every file of a release is listed in <code>SHA512SUMS</code>, signed with the release key, and has a build attestation that says GitHub's runner built it from this repository. Each release also has a software bill of materials, in SPDX and CycloneDX.</p>
<ul class="adl-links"><li><a href="../guide/verifying-releases/">Verifying a release</a></li></ul>
</div>
<div>
<h2>What the programs need</h2>
<ul class="adl-ticks">
<li>Windows (x86-64), Linux (x86-64) or macOS 14.4 or later on Apple silicon.</li>
<li>For a target: a network path to it that carries UDP over IPv4.</li>
<li>The packages are not signed for an operating system's store, so Windows and macOS may warn the first time a program starts. <a href="../guide/installing/">Installing</a> says what to do.</li>
</ul>
</div>
</div>
</div></section>

<section class="icl-section icl-cta"><div class="icl-wrap">
<h2>Need support or a licence?</h2>
<p class="icl-lead">Pro and Enterprise use the same packages, with support and a licence file.</p>
<div class="icl-actions"><a class="icl-button" href="../pricing/">Compare editions</a><a href="../support/#talk-to-us">Talk to us</a></div>
</div></section>
