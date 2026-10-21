# Glossary

This page is for engineers and technicians who prepare and load software, and for programmers who evaluate the code. It lists the terms the documents use, says what each means here, and links to the page that explains it in full.

- **Abort.** The operator's request to stop an operation. The loader tells the target, and the operation ends when the target confirms, or after 13 seconds without hearing from it. See [Upload](user-guide/upload.md#while-it-runs).

- **Activity log.** The loader's list of what the target said and what the loader noticed, each line with its time. It is kept in memory only. See [The log, the receipt and failures](user-guide/log-and-receipt.md#the-activity-log).

- **AFDX.** A network that has no broadcast, so FIND cannot find a target on it. The address and `THW_ID_POS` are typed by hand. See [Find a target](user-guide/find-a-target.md#find-a-target-find).

- **ARINC 615A.** The standard for the conversation between a data loader and a target over a network. The two exchange small files by TFTP over UDP. The code follows ARINC Report 615A-3. See [What ARINC 615A and 665 are](user-guide/concepts.md#arinc-615a-how-a-load-gets-into-the-equipment) and [ARINC 615A-3 coverage](arinc615a-coverage.md).

- **ARINC 665.** The standard for how loadable software is packaged: the header, data and support files of a load, their check values, and the media set that carries them. The code follows ARINC Report 665-3. See [What ARINC 615A and 665 are](user-guide/concepts.md#arinc-665-how-a-load-is-packaged) and [ARINC 665-3 coverage](arinc665-coverage.md).

- **Batch file (`.LUB`).** A file that names loads for several targets, so that one operation can load more than one piece of equipment. The generator writes one if asked, named from the first three characters of its part number and `.LUB`. The loader shows how many batch files a set has and does not use them to run an operation. See [Make a media set](user-guide/make-a-media-set.md#03-batch).

- **`BATCHES.LUM`.** The list file of the batch files of a media set: for each, its part number, file name and member. A member has one only when the set has a batch. See [The list files](file-reference.md#the-list-files).

- **Check value.** A number worked out from a file's contents and used to show that the file arrived unchanged. ARINC 665 has five types, numbered 1 to 5: 8-bit CRC, 16-bit CRC, 32-bit CRC, MD5 and SHA-1. The log and the TFTP option call it a checksum. See [Check values and CRCs](file-reference.md#check-values-and-crcs).

- **Config file (`CONFIG.LDR`, `EXCONFIG.LDR`).** Two support files that ARINC 615 adds to a load. The standard's Appendix F lists them for loads that ARINC 615 and Boeing 777 loaders can load too, and the generator allows the `.LDR` extension for a support file. See [File names](user-guide/make-a-media-set.md#file-names).

- **Conformance suite.** The tests that run the same checks on the fake platform and on the real host of each platform, to show that the two behave alike. See [Testing](testing.md#the-fake-and-the-conformance-suite).

- **Control port.** UDP port 59 in the standard, on which the target takes the loader's requests. The loader has a control port of its own as well (set in Preferences, 59 unless changed), on which the target comes back to it; a loader and a target on one computer need different numbers. See [What ARINC 615A and 665 are](user-guide/concepts.md#words-this-guide-uses) and [Preferences](user-guide/preferences.md).

- **CRC.** A check value of 8, 16 or 32 bits worked out by a calculation that ARINC 665 fixes. Every file listed in `FILES.LUM` has a 16-bit CRC, whatever other check value is chosen. See [Check values and CRCs](file-reference.md#check-values-and-crcs).

- **Data file.** A file of the load's payload, the software itself. A load needs at least one, and `.LUP` is the recommended extension. See [Make a media set](user-guide/make-a-media-set.md#the-files).

- **Data loader.** The party that loads a target, in ARINC 615A. In these documents it is the program the operator uses (`adl_app`, called the loader) and the library code behind it. See [What ARINC 615A and 665 are](user-guide/concepts.md#arinc-615a-how-a-load-gets-into-the-equipment).

- **DLA, DLP.** Two layers of the standard that the coverage pages use. DLP is the data load protocol (the user's guide writes "data load protocol time-out"), and DLA is the application above it. In the code `DataLoader` is the DLP layer, and the DLA is its listener (`OperationListener`) and the caller. See [ARINC 615A-3 coverage](arinc615a-coverage.md#section-by-section).

- **`DNLD_DATA_<THW_ID_POS>_<number>`.** The folder a download writes into, made inside the download folder. `<number>` is the lowest free number from 1. See [The loader's download folder](file-reference.md#the-loaders-download-folder).

- **Download.** The operation that fetches files from a target. In the media defined mode the operator names the files, and in the operator defined mode the target lists what it can give and the operator picks. See [Download](user-guide/download.md).

- **Fake platform.** An in-memory stand-in for `adl::platform`, with a clock that moves only when told, a network and files. The tests use it so that no socket or real second is needed. See [Platform abstraction](platform-abstraction.md#the-fake-platform).

- **`FILES.LUM`.** The list file of every file on the media set except itself: for each, its name, folder, member and 16-bit CRC, and a check value if one was chosen. It has a check value of its own. See [The list files](file-reference.md#the-list-files).

- **FIND.** The request the loader broadcasts on the local network, asking which targets are there. Every target that hears it answers with its identity. It uses UDP port 1001 (the FIND port), and the loader listens for three seconds. See [Find a target](user-guide/find-a-target.md#find-a-target-find).

- **Finding.** The library's word for something in a media set that differs from what the standard recommends ("should"), but does not stop it being read. The loader shows findings as warnings. A "must" that is broken is an error instead. See [ARINC 665-3 coverage](arinc665-coverage.md) and [Choose and check a media set](user-guide/choose-a-media-set.md#a-set-that-passes).

- **Golden payloads.** Files that Python scripts in `tests/golden` write from the standard's tables, sharing no code with the library. The tests compare the library's output with them byte for byte. This catches a mistake in the C++ and not a misreading that both share. See [Testing](testing.md#golden-payloads).

- **Header file (`.LUH`).** The file that describes a load: its part number, the equipment it is for, and each of its other files with a check value. Its name begins with a manufacturer's code and ends in `.LUH`. See [What ARINC 615A and 665 are](user-guide/concepts.md#arinc-665-how-a-load-is-packaged).

- **Host.** In `adl::platform`, one object (`Host`) that gives a program the platform's services: clock, memory, files, threads, network and message queues. `make_host()` returns the real one, and the fake platform is the other. See [Platform abstraction](platform-abstraction.md#the-shape).

- **Information.** The operation in which the loader asks the target what it is and what it holds: its hardware and the software parts installed in it. It changes nothing on the target. See [Find a target](user-guide/find-a-target.md#read-information).

- **Load.** One loadable thing: a header file with its data files and its support files. See [What ARINC 615A and 665 are](user-guide/concepts.md#words-this-guide-uses).

- **Load check value.** The check value that the header file records for the whole load. The generator offers none, the three CRCs, MD5 and SHA-1. See [Make a media set](user-guide/make-a-media-set.md#02-load).

- **`LOADS.LUM`.** The list file of the loads of a media set: for each, its part number, header file name, member and target hardware IDs. Every member has one. See [The list files](file-reference.md#the-list-files).

- **Loopback.** The address 127.0.0.1: network traffic that stays on one computer. The demonstration and the `loopback.integration` test run the loader and the mock target over it. See [Installing](user-guide/installing.md#the-demonstration-needs-no-prompt).

- **Media set.** The folder tree that holds one or more loads: one folder per member, each with its list files. See [The media set on disk](file-reference.md#the-media-set-on-disk).

- **Media set member (`member_001`, ...).** One disk of a media set. On disk it is a folder called `member_` and three digits, `member_001` to `member_255`. See [The media set on disk](file-reference.md#the-media-set-on-disk).

- **Mock target.** `adl_mock_target`, a program that behaves as this project reads ARINC 615A, over real sockets. It is for practice, demonstrations and testing, and is not a real target. See [Run the mock target](user-guide/mock-target.md).

- **Part number.** The identity of a load, a batch or a media set. A load's or batch's has the form `MMMCC-SSSS-SSSS`, for example `ACM47-1234-5678`: a manufacturer's code, two check characters and a product identifier. A media set's is at most 15 characters. See [Make a media set](user-guide/make-a-media-set.md#the-rules-the-generator-enforces).

- **Part root directory.** The folder of a load on a member. It is named after the load's part number without its hyphens, for example `ACM4712345678`. See [The media set on disk](file-reference.md#the-media-set-on-disk).

- **Protocol files.** The twelve files that the loader and the target exchange in an ARINC 615A operation: `.LCI`, `.LUI`, `.LND` and `.LNO` (initialization), `.LCL`, `.LCS`, `.LUR`, `.LUS`, `.LNR`, `.LNS`, `.LNL` and `.LNA`. Each is named `<THW_ID_POS>` followed by its extension. See [The twelve ARINC 615A protocol files](file-reference.md#the-twelve-arinc-615a-protocol-files) and [ARINC 615A codec](arinc615a-codec.md#the-protocol-files-section-64).

- **Receipt.** The summary the loader shows after a transfer: the outcome, the detail, and the target, media set and loads involved. See [The log, the receipt and failures](user-guide/log-and-receipt.md#the-receipt).

- **Runner.** `adl::arinc615a_runner`, the thin code that drives the engine over the platform's sockets and clock. One thread runs it. See [ARINC 615A engine](arinc615a-engine.md#the-runner).

- **Sans-I/O.** Code that holds no thread, socket, clock or file system. It is told the time and the bytes that arrived, and answers with the bytes to send and the time it next wants to be called. The 615A codec and engine are written this way. See [ARINC 615A engine](arinc615a-engine.md#sans-io).

- **Simulated target.** In the tests, a 615A target that runs in memory on the fake platform (`tests/engine/sim_target.cpp`). It is not the mock target. See [Testing](testing.md#the-simulated-target).

- **Status file.** A file the target writes (`.LCS`, `.LUS` or `.LNS`) to say how an operation is going. The loader treats it as the target's heartbeat: it ends the operation if no status file arrives for 13 seconds (longer, if the target announced that it would be silent). See [ARINC 615A engine](arinc615a-engine.md#status-files-5451-64).

- **Support file.** An optional file that goes with a load, such as a configuration file. It is listed in the header file. See [Make a media set](user-guide/make-a-media-set.md#the-files).

- **Target.** The equipment being loaded, or the mock target that stands in for it. See [What ARINC 615A and 665 are](user-guide/concepts.md#words-this-guide-uses).

- **TFTP.** A simple file-transfer protocol, here over UDP (RFC 1350, with the options of RFCs 2347 to 2349). Every ARINC 615A exchange except FIND is a TFTP file transfer. See [ARINC 615A codec](arinc615a-codec.md#tftp-section-532).

- **`THW_ID_POS`.** The name a target goes by in file names: its hardware ID, an underscore and its position, for example `HNPFMS_L`. A target with no position is its hardware ID alone. The protocol files and the download folder are named with it, and the way the loader forms it from a FIND answer has not been confirmed with a real target. See [What ARINC 615A and 665 are](user-guide/concepts.md#words-this-guide-uses) and [Find a target](user-guide/find-a-target.md#find-a-target-find).

- **Upload.** The operation that sends one or more loads from the loader to the target. The standard calls this loading. The target reads the files it needs from the loader and reports its progress in status files. See [Upload](user-guide/upload.md).

- **WAIT.** A target's answer to a request that means "not yet, ask again in so many seconds". It is a TFTP error packet whose text starts with `WAIT:`. See [ARINC 615A codec](arinc615a-codec.md#tftp-section-532).
