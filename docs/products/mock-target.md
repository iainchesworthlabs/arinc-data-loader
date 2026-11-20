---
template: marketing.html
title: Mock Target
title_tag: "Mock Target: an ARINC 615A target in software"
description: The Mock Target answers an ARINC 615A loader over real network sockets, keeps what is uploaded to it, and misbehaves on request, for test benches, demonstrations and continuous integration.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Mock Target</p>
<h1>A target on your desk that fails when you ask it to</h1>
<p class="icl-lead">The Mock Target answers a loader over real network sockets the way this project reads ARINC 615A. It answers FIND, serves Information, Upload and both Downloads, keeps what is uploaded to it, and can be set to wait, refuse, drop, corrupt, abort or go silent.</p>
<div class="icl-actions"><a class="icl-button" href="../../downloads/">Download</a><a class="icl-button icl-button--ghost" href="../../guide/mock-target/">Run the Mock Target</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>A target you can configure</h2>
<p>Give it the identity a target reports to FIND (hardware ID, type, position, name and manufacturer), the ports it listens on and a folder for what it stores. It shows the current operation, every status file and transfer in its activity log, and the files it has kept.</p>
</div>
<figure class="adl-shot"><img src="../../images/mock-serving.png" width="1280" height="1320" loading="lazy" alt="The Mock Target serving, with its identity, ports and storage settings, and its activity log."><figcaption>The Mock Target, serving.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Faults on request</h2>
<p>Each fault shows what a loading process does when a target is not well behaved. The window says how many are armed, and one button puts them all back.</p>
<ul class="adl-ticks">
<li>Answer WAIT, once or on every request.</li>
<li>Refuse Information, Upload or either Download.</li>
<li>Drop every Nth status file.</li>
<li>Corrupt the files it serves, or send a wrong check value.</li>
<li>Abort after N status files.</li>
<li>Go silent after the first status file.</li>
</ul>
</div>
<figure class="adl-shot"><img src="../../images/mock-faults.png" width="1280" height="1320" loading="lazy" alt="The Mock Target with one fault switched on: Refuse Upload."><figcaption>A fault switched on.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-intro">
<h2>Independent of the loader it tests</h2>
<p>The Mock Target's file transfer is a second implementation of TFTP, written separately from the engine's, so the two ends of a dialogue share no transfer code. The project's own tests use the same implementation for their simulated target.</p>
</div>
<ul class="icl-points">
<li><h3>Test benches</h3><p>Rehearse a loading procedure, or train people on it, with no equipment on the bench.</p></li>
<li><h3>Your own loader</h3><p>Point a loader you are building at it, and watch each failure path.</p></li>
<li><h3>Demonstrations</h3><p>Show the whole loading process on one computer, from a media set to a receipt, in about ten minutes.</p></li>
</ul>
<ul class="adl-links"><li><a href="../../solutions/#test-benches-and-ci">Test benches and CI</a></li><li><a href="../data-loader/">Data Loader</a></li></ul>
</div></section>
