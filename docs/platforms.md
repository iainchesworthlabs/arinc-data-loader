---
template: marketing.html
title: Platforms
title_tag: "Platforms: ARINC Data Loader"
description: Where the ADL Library and the tools run, what has run on each platform, and how a port to another operating system is made and checked.
---
<section class="adl-head"><div class="icl-wrap">
<p class="icl-eyebrow">Platforms</p>
<h1>One library, many operating systems</h1>
<p class="icl-lead">Everything that differs between operating systems sits behind one platform layer. The tools run on the three desktop systems; the library also runs on real-time operating systems, and a port to yours is a folder of implementations, checked by the same conformance suite.</p>
</div></section>

<section class="icl-section icl-section--tint"><div class="icl-wrap">
<div class="adl-table"><table>
<caption>Where each part runs</caption>
<thead><tr><th scope="col">Platform</th><th scope="col">Status</th><th scope="col">What has run</th></tr></thead>
<tbody>
<tr><th scope="row">Windows (x86-64)</th><td><span class="adl-chip adl-chip--ok">Supported</span></td><td><p>The library and the tools. Built warning-free with MSVC and tested under AddressSanitizer on every change; the release build is tested, packaged and started from its package.</p></td></tr>
<tr><th scope="row">Linux (x86-64)</th><td><span class="adl-chip adl-chip--ok">Supported</span></td><td><p>The library and the tools. Built with GCC and tested under AddressSanitizer and UBSan on every change, with the coverage gate; the release build is tested and packaged.</p></td></tr>
<tr><th scope="row">macOS (Apple silicon)</th><td><span class="adl-chip adl-chip--ok">Supported</span></td><td><p>The library and the tools, macOS 14.4 or later. Built with AppleClang and tested under AddressSanitizer and UBSan on every change; the release build is tested and packaged.</p></td></tr>
<tr><th scope="row">FreeRTOS</th><td><span class="adl-chip adl-chip--exp">Experimental</span></td><td><p>The library, on FreeRTOS-Kernel V11.2.0 with lwIP 2.2.1 and littlefs. The platform conformance suite and the ARINC 615A engine tests, 227 test cases, run in CI on the kernel's POSIX simulator port.</p></td></tr>
<tr><th scope="row">Zephyr</th><td><span class="adl-chip adl-chip--exp">Experimental</span></td><td><p>The library, on Zephyr 4.4.2 with its own network stack and littlefs. The same 227 test cases run in CI on <code>native_sim</code> and, with the Zephyr SDK's own toolchain, on a Cortex-A9 in QEMU; a smoke program runs on a Cortex-M3 with no exceptions or RTTI.</p></td></tr>
<tr><th scope="row">QNX Neutrino</th><td><span class="adl-chip adl-chip--port">Port written</span></td><td><p>The platform layer is written, reusing the POSIX code. It waits for its first build.</p></td></tr>
<tr><th scope="row">Green Hills INTEGRITY 5.0.8</th><td><span class="adl-chip adl-chip--port">Port written</span></td><td><p>The platform layer is written. It waits for its first build with a Green Hills toolchain.</p></td></tr>
<tr><th scope="row">Green Hills INTEGRITY-178B</th><td><span class="adl-chip adl-chip--port">Port written</span></td><td><p>The platform layer is written for the 178B profile, where starting a thread at run time and the file system are left out on purpose. It waits for its first build.</p></td></tr>
<tr><th scope="row">VxWorks</th><td><span class="adl-chip adl-chip--plan">Planned</span></td><td><p>A design. A build for it is refused with an explanation.</p></td></tr>
</tbody></table></div>
<p class="icl-muted"><strong>Supported:</strong> built and tested on every change, and released. <strong>Experimental:</strong> built in CI, with the test suite passing on a simulator. <strong>Port written:</strong> code that has not yet been compiled; the build refuses it unless asked. <strong>Planned:</strong> no code yet. A port moves up only when the step before it has run. <a href="../guide/what-has-been-checked/#operating-systems-and-rtos-ports">What has been checked</a> has the detail.</p>
</div></section>

<section class="icl-section"><div class="icl-wrap">
<div class="adl-two">
<div>
<h2>What a platform has to give</h2>
<p>The platform layer is four areas of interfaces. A port implements each one for its system, in a folder of its own. The system is chosen by CMake; the code has no <code>#ifdef</code>, and a check holds that count at zero.</p>
<ul class="adl-ticks">
<li><strong>Files and storage:</strong> read and write files and folders, or say that the system has none.</li>
<li><strong>Memory:</strong> an arena and a pool, sized up front.</li>
<li><strong>Threads, priority and time:</strong> threads at named priorities, locks, semaphores and a monotonic clock.</li>
<li><strong>Networking and messages:</strong> UDP sockets over IPv4 and message queues.</li>
</ul>
</div>
<div>
<h2>How a port is checked</h2>
<p>The conformance suite needs only the interfaces. It runs every check against the port's real implementation and against the fake platform, so the two are held to each other. The ARINC 615A engine tests then run on top, the same sources the desktops run.</p>
<ol class="adl-steps">
<li><h3>Write it</h3><p>Implement the interfaces in a new folder, reusing the POSIX code where the system is POSIX.</p></li>
<li><h3>Run the suite</h3><p>Build the conformance suite and the engine tests for the system and run them on a simulator or a board.</p></li>
<li><h3>Raise its status</h3><p>Experimental once CI runs the suite on a simulator; supported once it runs on the target.</p></li>
</ol>
</div>
</div>
<ul class="adl-links"><li><a href="../developers/platform-abstraction/">Platform abstraction</a></li><li><a href="../developers/platform-isolation/">Platform isolation</a></li><li><a href="../developers/building/">Building</a></li></ul>
</div></section>

<section class="icl-section icl-section--tint icl-cta"><div class="icl-wrap">
<h2>Your operating system is not listed?</h2>
<p class="icl-lead">The platform layer is the only part that changes. Tell us which system and toolchain you use.</p>
<!-- OWNER DECISION: whether a port to a customer's platform is offered, in which edition, and on what terms. -->
<div class="icl-actions"><a class="icl-button" href="../support/#talk-to-us">Talk to us</a><a href="../developers/platform-abstraction/#adding-a-port">Adding a port</a></div>
</div></section>
