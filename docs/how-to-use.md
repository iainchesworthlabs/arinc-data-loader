---
template: marketing.html
title: How to use
title_tag: "How to use: ARINC Data Loader"
description: Three steps take a media set from files on your disk to an upload and a download, on one computer, with no hardware.
---
<section class="adl-head"><div class="icl-wrap">
<h1>How to use ARINC Data Loader</h1>
<p class="icl-lead">Three steps, on one computer, with no hardware. The guide has the rest.</p>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Step 1: Make a media set</h2>
<p>Start <strong>ARINC Media Set Generator</strong>. Give the media set and the load their part numbers, add the files, and say which are data files and which are support files. When the header chip says READY, press <strong>Generate media set</strong>.</p>
<p>The generator writes the set to the folder you chose, reads it back and verifies it. A banner then says the set was written and verified.</p>
<p><a href="../guide/make-a-media-set/">Make a media set</a> in the guide has every field.</p>
</div>
<figure class="adl-shot"><img src="../images/gen-ready.png" width="1280" height="1200" loading="lazy" alt="The media set generator with its form filled in and the header chip saying READY."><figcaption>The generator, ready to generate.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Step 2: Start a target</h2>
<p>Start <strong>ARINC Mock Target</strong>, choose a folder where it keeps what you upload, and press <strong>Start</strong>. The chip says SERVING.</p>
<p>To keep the firewall from asking, tick <strong>This computer only (127.0.0.1)</strong> before you start it. With a real target on your network, you need its address instead, and nothing has been checked against one.</p>
<p><a href="../guide/mock-target/">Run the mock target</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/mock-serving.png" width="1280" height="1320" loading="lazy" alt="The mock target serving: the chip says SERVING."><figcaption>The mock target, serving.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Step 3: Load and fetch</h2>
<ol>
<li>Start <strong>ARINC Data Loader</strong> and press <strong>Start loading</strong> on the first screen.</li>
<li>Type the target's address and its identifier. On one computer FIND does not reach the mock target. If the mock target is on the same computer, set the loader's control port to another number, such as 5900, in Preferences.</li>
<li>Choose the media set folder. The loader reads and checks it.</li>
<li>Tick the load and press <strong>Upload</strong>. The header says COMPLETE and the load is at 100 percent.</li>
<li>Switch to <strong>Download</strong>, name some files or let the target list them, and press <strong>Download</strong>. The files arrive in a new folder.</li>
</ol>
<p><a href="../guide/demonstration/">The demonstration</a> does all of this, with a picture of every screen, and then makes the target refuse an upload so you can see a failure.</p>
</div>
<figure class="adl-shot"><img src="../images/app-upload-complete.png" width="1280" height="900" loading="lazy" alt="The data loader after an upload to the mock target: the header says COMPLETE."><figcaption>The data loader after an upload.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>Going further</h2>
<ul class="adl-cards">
<li><a href="../guide/">The guide</a><p>By task, with the reference pages.</p></li>
<li><a href="../guide/troubleshooting/">Troubleshooting</a><p>When the target does not answer, or a port is in use.</p></li>
<li><a href="../guide/what-has-been-checked/">What has been checked</a><p>Before you rely on it for anything that matters.</p></li>
<li><a href="../developers/libraries/">The libraries</a><p>To use the code in a program of your own.</p></li>
</ul>
</div></section>
