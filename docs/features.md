---
template: marketing.html
title: Features
title_tag: "Features: ARINC Data Loader"
description: What the Data Loader, the Media Set Generator, the Mock Target and the ADL Library do, feature by feature, with a picture of each.
---
<section class="adl-head"><div class="icl-wrap">
<h1>Features</h1>
<p class="icl-lead">What each tool and the library do. Each product has a page of its own, and the guide has every detail.</p>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Check a media set before anything is sent</h2>
<p>The loader opens a media set as a folder. Reading it is the check: a set that cannot be read cannot be uploaded, and where the set departs from the standard's recommendations the loader shows a warning and carries on. If a file changes on disk after the check, the loader refuses to upload the set until it is checked again.</p>
<p><a href="../guide/choose-a-media-set/">Choose and check a media set</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/app-media-set-properties.png" width="1280" height="900" loading="lazy" alt="The properties of a media set in the loader: its part number, its load and its files."><figcaption>The properties of a media set, as the loader read it.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Find a target, or name one</h2>
<p>FIND broadcasts one request on the local network, listens for three seconds and lists the targets that answer, up to 64. You can also type a target's address and its identifier, which reaches a target that does not answer FIND.</p>
<p><a href="../guide/find-a-target/">Find a target</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/app-find-results.png" width="1280" height="900" loading="lazy" alt="The loader after a FIND that no target answered: the result says 0 targets answered."><figcaption>FIND on one computer finds nothing. The address is typed instead.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Information, upload and download</h2>
<p>The loader runs the three ARINC 615A operations. Upload sends the loads you tick, with progress for the whole job and for each load. Download works both ways the standard describes: you name the files, or the target lists them and you choose. Each operation can be aborted, has a log, and ends with a receipt.</p>
<p>Downloaded files go into a new folder, <code>DNLD_DATA_&lt;THW_ID_POS&gt;_&lt;number&gt;</code>, and an existing file is never overwritten.</p>
<p><a href="../guide/upload/">Upload</a>, <a href="../guide/download/">Download</a> and <a href="../guide/log-and-receipt/">the log and the receipt</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/app-upload-receipt.png" width="1280" height="900" loading="lazy" alt="The receipt of an upload: the outcome, how many files the target read, and the time it ended."><figcaption>The receipt of an upload.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Make a media set</h2>
<p>The generator turns files you choose into an ARINC 665 load and media set, with one to 255 members, and an optional batch file. It writes the folder tree the loader opens. It then reads the tree back and verifies it, and writes into place only after that, so a full disk or a cancel does not leave a half-written set.</p>
<p><a href="../guide/make-a-media-set/">Make a media set</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/gen-complete.png" width="1280" height="1200" loading="lazy" alt="The media set generator after it wrote and verified a media set."><figcaption>The generator, after it wrote and verified a set.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>A target that misbehaves on request</h2>
<p>The mock target answers FIND, Information, Upload and both Downloads over real UDP sockets, and keeps what you upload on disk. You can make it ask the loader to wait, refuse an operation, drop status files, send a wrong checksum, abort, or go silent, and watch what the loader does.</p>
<p><a href="../guide/mock-target/">Run the mock target</a> in the guide.</p>
</div>
<figure class="adl-shot"><img src="../images/mock-faults.png" width="1280" height="1320" loading="lazy" alt="The mock target with one fault switched on: Refuse Upload."><figcaption>The mock target with a fault switched on.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<h2>Also</h2>
<ul class="icl-points">
<li><h3>Eight languages</h3><p>English, French, Spanish, German, Yiddish, Hebrew, Simplified Chinese and Japanese. Hebrew and Yiddish mirror the whole window. Seven of the translations are machine drafts; review by speakers is on the roadmap.</p></li>
<li><h3>Light and dark</h3><p>The programs follow the system's colour scheme. A status is shown by a symbol and a word as well as by colour.</p></li>
<li><h3>The libraries</h3><p>A development package holds the headers, the static libraries and a CMake package, <code>adl</code>. Its interfaces are not stable before version 1.0. <a href="../developers/libraries/">Using the libraries</a>.</p></li>
<li><h3>Tested</h3><p>The tests run on Windows, Linux and macOS in continuous integration. Sixteen fuzz harnesses cover the code that reads bytes from outside. <a href="../developers/testing/">Testing</a> says what each tier holds down.</p></li>
</ul>
<p><a href="../products/">Products</a> has a page for each tool and the library, and <a href="../assurance/">Assurance</a> says how they are verified.</p>
</div></section>
