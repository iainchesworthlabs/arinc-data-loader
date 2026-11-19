---
template: marketing.html
title: Data Loader
title_tag: "Data Loader: an ARINC 615A loader for Windows, Linux and macOS"
description: The Data Loader checks an ARINC 665 media set, finds ARINC 615A targets, reads their information, uploads loads and downloads files, with a log and a receipt for every operation.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Data Loader</p>
<h1>Load targets over Ethernet, with a record of every step</h1>
<p class="icl-lead">A desktop ARINC 615A data loader for Windows, Linux and macOS. It checks the media set first, then finds the target, reads it, uploads to it and downloads from it.</p>
<div class="icl-actions"><a class="icl-button" href="../../downloads/">Download</a><a class="icl-button icl-button--ghost" href="../../guide/loader/">The loader, screen by screen</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Check the media set before anything is sent</h2>
<p>The loader opens a media set as a folder and reads it, and reading it is the check: a set that cannot be read cannot be uploaded. Where a set departs from what the standard recommends, the loader shows each finding with its clause and carries on. If a file changes on disk after the check, the set must be checked again before it is uploaded.</p>
<p><a href="../../guide/choose-a-media-set/">Choose and check a media set</a></p>
</div>
<figure class="adl-shot"><img src="../../images/app-set-warnings.png" width="1280" height="900" loading="lazy" alt="The loader showing a media set that has findings: each one names the clause of the standard."><figcaption>A media set with findings, each with its clause.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Find a target, or name one</h2>
<p>FIND broadcasts one request on the local network, listens for three seconds and lists the targets that answer, up to 64. A target can also be named by its address and identifier, which reaches one that does not answer FIND or sits behind a router.</p>
<p><a href="../../guide/find-a-target/">Find a target</a></p>
</div>
<figure class="adl-shot"><img src="../../images/app-information.png" width="1280" height="900" loading="lazy" alt="The loader after an Information operation on a target."><figcaption>What a target holds, from the Information operation.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Upload and download</h2>
<p>Upload sends the loads you tick, with progress for the whole job and for each load. Download works both ways the standard describes: you name the files (media defined), or the target lists them and you choose (operator defined). Downloaded files go into a new folder for each operation, and an existing file is never overwritten. Every operation can be aborted.</p>
<p><a href="../../guide/upload/">Upload</a> and <a href="../../guide/download/">Download</a></p>
</div>
<figure class="adl-shot"><img src="../../images/app-upload-running.png" width="1280" height="900" loading="lazy" alt="An upload in progress: the progress of the whole job and of the load."><figcaption>An upload in progress.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>A log and a receipt for every operation</h2>
<p>Each operation has an activity log you can copy, and ends with a receipt: COMPLETE or FAILED, what the target did, and the time it ended. A failure says what went wrong and, where it is known, what to do next.</p>
<p><a href="../../guide/log-and-receipt/">The log, the receipt and failures</a></p>
</div>
<figure class="adl-shot"><img src="../../images/app-upload-receipt.png" width="1280" height="900" loading="lazy" alt="The receipt of an upload: the outcome, how many files the target read, and the time it ended."><figcaption>The receipt of an upload.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>Details</h2>
<div class="adl-table"><table>
<tbody>
<tr><th scope="row">Standards</th><td><p>ARINC 615A-3 for the loading protocol, over TFTP (RFC 1350, with the options of RFCs 2347 to 2349). ARINC 665-3 for the media set.</p></td></tr>
<tr><th scope="row">Operations</th><td><p>FIND, Information, Upload, and Download media defined and operator defined, each with an abort.</p></td></tr>
<tr><th scope="row">Network</th><td><p>UDP over IPv4. Requests go to port 59 of the target; FIND uses port 1001.</p></td></tr>
<tr><th scope="row">Systems</th><td><p>Windows (x86-64), Linux (x86-64), macOS 14.4 or later (Apple silicon). A portable package for each; no installer.</p></td></tr>
<tr><th scope="row">Languages</th><td><p>English, French, Spanish, German, Yiddish, Hebrew, Simplified Chinese and Japanese. Hebrew and Yiddish mirror the whole window.</p></td></tr>
<tr><th scope="row">Use</th><td><p>Light and dark, following the system. The loader can be used without a mouse. A status is shown by a symbol and a word as well as by colour.</p></td></tr>
<tr><th scope="row">Built on</th><td><p>The <a href="../library/">ADL Library</a>: the same ARINC 615A engine and ARINC 665 code you can build into your own tools.</p></td></tr>
</tbody></table></div>
<ul class="adl-links"><li><a href="../media-set-generator/">Media Set Generator</a></li><li><a href="../mock-target/">Mock Target</a></li><li><a href="../../pricing/">Editions</a></li></ul>
</div></section>
