# Questions and answers

This page is for engineers and technicians who are starting with these programs, and for programmers who are evaluating the code. It answers the questions the repository can answer, each in a short paragraph with a link to the page that has the detail.

## What it is and what has been checked

### Has it been used with real equipment?

No. The programs work together and are tested against simulated and mock targets, but nothing has been checked against real equipment or another vendor's tools. The ARINC 615A dialogues have not been exchanged with a real target or a third-party loader, and the media sets the generator writes have not been read by another tool. See [What has been checked](status.md) and [Where files go, and limits](user-guide/files-and-limits.md#what-has-not-been-checked).

### Is there a program that runs on the equipment being loaded?

No. The loader, the generator and `adl_write_payloads` are for the operator's computer, the source side. `adl_mock_target` stands in for a target in demonstrations and tests, and it is not one. The libraries build for a target's platform, but no program answers a loader and writes status files on real equipment. See [What these programs are, and are not](user-guide/concepts.md#what-these-programs-are-and-are-not) and [Building](building.md#what-cmake-builds).

### Which standards does it follow, and are they needed to use it?

The code follows ARINC Report 665-3 (Loadable Software Standards) and ARINC Report 615A-3 (Software Data Loader Using Ethernet Interface), and cites them by section. For TFTP it also follows RFC 1350 with the options of RFCs 2347 to 2349. The standards are not needed to use the programs; the user's guide explains the words you will meet. They are needed to follow the code's citations. They are not in the repository because they are documents of the publisher, Aeronautical Radio, Inc., under the publisher's terms, and most such standards may not be redistributed. `.gitignore` excludes the PDFs, so anyone who wants them puts their own copies in `docs/specs/`. See [Licensing](licensing.md#the-standards), [Where the standards go](specs/README.md) and [coverage of ARINC 665-3](arinc665-coverage.md) and [of ARINC 615A-3](arinc615a-coverage.md).

### What does the loader send on the network, and does anything go to the internet?

The user's guide says the programs send nothing except to the target, plus the FIND broadcast on the local network: no telemetry, no update check and no account. A web page opens only when the operator clicks the licence or repository link in About. The build agrees: the programs use only the Qt modules Core, Gui, Qml, Quick and QuickControls2, which include no Qt network module, and the only addresses the windows' code opens are those two About links and local folders. Tracy, the profiler whose client opens a network listener, is off by default and never in a release. The network traffic is UDP over IPv4. See [Where files go, and limits](user-guide/files-and-limits.md#where-files-go) and [Releasing](releasing.md#what-is-not-shipped-and-why).

### What is a media set, and what is a load?

A load is one loadable thing: a header file that describes it, with its data files (the payload) and optional support files. A media set is the folder tree that holds one or more loads, with one folder per member and the list files that say what is on each. See [What ARINC 615A and 665 are](user-guide/concepts.md) and [The media set on disk](file-reference.md#the-media-set-on-disk).

### What are the limits?

A media set has 1 to 255 members. The generator makes one load per media set and reads each file whole into memory. The loader reads sets with more than one load. Choosing or re-checking a set reads every file of it whole into memory, so the check needs memory for the whole set; during an upload the files are read from disk again, in blocks, as the target asks. The loader does one job at a time. The time-outs are fixed by the standard: 2 seconds and one retry for a TFTP packet, 13 seconds without a status file, and 3 seconds for FIND. See [Where files go, and limits](user-guide/files-and-limits.md#limits).

## Installing and running

### Which operating systems and packages are there?

The three programs are built for Windows, Linux and macOS with Qt 6.12. Windows is the platform used by hand. Linux and macOS build and run their tests in CI, and macOS has not been tried by hand. The release workflow builds a zip for Windows (x86-64), a tar.gz for Linux (x86-64) and a disk image for macOS (Apple silicon, macOS 14.4 or later), each with a development package. There is no installer, `.deb` or AppImage, and the packages are not code-signed for the operating system, so Windows and macOS warn the first time. The programs can also be built from source. See [Operating systems and RTOS ports](status.md#operating-systems-and-rtos-ports), [Releasing](releasing.md#what-a-release-contains) and [Installing](user-guide/installing.md).

### How do I check a download?

Every release carries `SHA512SUMS`, a PGP signature for every file, and a GitHub build-provenance attestation for every file the sums list. Import the project's release key, compare its fingerprint with the one in `SECURITY.md` and in [Verifying a release](verifying-releases.md#the-release-key) (the key is not on a keyserver), then check the signatures, the SHA-512 sums and the attestation. `tools/release/verify_release.sh` does all three and needs `gpg`, `sha512sum` and `gh`. If a check fails, do not run the file. See [Verifying a release](verifying-releases.md).

### Why do the loader and the mock target ask the firewall for access, and which ports do they use?

The programs talk over UDP, and the first time one listens on a network interface the operating system may ask whether to allow it. The programs never change a firewall setting themselves. The mock target listens on UDP 1001 (FIND), on 59 (the loader's requests) and, for each transfer, on a port from 40000 up. The loader listens on 1001 during a FIND, on its own control port (59 unless changed in Preferences) and, for each transfer, on a port the system picks. The loader always sends its requests to port 59 of the target. The generator listens on nothing. When the loader and the mock target run on one computer, tick "This computer only (127.0.0.1)" in the mock target and use 127.0.0.1 as the target's address: nothing then listens on the network, and Windows does not ask. On Linux, ports below 1024 need extra privilege. See [What the firewall asks](user-guide/installing.md#what-the-firewall-asks) and [A port is in use, or not allowed](user-guide/troubleshooting.md#a-port-is-in-use-or-not-allowed).

### How does the loader find a target, and what if FIND finds nothing?

FIND broadcasts one request on UDP port 1001, listens for three seconds, and lists every target that answers with its identity. Click one to fill in its address and `THW_ID_POS`. If nothing answers, type the address and `THW_ID_POS` by hand. That reaches a target that does not answer FIND, provided the target listens on UDP port 59 and the identifier is the one the target expects. FIND does not reach a target on the same computer, does not cross routers, can be blocked by a firewall, and cannot be used on AFDX, which has no broadcast. A computer with several network adapters may send the broadcast out of the wrong one. See [Find a target](user-guide/find-a-target.md) and [FIND finds nothing](user-guide/troubleshooting.md#find-finds-nothing).

### Where do the programs put files?

A media set goes in `<output folder>/<media set part number>/member_NNN`, and nothing is ever overwritten. A download goes in a new `DNLD_DATA_<THW_ID_POS>_<number>` folder inside the download folder, and an existing file is never overwritten. The mock target keeps an upload in its storage folder and serves downloads from `holds` inside it. The loader's log is not written to a file. See [Files the programs read and write](file-reference.md#folders-and-files-the-programs-create).

### Is there a command-line version of the loader?

The documents describe none. The loader, the generator and the mock target each open a window. `adl_write_payloads` is a command-line program that writes sample media sets. A program that needs the protocol without a window can use the libraries. See [Installing](user-guide/installing.md#getting-the-programs), [`adl_write_payloads`](file-reference.md#adl_write_payloads) and [Using the libraries](libraries.md).

## The libraries and the ports

### Can the libraries be used in another program?

Yes. Each release has a development package with the headers, static libraries and a CMake package called `adl`. It holds `adl::arinc665`, `adl::arinc615a`, `adl::arinc615a_runner`, `adl::media_tree` and `adl::platform`. The code is C++23. Before 1.0 the interfaces are not stable, and `find_package(adl 0.1 ...)` accepts the same minor version only. The ARINC 615A codec and engine hold no socket, thread, clock or file system, so a program can drive them with its own transport. See [Using the libraries](libraries.md) and [ARINC 615A engine](arinc615a-engine.md#sans-io).

### What are the Zephyr and FreeRTOS ports?

They are ports of `adl::platform` (files, memory, threads, clock, UDP and queues) to those two operating systems. They are experimental: they are built in CI, and the platform conformance suite and the 615A engine tests run on simulators (FreeRTOS's POSIX simulator port, Zephyr's `native_sim`, and Zephyr on QEMU). No board, real network link or flash has run them. Each is built by a project of its own, not by the top-level build or the development package. QNX, the two Green Hills INTEGRITY ports and VxWorks are further behind: unbuilt code, or only a design. See [Operating systems and RTOS ports](status.md#operating-systems-and-rtos-ports), [The FreeRTOS and Zephyr ports](platform-abstraction.md#the-freertos-and-zephyr-ports) and [Building](building.md#zephyr-and-freertos).

## Licence, languages and reports

### What is the licence?

The project is GPL-3.0-or-later (`LICENSE`, `vcpkg.json`). No separate licence is stated for the libraries. The programs also carry Qt 6.12, linked dynamically under the LGPL 3 (one module the deployment places is offered under the GPL 3), the Archivo and Noto Sans Hebrew typefaces under the SIL Open Font License 1.1, and, on Windows, the runtime files Qt's deployment tool places beside Qt. Each is listed with its terms in About, Licences and in `NOTICES.txt`. Catch2 (a test framework) and vcpkg (a build tool) are not linked into anything that is distributed. The standards are not under this licence. The licensing page is an inventory and not legal advice. See [Licensing](licensing.md) and the [LICENSE](../LICENSE).

### Why are the translations marked as unreviewed?

The loader, the generator and the mock target offer eight languages. Seven of them are machine-drafted first versions written without a translator, and no speaker of French, Spanish, German, Yiddish, Hebrew, Chinese or Japanese has read them. The tests check that every message has a translation of the right shape (placeholders, plural forms, markup). They do not check whether the wording is good. Hebrew and Yiddish, which have no established avionics vocabulary, most need review. A review is a pull request that edits the `.ts` file of the language. See [Languages and colour](user-guide/languages-and-colour.md#language) and [Translations](translations.md).

### Where do I report a bug?

Open an issue on the project's GitHub repository with the bug report form. It asks for the version (from About), the platform, what happened and what was expected, the steps to reproduce, and logs. The log text and the dialog are what a report needs. The smallest load or header file that shows the problem helps most. The project is maintained by one person, so there is no guaranteed response time. Feature requests have a form of their own, and for more than a small change it is worth opening an issue before a pull request. See [Troubleshooting](user-guide/troubleshooting.md), the [bug report form](../.github/ISSUE_TEMPLATE/bug_report.yml) and [CONTRIBUTING.md](../CONTRIBUTING.md).

### Where do I report a security problem?

Privately, not as a public issue: open the repository's Security tab and choose "Report a vulnerability". Without a GitHub account, open an issue that says only that there is a security matter, with no details, and the maintainer will arrange a private channel. Do not attach a load, media set or capture taken from a real aircraft or a customer's equipment. The policy counts crashes and memory faults from crafted data, the same from a message sent to the loader's control port, a file name or path that makes the loader read or write outside its folder, and flaws in the build and release files. The programs do not authenticate a target or encrypt anything, because ARINC 615A over TFTP has neither, and a report that depends on that is a limit of the protocol. See [SECURITY.md](../SECURITY.md).
