# The mock target

`src/mock_target` is the second Qt application: a mock ARINC 615A target that runs on Windows,
macOS and Linux over real UDP sockets, so that the data loader's dialogues can be demonstrated
end to end against something that behaves like a target without an embedded box. It is a mock
for demonstrations, not a real target, and it is not what the loader's tests run against (those
use the simulated target of `tests/engine`, which runs in memory on the fake platform).

The window shows what the target is doing while it does it: it answers FIND with the identity
the operator configured, serves the initialization files, writes its status files, reads the
load's files (an upload), writes the files of a download, and stores what an upload left in it
on disk. The operator can also make it misbehave, to watch the loader cope: answer WAIT, refuse
an operation, drop status files, send a wrong checksum, abort on its own, or go silent.

## What it does

| What | How |
| --- | --- |
| FIND | Listens on UDP port 1001 (configurable) and answers a FIND request with the configured identity: target hardware identifier, type name, position, literal name and manufacturer code (`adl::arinc615a`'s FIND codec). |
| Initialization files | A TFTP server on port 59 (configurable) serves `<THW_ID_POS>.LCI`, `.LUI`, `.LND` and `.LNO` with an "accepted" status file and protocol version A4. It honors the loader's options (block size, and the `port` option that lets the loader use a control port of its own). With an injection it answers WAIT, refuses (0x1000 or 0x1002) or serves garbage, so the loader's error paths can be shown. |
| Information | Writes the configuration status files (`.LCS`), then the configuration list (`.LCL`, hardware with parts), then the final status 0x0003, on the DLP status cadence. |
| Upload | Reads the upload request (`.LUR`), then each load's header, data and support files from the loader, checking each against the check value the header file records; writes upload status files (`.LUS`) with per-load progress; stores what it read to `<storage directory>/<load part number>/<file name>`; ends 0x0003, or 0x0002 naming the failed loads. |
| Download | Both modes: for a media defined download it serves the files the loader's `.LNR` asks for; for an operator defined one it writes the list of files it holds (`.LNL`) and serves only the ones the loader's `.LNA` chooses. Files are read at operation start from `<storage directory>/holds`. Download status files (`.LNS`) carry the progress; it ends 0x0003. |
| Aborts | When the loader sends ABORT, the mock target stops its transfers and ends the operation with a final status file carrying the loader's code, which is what makes the loader report the abort as confirmed. |
| Injections | Think time, status period, first-request WAIT, refuse an operation, drop every Nth status file, a bit flipped in a served file, a wrong checksum option, an abort after N status files, and silence (the loader then runs into its 13 second data load protocol time-out). Defaults: behave. |
| The log | Every status file, file read or written, WAIT and abort as a line with a severity, shown in the window like the loader's activity log. |

## Layout

```text
src/mock_target/
  main.cpp              creates the QGuiApplication, one MockTargetModel and the QML engine
  qml/                  TargetWindow.qml (the window), LogView.qml
  core/                 adl_mock_core: everything but the window, Qt Core only
    engine              the 615A target behaviour: no Qt, no socket, one thread, driven by
                        datagrams and timers (like the loader's own engine, the other way round)
    link                the seam the engine speaks through: RealTargetLink over adl::platform's
                        sockets, and the tests' FakeTargetLink on the fake network
    model               MockTargetModel: the worker thread, the properties and signals the
                        window binds to
tests/mock_target/      the engine against the loader's real engine on the fake platform, the
                        model, and the window loaded offscreen
```

## How it is put together

- **The engine is the loader's engine turned round.** `MockEngine` holds no socket, thread or
  clock: it is handed datagrams and time, and it answers with datagrams and timers, exactly like
  `adl::arinc615a::DataLoader` on the loader's side. It is event-driven and single-threaded;
  `run()` is the loop that adapts it to a real link (wait, feed, fire timers), and over the fake
  link the world's events drive it directly.
- **Its TFTP is the second implementation.** Every transfer goes through
  `adl::arinc615a_mini::MiniTransfer` (the independent TFTP written for the tests, now shared
  from `src/arinc615a_mini`), and every protocol file through `adl::arinc615a`'s codec. The
  mock target shares no transfer code with the loader's engine, so a dialogue that completes
  exercises two independent readings of the standard.
- **Uploads are checked, not just received.** Each file the target reads is verified against the
  check value its header file records (the header's check value, or the 16-bit CRC field), and
  the header file itself is decoded with `adl::arinc665::decode_header_file` to learn which
  files to read. A load whose file fails the check is failed in the upload status file, which
  is what makes the loader report the operation as failed.
- **The link owns the platform.** `RealTargetLink` makes an `adl::platform::Host` once per
  session on the engine's thread (like the loader's `system_host_factory`), binds the FIND and
  TFTP ports with `reuse_address` and a receive buffer, opens one socket per transfer port on
  first use, and waits on all of them with a deadline taken from the earliest pending timer.
- **The model is the loader's worker pattern.** `MockTargetModel` lives on the window's thread,
  owns the engine's `QThread`, and everything the engine reports crosses as queued signals of
  copied Qt values. Stopping is an atomic flag the run loop polls (a queued call cannot reach a
  thread that is inside the loop).
- **The window** lets the operator edit the identity, the ports, the storage directory and the
  injections before pressing Start, and shows the operation, its progress, the log and the
  files an upload stored while it runs.

## What has and has not been checked

Checked: the engine against the loader's real engine (`DataLoader` and `Runner` on the fake
platform, both ends on one fake network): FIND, a full Information operation, an Upload in which
every file arrives byte for byte, both Download modes, an operator abort with the target's
confirmation, and every injection the loader has a path for (denied, DLP time-out, protocol
error, checksum mismatch, a lost status file, a target-side abort, WAIT). The model on its
worker thread, storage written to disk, and the window loaded offscreen with no QML complaints.
The platform-isolation check covers the new sources.

Not checked: a dialogue over real sockets (the loader's runner over real sockets has not been
run either; the fake-network tests prove the protocol, and `RealTargetLink` uses the same
platform sockets the conformance suite holds to the real hosts' behaviour); FIND between the two
applications on one machine, which on Windows needs two sockets on port 1001 to share a
broadcast and is not guaranteed there (typing the target's address is the reliable single-host
path; on Linux and macOS binding ports 59 and 1001 may need privileges). The window at other
scales, keyboard use and screen readers beyond the accessible names.
