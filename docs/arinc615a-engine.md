# ARINC 615A engine

`adl::arinc615a` (`src/arinc615a`) is, beside the [codec](arinc615a-codec.md), the data loader's side of
ARINC 615A-3: the TFTP transfer, the Information, Upload and Download operations of section 5.4, the
time-outs and retries of Attachment 4, and FIND (Attachment 3). `adl::arinc615a_runner`
(`src/arinc615a_runner`) is the thin layer that drives it over `adl::platform`. This page says how it is
built, what the standard leaves unclear and what was chosen, and what has and has not been checked.
[The coverage page](arinc615a-coverage.md) maps the standard onto it section by section. Section numbers
are 615A-3's, which is not in the repository ([docs/specs](specs/README.md)).

This is the loader's half of the protocol, which is what a ground tool or an airborne data loader needs.
The library has no target side: the targets that exist are the simulated target of `tests/engine` and the
mock target application ([the mock target](mock-target.md)), both for tests and demonstrations. The Qt
application, [the application](app.md), is a window over this engine.

## Shape

| Header | What it holds |
| --- | --- |
| `time.hpp`, `endpoint.hpp` | `Instant`, `Duration`, `Endpoint`: the engine's own small types for time and addresses (same representation as the platform's), so that it includes nothing of the platform |
| `tftp_transfer.hpp` | `TftpTransfer`: one file transfer, in both roles and both directions, as a state machine |
| `checksum.hpp` | `ChecksumVerifier`: the `checksum_<n>` option checked as the bytes arrive |
| `file_io.hpp` | `FileSource` and `FileSink`: where an upload's files come from and a download's go; `IoResult` |
| `data_loader.hpp` | `DataLoader`: an operation against one target; `Operation`, `OperationListener`, `Outcome`, `Command`, `LoaderConfig` |
| `find_client.hpp` | `FindClient`: the broadcast request and the three second window |
| `limits.hpp` | adds the figures of Attachment 4 and 3: `tftp_timeout` 2 s, `tftp_retries` 1, `dlp_timeout` 13 s, `dlp_retries` 1, `tftp_dally` 2 s, `find_window` 3 s |
| `runner.hpp` (runner library) | `Runner`: sockets, the wait, the clock, `RunController` |
| `directory_files.hpp` (runner library) | `DirectoryFileSource` and `DirectoryFileSink` over a directory of the platform's files |

A second TFTP implementation, `adl::arinc615a_mini` (`src/arinc615a_mini`), is not part of the engine. It is
written separately from it, on the codec only, and is the TFTP of the simulated target of the tests and of the
mock target application.

### Sans-I/O

The core holds no thread, socket, clock or file system. A state machine is told the time and the bytes
that arrived and answers with the bytes to send and the time it next wants to be woken:

```cpp
loader.on_control_datagram(now, from, bytes);          // the target sent a request
loader.on_transfer_datagram(now, slot, from, bytes);   // a datagram for one transfer's socket
loader.on_timer(now);                                  // time passed
for (const Command& c : loader.commands()) { ... }     // send, send from the control port, close a socket
const Instant wake = loader.next_wake();               // when to call on_timer next, or `never`
```

A transfer is one **slot**, and a slot is one socket (the runner opens it at the transfer's first send and
closes it on a `release_slot` command). The loader is TFTP client for the initialization file and for the
list it writes, and TFTP server, on its control port (59, or the one the `port` option named), for
everything the target starts, so several slots are open at once. `Runner` waits on all of them from one
thread with `Network::wait_readable`.

What the runner does is small enough to read in one sitting (`src/arinc615a_runner/src/runner.cpp`), which is
the point: anything that can send a UDP datagram and tell the time can drive the engine, an RTOS port
included, and a test can drive it with no network at all ([Testing](#what-has-been-checked-and-what-has-not)).

### Memory, buffers and lifetimes

- **No allocation, no exceptions, no RTTI, no `iostream`.** The one exception to "allocates nothing" is
  calculating a checksum: `adl::arinc665::IntegrityChecker::check_value()` returns the value in a
  `std::vector`, so a file that has a checksum costs one small allocation when it ends. The runner library
  (`adl::arinc615a_runner`) is host code and uses `std::vector` and `std::string`.
- **The caller gives the loader a workspace** (`DataLoader::workspace_bytes(config)`, about 270 KB with the
  defaults, most of it the protocol file buffers of 32 KB, one for each of the six slots and one for the
  outgoing list): a datagram buffer and a protocol file buffer for
  each slot, a buffer the outgoing list is encoded into, and a pool for the names a download expects. The
  `DataLoader` object itself (some 16 KB on MSVC when this was written; not re-measured) holds the slots, the expected files and the commands, so
  keep it off a small stack. A protocol file larger than `max_protocol_file_bytes` is refused with error 3.
- **A decoded packet or file views the bytes it was decoded from**, so the loader owns every buffer it
  holds a view into: the protocol file being received lives in the slot's buffer until the file has been
  decoded, shown to the listener and dealt with. What the listener is handed (a status file, a list, a file
  name) is valid **during the call only**; a listener that wants to keep it copies it.
- **`Command::bytes` points into a slot's datagram buffer** and is valid until the next call. The runner
  carries out the commands after each call, in order, before it makes another.
- **The work is bounded.** At most 8 slots (6 by default), 256 files expected by a download, 128 chosen in an
  operator defined download, 64 targets in a FIND, 64 commands per call. A request beyond a bound is refused
  with an error code rather than allowed to grow.

## One transfer: `TftpTransfer`

The four ways a transfer starts are `start_read` and `start_write` (we send a request) and `accept_read` and
`accept_write` (a peer's request has arrived). It holds to:

- **Lock-step blocks**, numbered from 1; a write request is acknowledged with block 0; **after 65535 comes
  1**, never 0 (5.3.2.3.9). The wrap is tested with 65 541 blocks of 8 bytes each way.
- **A time-out of two seconds and one retry for each exchange**, so two attempts in all, and then the
  transfer has failed (Attachment 4, 4-3). A receiver that waits for a block sends its last ACK again; a
  sender sends its last block again; a client sends its request again. A peer's `timeout` option sets the
  interval.
- **The Sorcerer's Apprentice fix**: a sender never sends a block again because it saw a duplicate ACK
  (4-3.3). It sends again only when its time-out runs out. A test duplicates every datagram on the link and
  counts the blocks the sender sent: each once.
- **The options of 5.3.2.3.8**: `blksize` (asked for by the loader's own requests, taken down by the server,
  never up), `tsize` (a read asks for it with 0, a write gives the file's size), `timeout`, `port`, and
  `checksum_<n>`. An option it does not know is left out of the OACK, and a transfer does not fail for an
  option the peer did not take (5.3.2.2): when it did not take `blksize`, the block size is 512.
- **`tsize` that does not match fails the transfer** (5.3.2.3.8.2): a file received whose length is not the
  size the peer announced is `size_mismatch`, and an error packet goes back.
- **The checksum never fails a transfer** (5.3.2.3.8.5): a write request's `checksum_<n>` is acknowledged
  if the algorithm can be calculated, the value is calculated as the blocks arrive (`ChecksumVerifier`), and
  the verdict (`matched`, `mismatched`, `not_given`, `unsupported`) is reported with the file. A file that
  did not match is handled exactly as one that did.
- **WAIT** (5.3.2.3.4): an answer `WAIT:x` to a *request* ends the transfer with `wait_requested` and `x`.
  The same text in the middle of a transfer is an ordinary error, which is what 5.3.2.3.5 says ("only when a
  connection request is received").
- **ABORT** (5.3.2.3.6): `ABORT:XXXX` from the peer ends the transfer with the status code; `abort()` sends
  one, to a peer that is known.
- **A repeated request** from a client whose first answer was lost is answered with the first answer again,
  not by a second transfer. A block that arrives again is acknowledged again and written once. After the last
  ACK of a file it received, a receiver stays for the **dally** (two seconds; the standard calls it
  "optional" in 5.4.3.1) to answer a repeat of the last block, and the transfer is then released.
- **Datagrams that are not for it** are ignored: another host, another port than the one the peer is
  using, bytes that do not decode (they are taken as lost, which is what a damaged UDP datagram is). A
  block longer than the block size is an illegal operation (error 4).

## An operation: `DataLoader`

### Information (5.4.2, 6.3.1)

The loader reads `<THW_ID_POS>.LCI`. If the target accepts, it writes `<THW_ID_POS>.LCL` (the list) and, if
the operation takes time, `.LCS` status files. The operation is complete when the list has arrived and the
target either sent no status file (the fast case of 6.3.1, "the operation will be completed at the end of
transmission of the .LCL file") or sent one that says 0x0003.

### Upload, short load and full load (5.4.3, 6.3.2)

`.LUI` is read; the target writes a `.LUS` status file saying "accepted, not yet started" (0x0001); only
then the loader writes `.LUR`, the list of loads (6.2.7.3). From then the loader is a TFTP server "without
any hypothesis in the sequence of the target hardware request files" (6.3.2): it serves header and data
files, several at a time, through the `FileSource`, while status files keep arriving. Short load and full
load are the same protocol (5.4.3.2), the target asking for fewer or more files; there is nothing to do for
either. The operation is complete at the status 0x0003.

The engine does not read the header file: the names of the data files are in it, and the target asks for
them by name, so the `FileSource` is asked for each name as it comes. A file that is not available yet is
answered with `WAIT:x` ([File_Not_Available], 6.2.8). Two files of one name in a load (section 3.2.4 of 665,
the two disks of an ARINC 615 part) are told apart by the 16-bit CRC the target puts in the request's
`checksum_2` option, which the `FileSource` receives.

### Download (5.4.4, 6.3.3, 6.3.4)

*Media defined*: `.LND` is read; at the status 0x0001 the loader writes `.LNR` (the files to fetch); the
target writes them, and status files. *Operator defined*: `.LNO` is read; the target writes `.LNL` (what it
could give); the listener chooses (`on_download_list` fills a `FileSelection`, or defers the choice and calls
`DataLoader::provide_selection` later, or cancels, which aborts); the loader writes `.LNA`; the target
writes the files.

Only the files the operation asked for are accepted, by name and once each: a write request for anything
else is refused with error 2, so a target cannot choose a path (the `FileSink` is still free to defend
itself, and `DirectoryFileSink` does). Each file goes to the `FileSink` as it arrives and is made
permanent only when complete (`finish`) or thrown away (`discard`).

### The data load protocol time-out (Attachment 4, 4-4.1)

Between the end of one of the listed transfers and the start of the next the target has **13 seconds**. The
table lists `.LCI→.LCS`, `.LCS→.LCS`, `.LUI→.LUS`, `.LUS→.LUS`, `.LNI→.LNS` and `.LNS→.LNS`. How this
resolves what the standard leaves unclear:

- **`.LNI` does not exist.** The standard's own list of protocol files (Table 6.4-1) has `.LND` and `.LNO`
  and no `.LNI`; the download's initialization files are those two. The time-out is applied to both, as the
  start of `.LNS`; for the operator defined mode the target's next transfer may also be `.LNL`.
- **`.LCI→.LCL`.** 6.3.1 says the target may send no status file if "the `.LCI`-`.LCL` sequence is initiated
  before expiration of the DLP-TO". After `.LCI` the loader therefore takes either a status file or the
  list.
- **After the loader's own list** (`.LUR`, `.LNR`, `.LNA`) and **after the target's list** (`.LCL`, `.LNL`)
  the table says nothing. The same 13 seconds are applied from the end of that transfer, because the
  standard also says status files are to be sent "within the maximum delay" "as soon as the list transfer
  step phase begins".
- **While a status file, a list or the loader's own list is being transferred the time-out is not
  running**; the TFTP time-out covers that transfer. A data file in flight does *not* suspend it (5.4.5.1
  says status files are the heartbeat), so a target that needs longer than 13 seconds for one file has to
  send status files in parallel (6.3.2: "status file exchanges are relative to the whole transfer step") or
  announce it with the exception timer.
- **The exception timer** (6.3.1, field of the status file, used only with status 2 and 4): the loader waits
  `max(13 s, timer)` for the next status file, and the operation ends with `exception_timer_expired` if
  none comes.
- **A time-out is fatal**, with no retry (4-4.1): the operation ends with `dlp_timed_out`, and whatever is open
  is ended with `ABORT:1004`.

### Retries (Attachment 4, 4-4.2)

The **DLP retry number** is 1 for every file. It applies to the loader's own requests (`.LCI`, `.LUI`,
`.LND`, `.LNO`, `.LUR`, `.LNR`, `.LNA`): a transfer that failed by a time-out, an error from the peer or an
OACK that broke the rules is made again, once; the second failure ends the operation with
`transfer_failed`. It does not apply to a local file error, to an ABORT, or to WAIT. WAIT is not an error: the
request is made again after `x` seconds, and does not use the retry (the standard sets no limit; the loader
gives up after `max_waits`, 10 by default, so that a target cannot hold it for ever). A target that answers
our request with error 8 ("option refused") is asked again without any options, which also does not use
the retry (5.3.2.2: a transfer should not fail for an option that is not implemented).

What the target starts is not retried by the loader: "it is up to the client to activate the iterations"
(Attachment 4). A newer request from the target for the status file, a list or the same file replaces one
still open (the target gave up on that one), and the loader says nothing to the old one.

### Status files (5.4.5.1, 6.4)

- **The counter** starts at 0, goes up by one with each status file and wraps from 0xFFFF to 0 (6.4.3).
  "Initial value is 0x0000" and "incremented before sending" can be read as the first file carrying 0 or 1:
  both are taken as the first, and a larger number is a gap. A counter equal to the last is a **repeat** (the
  target did not see our last ACK and sent the file again) and one that is behind it by less than 32768 is
  **out of order**: neither is shown to the operator again. A counter ahead by more than one means status
  files were **lost**, and says how many (`Notice::status_counter_gap`).
- **The status code** is what ends the operation: 0x0003 completes it (after the transfers still open for
  the list or the files have ended), 0x1003 is an abort by the target (5.4.5.2), and 0x1004 and 0x1005
  confirm an abort of ours. Anything else is "in progress". A status of an operation that completes before
  the loader has sent its list is a protocol error.
- **A file that is not a valid structure is fatal** (6.3.1 and the other charts: "any invalid structure is
  considered a fatal error"). It ends the operation with `protocol_error` and whatever is open is aborted.
- A status file may list loads or files, and some of them may have failed (0x1007); the number is in
  `Outcome::items_failed`, and the operation's own outcome is the status of the whole.

### Interruptions (5.4.5.2, 6.2.17, 6.3.5)

- **The operator's abort** (`DataLoader::abort`, with 0x1005, or 0x1004 for the protocol's own): every
  transfer open is ended with `ABORT:XXXX`. The target's next request for a status file is answered with
  `ABORT:XXXX` too, **unless** an ABORT has already gone to the target on another transfer, which informs it
  as well; the status file after that is the target's confirmation (it echoes the code), and ends the
  operation with `Outcome::abort_confirmed` set. If none comes in 13 seconds the operation ends anyway, unconfirmed.
  Until then a request for anything but a status file gets `ABORT:XXXX`. An abort before the target has
  accepted the operation has nothing to confirm and ends at once.
- **The target's abort** is a status file saying 0x1003: the operation ends (after the file has been
  acknowledged, which is the acknowledgment 6.3.5 asks for) and everything else open is ended with
  `ABORT:1003`.
- **A protocol version that is not `A4`** (6.4): the loader does not read the file, and answers the first
  status file request with `ABORT:1004`; the operation ends with `unsupported_protocol_version`.

### What a refused request gets

Replies to a target's request that is not accepted are sent **from the control port**, not from a socket of
their own, so that a target that expects the answer from the port it wrote to sees it.

| Request | Answer |
| --- | --- |
| before the operation has been accepted | error 4, illegal operation |
| a protocol file that is not this operation's, a file nobody asked for, a read when nothing is being uploaded | error 2, access violation |
| the list or a file already received | error 6, file already exists |
| no slot is free | error 3, "disk full or allocation exceeded", as 5.3.2.3.3 says the loader should |
| the file is not there | error 1; a file that is there later: `WAIT:x` |
| an abort is under way (anything but a status file) | `ABORT:XXXX` |
| from another host than the target | nothing |

## FIND (Attachment 3)

`FindClient` builds the request (`0x0001 0x00 0x10`) and opens a window of three seconds; `Runner::find` sends
it from a socket bound to port 1001 to the limited broadcast address (or to each address of a list, which is
what AFDX needs, Appendix E) and feeds the answers back. A packet that does not decode as an answer, an answer
with no hardware identifier, a repeat of an answer already registered and an answer after the window are
ignored (3-2: no errors in FIND). A concentrator may answer for several units, so only an identical answer
from the same host is a repeat. There is no retry: the standard sends one request. Answers are taken from any
port (it asks for 1001 but does not say a target must).

## The runner

`Runner::run` opens the control port, starts the operation, and loops: wait on the control socket and every
open slot until the next wake-up, read what arrived, hand it to the loader, carry out the commands, and call
`on_timer`. It ends when the loader is `idle()`: it has an outcome and every transfer, a lingering receiver
included, has ended. A `RunController` is polled between waits (every 100 ms by default) for an operator's
abort or a deferred choice. A datagram that cannot be sent is treated as lost.

## Things chosen where the standard is silent

1. **The loader's block size**: it asks for 1024 and agrees to up to 4096, and takes what the target answers
   (down to 8). 5.3.2.3.8.1 says only that the loader "should implement the block size option" and the
   fragmentation it implies. Both are in `LoaderConfig`.
2. **Port 59** is the loader's control port unless configured; the `port` option is sent only for another.
   A target that leaves the option out of its OACK has refused it, and "the load operation may fail"
   (5.3.2.3.8.6): the operation goes on, `Notice::port_option_ignored` is given, and the target, writing to
   port 59, is not heard, so the 13 second time-out ends it.
3. **Strays**: a datagram from another port of the right host is ignored. RFC 1350 says to answer it with
   error 5; that is not done, so the engine never speaks to a host it was not talking to.
4. **The list is written after status 0001, not before** (6.2.7.3 says so for `.LUR`); the same is done for
   `.LNR`, whose chart (6.3.3) shows the same wait. For `.LNA` it is the operator's choice.
5. **A WRQ for a file already received** is refused with error 6 (a target that did not see our last ACK
   repeats the last block, which the dally answers, and not the whole request).
6. **Completion** waits for transfers still open for the list or a file to end: 6.3 says status 0x0003 is
   "the last file sent", but they are separate transfers and may overlap.
7. **What the engine does not do**: it does not read a header file, so it does not know which files belong
   to a load (the `FileSource` does); it does not create the `DNLD_DATA_<THW_ID_POS>_<number>` directory
   of 5.4.4.3 (the application's job, with the sink); it does not run FIND before each operation (the
   application may); it does not show the display texts of Table 6.4.10-1; and it has no target side.

## What has been checked, and what has not

`tests/engine` (the `adl_engine_tests` program, [testing](testing.md)): the transfer on its own (two of them
over a link in memory with a seed for loss, duplication and reordering, and one at a time with every datagram
written out), the loader with hand-written datagrams (what it sends and when, to the nanosecond at the figures
of Attachment 4), and whole operations through the runner on the fake platform against a **simulated target**
(`tests/engine/sim_target.cpp`) that has its own TFTP endpoint (`adl::arinc615a_mini`,
`src/arinc615a_mini/src/mini_tftp.cpp`) sharing no code with the
engine. A full load of a media set built with `adl::arinc665::build_media_set` goes to the target, and files
come back in a media defined and an operator defined download, also over a network that loses, repeats and
reorders datagrams, with the target asking to wait, going silent, aborting, announcing an exception timer,
losing and repeating status files, sending a wrong checksum, and with the operator aborting. Every
history is deterministic from a seed.

**Not checked:** against any other implementation of 615A (a loader or a target); on a
real network or against a device: `loopback.integration` (`tests/loopback`) runs whole operations through the
runner and the real host's sockets on 127.0.0.1, but against the mock target's engine, whose TFTP is
`adl::arinc615a_mini` (the same reading of the standard), so it checks the sockets and the runner and not the
choices; `adl::platform`'s conformance suite covers the sockets on their own; under `-fno-exceptions -fno-rtti` in CI (the engine is written for it and
builds that way by hand, see [testing](testing.md#known-gaps)); the choices above that are marked as
mine, of which 1 to 3 and the DLP-TO applications to `.LNO`, `.LCL` and `.LNL` are the first to confirm
against a real device.
