# ARINC 615A-3 coverage

This page reviews `docs/specs/615A-3.pdf` against `adl::arinc615a` (the [codec](arinc615a-codec.md) and
the [engine](arinc615a-engine.md)) and `adl::arinc615a_runner`: what each section asks for, where the code
does it, and which tests hold it down. It is also the list of places the standard is unclear, because those are
where an interoperability test against a real loader or target matters most. It is in the form of [the 665
page](arinc665-coverage.md). Section numbers are those of the 615A-3 edition in `docs/specs/`.

The code is the **data loader's** half of the protocol. A target's half (answering `.LCI`, writing status
files) exists only as the simulated target of the tests.

## In short

- Everything the standard asks of a data loader in sections 5.3 to 6.4 and Attachments 2 to 4 is built: the
  TFTP of 5.3.2 with its options, WAIT and ABORT; FIND; the Information, Upload (short and full load) and
  Download (media defined and operator defined) operations with the status files, the time-outs and retries of
  Attachment 4, the abort paths of 5.4.5.2 and the protocol files of 6.4.
- It is **sans-I/O**: the state machines hold no socket, thread, clock or file system, work on bytes, allocate
  nothing, throw nothing and need no RTTI, so they can run on an RTOS and be tested without a network. The
  runner is the only code that calls `adl::platform`.
- A full load of a media set built with `adl::arinc665::build_media_set` goes to a simulated target, and
  files come back in both download modes, over a clean network and over one that loses, repeats and reorders
  datagrams, with deterministic seeds. Many of the tests deliberately break the protocol: a target that
  waits, goes silent, aborts, loses or repeats status files, sends a wrong checksum, announces an exception
  timer, refuses options, speaks another protocol version or writes a file that was not asked for.
- **Nothing has been checked against another implementation**, a loader or a target, and no test runs an
  operation over real sockets (see [Not yet verified](#not-yet-verified)). The simulated target's TFTP is a
  second, independent implementation, but both are my reading of the standard.
- Not built: hardware (sections 1 to 4, Attachments 1 and 5), and anything on the target's side.

## Section by section

| Section | What the standard asks | Where | Tests |
| --- | --- | --- | --- |
| 5.1, 5.2 | Introduction and scope: the data loader and the target, on Ethernet or AFDX | the library is the loader; the transport is UDP/IPv4 through `adl::platform` | n/a |
| 5.3.1 | Layers: application, load protocol (DLP and THP), TFTP, UDP, IP | `DataLoader` is the DLP layer, `TftpTransfer` the TFTP layer, `Runner` and `adl::platform` the rest; the DLA is the `OperationListener` and the caller | `data_loader_test`, `operations_test` |
| 5.3.2, 5.3.2.1 | All exchanges but FIND are TFTP file transfers (RFCs 1123, 1350, 2347 to 2349, 1785) | `TftpTransfer`, `tftp.hpp` | `tftp_transfer_test`, `tftp_test`, `tftp_options_test` |
| 5.3.2.2 | Options may be used, a transfer does not fail for one that is not implemented; octet mode only; no UDP data after the last option | an OACK that leaves an option out is taken as defaults; a request that is refused with error 8 is repeated without options; the codec writes and reads octet mode only and refuses bytes after the options | `tftp_transfer_test` ("the block size we ask for", "an OACK that breaks the rules"), `data_loader_test` ("a target that refuses options is asked again without them"), `tftp_test` |
| 5.3.2.3.1 | 615A messages are carried in error packets with code 0 | `interpret_error` | `tftp_options_test` |
| 5.3.2.3.2 | Port 59 for all 615A TFTP | `tftp_control_port`; the loader's control port and the target's request port (`LoaderConfig::target`) | `data_loader_test` ("the port option") |
| 5.3.2.3.3 | Error codes 0 to 8; code 3 when the loader has no resources for a new TFTP process | refusals use 1 (no such file), 2 (not allowed), 3 (no slot), 4 (illegal), 6 (exists), 8 (options); a peer's code is reported | `operations_test` ("every transfer is in use", "files that are not part of the operation"), `tftp_transfer_test` |
| 5.3.2.3.4 | WAIT: `WAIT:x`, x up to 65535 s; the receiver aborts the transfer and starts a new one after x | `WaitMessage`; a transfer answered with it ends `wait_requested`; the loader asks again after x and does not count it as a retry | `tftp_transfer_test` ("a WAIT answer"), `data_loader_test` ("a WAIT answer to the first request", "a target that keeps saying WAIT"), `tftp_options_test` (all 65 536 values) |
| 5.3.2.3.5 | WAIT only in answer to a connection request | a WAIT in the middle of a transfer is an ordinary error | `tftp_transfer_test` ("a WAIT in the middle") |
| 5.3.2.3.6, 5.3.2.3.7 | ABORT: `ABORT:XXXX`, four hexadecimal characters of the status code, sent in answer to a request; used by the DLP on an abort request and on problems it detects | `AbortMessage`; sent as the answer to the target's status file request and to other requests, and on open transfers | `tftp_transfer_test` ("an ABORT from the peer", "our own abort"), `operations_test` ("an abort is answered"), `data_loader_test` (the abort tests) |
| 5.3.2.3.8, .8.1 | Options may be implemented, unknown ones are ignored; the loader should implement the block size option | `blksize`: asked for (1024), taken down by the server, 8 to 65 464; a request's option is answered in the OACK | `tftp_transfer_test` ("a file is read whatever its size and block size", "the largest block size works"), `tftp_options_test` |
| 5.3.2.3.8.2 | If the transfer size is given and does not match, the transfer fails | `size_mismatch` for a received file; a write announces its size, a read answer carries the file's | `tftp_transfer_test` ("a transfer size that is not what arrives fails the transfer", "a read request's size") |
| 5.3.2.3.8.3 | The timeout option may be used | taken from a peer's request, 1 to 255 s, and used for our retransmissions | `tftp_transfer_test` ("the timeout option the peer asks for") |
| 5.3.2.3.8.4 | "Part Number" option | **there is none**: the text of 5.3.2.3.8.4 repeats the Timeout paragraph; nothing implemented | n/a (see [below](#places-the-standard-is-unclear)) |
| 5.3.2.3.8.5 | `checksum_<n>` on a read request names a file among two of one name (the 16-bit CRC from the header); on a write it gives a check value of the file the loader should verify, must not fail the transfer, and the file is handled the same either way | read: handed to the `FileSource`; write: acknowledged if the algorithm can be calculated, calculated while the blocks arrive, verdict reported, file kept | `tftp_transfer_test` ("a checksum is verified", "every algorithm", "a read request's 16-bit CRC"), `data_loader_test` ("two files of one name", "a wrong checksum is reported and the file is kept") |
| 5.3.2.3.8.6 | `port` option, on an operation's initialization request only, moves the loader's control port for the operation; the target's stays 59; a refusal may fail the operation | sent with the initialization request when the loader's port is not 59; not taken is a notice, and the time-out ends the operation | `data_loader_test` ("the port option moves", "the default control port", "a target that ignores the port option"), `operations_test` ("the first thing sent") |
| 5.3.2.3.9 | Block numbers 16 bits, first is 1, a WRQ is acknowledged with 0, after 65535 comes 1 | `next_block`, used by both directions | `tftp_transfer_test` ("block numbers go 65535 and then 1", written across too), `tftp_test` |
| 5.3.3, Attachment 3 | FIND: one broadcast request on port 1001, 3 s window, answers registered, invalid ones ignored | `FindClient`, `Runner::find` | `find_client_test`, `find_test` (codec) |
| 5.3.4 | `THW_ID_POS`: the identifier, an underscore and 0 to 8 characters | the loader is configured with it (`thw_id_pos`) and names the protocol files with it; a name with a path in it is refused | `operations_test` ("a request that cannot be made") |
| 5.4.1 | Operations are independent and serial; one completes before another starts for a target; any may be aborted | one operation per `DataLoader`; `abort()` | `operations_test`, `data_loader_test` (the abort tests) |
| 5.4.1 | A target that does not support an operation answers its initialization with 0x1002 | `OutcomeCode::not_supported`; 0x1000 is `denied` | `data_loader_test` ("a target that denies or does not support an operation") |
| 5.4.2 | Information: `.LCI` read, `.LCL` and status files written by the target; options may give an integrity check | `DataLoader` with `InformationOperation` | `data_loader_test` ("an Information operation ...", both forms) |
| 5.4.3 | Upload: `.LUI` read, `.LUS` written, `.LUR` written by the loader after status 0x0001, then the target reads the files; the loader checks nothing for it | `UploadOperation`; `FileSource` | `data_loader_test` ("a load of a media set is uploaded", "a large load", "a file that is not there yet", "a file the source will not give") |
| 5.4.3.1, 5.4.3.2 | Short load and full load are the same protocol: the target decides which files it asks for | nothing to do for either | `data_loader_test` ("a short load is the target asking for fewer files") |
| 5.4.4, 5.4.4.1 | Media defined download: `.LND` read, the list `.LNR` written by the loader, the target writes the files | `MediaDownloadOperation`; `FileSink`; the expected files are the ones listed | `data_loader_test` ("a media defined download", "a file nobody asked for is refused", "files missing at the end are reported") |
| 5.4.4, 5.4.4.2 | Operator defined download: `.LNO` read, `.LNL` written by the target, `.LNA` by the loader | `OperatorDownloadOperation`; `OperationListener::on_download_list`, `FileSelection`, `provide_selection` | `data_loader_test` ("an operator defined download ...", "whose choice is made later", "an operator who chooses nothing") |
| 5.4.4 | Checksum of downloaded data is for information and does not stop the export | as 5.3.2.3.8.5 | `data_loader_test` ("a wrong checksum is reported and the file is kept") |
| 5.4.4.3 | The download directory `DNLD_DATA_<THW_ID_POS>_<number>`, `DNLD_INFO_...` | **not in the engine**: it is the application's with the sink; `DirectoryFileSink` writes to the directory it is given | `directory_files_test` |
| 5.4.5.1 | Status messages are the heartbeat, the abort, and the close of an operation | `on_*_status` of the listener; the DLP time-out; 0x0003 completes | `data_loader_test` ("thirteen seconds without a status file", "an Information operation with status files") |
| 5.4.5.2 | Interruptions: the loader answers the target's status file write request with ABORT (codes 0x1004 or 0x1005) and the target echoes the code in its status file; the target aborts with 0x1003 | `DataLoader::abort`, `abort_status`; the target's 0x1003 | `data_loader_test` ("the operator's abort stops an upload", "an abort before the target has accepted", "an abort the target never confirms", "a target's own abort", "a download is aborted"), `operations_test` |
| 6.1, 6.2 | The messages between the DLA and the DLP, and the protocol files each uses | `OperationListener` callbacks and `DataLoader`'s inputs, one for each message: [TH_Information_Initialization] `start`, [Information_Initialization_Response] `on_initialization`, [TH_Information] `on_configuration_list`, [Information_Status] `on_configuration_status`, [Load_List] the `UploadOperation`, [File_Not_Available] `Notice::file_not_available`, [Upload_Information_Status] `on_upload_status`, [Downloading_Media_Initialization] and [..._Operator_...] the two download operations, [Downloading_File_Receipt] `on_file_received`, [Downloading_Information_Status] `on_download_status`, [Downloading_File_List_Receipt] and [File_Selection] `on_download_list`, [Abort_Request] `abort` | `data_loader_test` |
| 6.2.1.2 etc. | A WAIT in answer to the initialization read: the DLP retries after the wait | as 5.3.2.3.4 | `data_loader_test` |
| 6.2.7.3 | The `.LUR` write starts after the `.LUS` that says "accepted, not yet started" | `apply_status` sends the list at 0x0001; the simulated target refuses a list that comes before | `data_loader_test` (every upload, and the mutation checks, see [testing](testing.md)) |
| 6.2.17 | On an abort request the loader refuses the target's next status file request with ABORT, and the target then writes a status file with 0x1004 or 0x1005 | as 5.4.5.2 | as 5.4.5.2 |
| 6.3.1 | Information chart; "any invalid structure is a fatal error"; the status file at least once within the DLP time-out; WAIT lets the target reboot; the exception timer; with a fast `.LCL` the target may send no status; otherwise the last status file is 0x0003 | `DataLoader` | `data_loader_test` (the Information tests, the status and time-out tests), `operations_test` |
| 6.3.2 | Upload chart; the loader is a TFTP server "without any hypothesis in the sequence" of requests; several reads at a time; file not available means WAIT | up to 8 slots; `FileSource::open` `not_available` gives `WAIT:x` | `data_loader_test` ("a file that is not there yet ..."), `operations_test` ("every transfer is in use") |
| 6.3.3 | Media defined chart; the status 0x0001 before `.LNR` | as 6.2.7.3 | `data_loader_test` |
| 6.3.4 | Operator defined chart | as 5.4.4.2 | `data_loader_test` |
| 6.3.5 | Interruption service | as 5.4.5.2 | as 5.4.5.2 |
| 6.4 | Protocol version `A4`: the loader adapts or aborts at the first status file request | `peek_protocol_version`; `unsupported_protocol_version` | `data_loader_test` ("a protocol version that is not A4"), `operations_test` |
| 6.4.1 to 6.4.9 | The twelve protocol files | the [codec](arinc615a-codec.md) | `protocol_files_test`, `arinc615a_golden_test`, `arinc615a_robustness_test` |
| 6.4.3 to 6.4.5, 6.4.7 | The status counter starts at 0, goes up by one, wraps; "to detect the duplication of status files or the loss" | `take_counter`: repeats and out-of-order files are not shown again, gaps are counted | `operations_test` ("status files out of order", "the counter wraps"), `data_loader_test` ("a lost status file", "a status file that arrives twice") |
| 6.4.3 to 6.4.5, 6.4.7 | The exception timer: seconds of silence, only with status 2 and 4 | the window is `max(13 s, timer)`; `exception_timer_expired` | `operations_test` ("an exception timer longer than thirteen seconds"), `data_loader_test` (both exception tests) |
| 6.4.10 | The status code table | `StatusCode`; each code's outcome in `apply_status` | `data_loader_test`, `operations_test` |
| Att. 2 | Load scenarios (configuration query, upload, disk defined and operator defined downloads) | the four operations | `data_loader_test` |
| Att. 3 | FIND packets and the 3 s window, 2 s answer | the codec and `FindClient` | `find_client_test`, `find_test` (codec) |
| Att. 4, 4-3.1 | TFTP time-out 2 s | `tftp_timeout`; every exchange | `tftp_transfer_test` ("a request nobody answers is sent twice and then fails after four seconds", "a receiver that waits"), `operations_test` ("the request is repeated after two seconds") |
| Att. 4, 4-3.2 | TFTP retry number 1: one more try of each exchange, no accumulation across exchanges | `tftp_retries`; the count restarts at each exchange | `tftp_transfer_test` ("lost ACKs are covered by the retry") |
| Att. 4, 4-3.3 | Sorcerer's Apprentice: never resend the current data packet on a duplicate ACK | `on_ack` | `tftp_transfer_test` ("duplicated datagrams never make a sender resend a block", "a duplicate ACK is ignored by the sender") |
| Att. 4, 4-4.1 | DLP time-out 13 s between the listed transfers (`.LCI→.LCS`, `.LCS→.LCS`, `.LUI→.LUS`, `.LUS→.LUS`, `.LNI→.LNS`, `.LNS→.LNS`); fatal, no retry; `.LCI→.LCL` allowed | `DataLoader::arm_watchdog` and the suspension rules, with `.LNI` read as `.LND` and `.LNO` | `operations_test` (to the nanosecond), `data_loader_test` (the time-out tests) |
| Att. 4, 4-4.2 | DLP retry number 1, for every file, a whole file transfer again; no cumulative count | `on_client_finished` | `operations_test` ("the request is repeated after two seconds and the transfer retried once"), `data_loader_test` ("a target that never answers the first request ...") |
| App. E | AFDX has no broadcast: FIND by unicast | `FindConfig::destinations` | `find_client_test` ("FIND to listed addresses") |
| App. F, G | File transfer and TFTP examples | the exchanges of F are the Upload; G's figures contain no bytes (see below) | `data_loader_test` |

## Places the standard is unclear

The choices are mine. Each is isolated enough to change in one place, and each is a reason to check against a
real device. [The engine page](arinc615a-engine.md) says how each is resolved; the list of the codec's
([here](arinc615a-codec.md#places-the-standard-is-unclear)) is separate.

1. **Attachment 4 lists `.LNI`**, which is not a protocol file (Table 6.4-1 has `.LND` and `.LNO`). Read as
   those two. `.LND→.LNS` and `.LNO→.LNS` (or `.LNO→.LNL` in the operator defined mode) get the 13 seconds.
2. **The table of DLP time-outs leaves out several transfers**: `.LCI→.LCL` (6.3.1 allows it), the loader's
   own `.LUR`, `.LNR` and `.LNA`, and the target's `.LCL` and `.LNL`. After each the same 13 seconds are
   applied. The time-out does not run during a status file's or a list's transfer, and does not stop for a
   data file's.
3. **5.3.2.3.8.4 (Part Number) and the first paragraph of 5.3.2.3.8.5 repeat the Timeout paragraph**, so
   there is no Part Number option, and the Checksum option is defined by the text that follows (also in
   [the codec's list](arinc615a-codec.md#places-the-standard-is-unclear)).
4. **What "Initial value is 0x0000" and "incremented before sending" mean for the first status file**: 0 or
   1. Both are accepted; a larger number counts as lost files.
5. **When status 0x0003 is final** relative to the transfer of a list or a file: it is called "the last
   file sent", but a transfer may still be open. The loader waits for those to end. An Information operation
   that completes with no list is a protocol error.
6. **What informs the target of an abort**: 6.2.17 has it answer the status file request, 6.3.5 has *any
   other* transfer aborted with the message as well. Either informs the target; the status file after that is
   its confirmation.
7. **How many WAITs**: the standard sets no limit. The loader gives up after 10.
8. **Where an error to a refused request comes from**: the control port, where the request was sent (RFC 1350
   would have it from a new transfer ID).
9. **A stray datagram** (a transfer ID that is not the peer's): ignored. RFC 1350 would answer with error 5.
10. **The dally**: "optional" in 5.4.3.1, done (two seconds) after the last ACK of a received file.
11. **The block size** the loader asks for (1024) and allows (4096) is not set by the standard.
12. **The `port` option**: the standard says "should" be 49152 or more (the codec takes any port); a target
    that does not acknowledge it is noted, not failed.
13. **Appendix G is figures with no packet bytes**, so the TFTP layouts and options are checked against the
    RFCs and the standard's own WAIT and FIND examples only.
14. **The displayed text** of Table 6.4.10-1 is the application's; the engine reports codes.

## Not yet verified

- **Against any other implementation of 615A**, a data loader or a target. The simulated target is built from
  the same reading of the standard, with its own TFTP code. The choices above are where a real target is likeliest
  to differ; 1, 2, 4 and the text-length choice of the codec are the first to confirm.
- **Over real sockets, end to end.** The runner is tested on the fake platform only; the real platforms'
  sockets are covered by `adl::platform`'s conformance suite, but no test runs an operation over loopback or a
  network. macOS has not run any of it.
- **Under `-fno-exceptions -fno-rtti` in CI.** The library sources compile that way under g++ 15 with the
  repository's warnings as errors, and the objects hold no reference to `typeinfo` or to a throw (checked once by
  hand); there is no CI job.
- **Timing on a real-time system.** The time-outs are tested on a clock that moves only when told to; how they
  behave on a desktop that can be late is unmeasured (the loader's 2 and 13 second figures are generous to it).
