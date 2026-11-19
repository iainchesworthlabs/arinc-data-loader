---
template: marketing.html
title: Media Set Generator
title_tag: "Media Set Generator: build ARINC 665 load packages"
description: The Media Set Generator turns your data and support files into an ARINC 665 load and media set, with its header, lists and check values, and verifies it by reading it back.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Media Set Generator</p>
<h1>Build ARINC 665 load packages from your files</h1>
<p class="icl-lead">Choose the files, give the load and the set their part numbers, and the generator writes the media set: the header file, the data and support files, the three list files and the check values. Then it reads the set back and verifies it.</p>
<div class="icl-actions"><a class="icl-button" href="../../downloads/">Download</a><a class="icl-button icl-button--ghost" href="../../guide/make-a-media-set/">Make a media set</a></div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>A whole media set from one form</h2>
<p>The generator asks for the choices the standard leaves to you, such as names, part numbers and which check values to use, and enforces the rest. A set has one to 255 members, and the data files are spread over them. An optional batch file names the load for each target hardware identifier you list.</p>
<p>Check values can be an 8, 16 or 32-bit CRC, MD5 or SHA-1, chosen for each listed file, for the file list itself and for the whole load.</p>
</div>
<figure class="adl-shot"><img src="../../images/gen-ready.png" width="1280" height="1200" loading="lazy" alt="The Media Set Generator with its form filled in and the header chip saying READY."><figcaption>The generator, ready to generate.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-row adl-row--flip">
<div>
<h2>Verified before it is written into place</h2>
<p>The set is written to a temporary folder first. The generator then reads it back from disk, the way the loader does, and verifies every file and check value. Only then does the folder get its name, so a full disk or a cancel never leaves a half-written set, and an existing folder is never overwritten.</p>
<p>Anything that departs from what the standard recommends is counted in the result, with the clause.</p>
</div>
<figure class="adl-shot"><img src="../../images/gen-complete.png" width="1280" height="1200" loading="lazy" alt="The Media Set Generator after it wrote and verified a media set."><figcaption>Written and verified.</figcaption></figure>
</div>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-row">
<div>
<h2>Mistakes are caught on the form</h2>
<p>A part number in the wrong form, a file that cannot be read or a name the standard does not allow is shown where it is, before anything is written.</p>
<p><a href="../../guide/make-a-media-set/">Every field, and its rules</a></p>
</div>
<figure class="adl-shot"><img src="../../images/gen-errors.png" width="1280" height="1200" loading="lazy" alt="The Media Set Generator showing errors next to the fields that have them."><figcaption>Errors are shown next to their fields.</figcaption></figure>
</div>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>In your own build</h2>
<p>The generator is a window on the <a href="../library/">ADL Library</a>. A build pipeline can make the same media sets with no window: <code>build_media_set</code> takes a description of the loads and returns the members, and the media tree writes them to a folder.</p>
<p>The command-line program <code>adl_write_payloads</code>, in every package, writes sample media sets, including the ARINC 615 and Boeing 777 layouts of the standard's Appendix F, for trying other loaders on.</p>
<ul class="adl-links"><li><a href="../../developers/libraries/">Using the libraries</a></li><li><a href="../../guide/command-line/">The command line</a></li></ul>
</div>
<div>
<h2>Details</h2>
<div class="adl-table"><table>
<tbody>
<tr><th scope="row">Standard</th><td><p>ARINC 665-3: loads, batch files, media sets and their list files.</p></td></tr>
<tr><th scope="row">Members</th><td><p>1 to 255, written as the folder tree the Data Loader opens.</p></td></tr>
<tr><th scope="row">Check values</th><td><p>None, CRC-8, CRC-16, CRC-32, MD5 or SHA-1.</p></td></tr>
<tr><th scope="row">Systems</th><td><p>Windows, Linux and macOS, in the same package as the loader.</p></td></tr>
</tbody></table></div>
</div>
</div>
</div></section>
