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
| FIND | Listens on UDP port 1001 (configurable) and answers a FIND request with the configured identity: target hardware ID, type name, position, literal name and manufacturer code (`adl::arinc615a`'s FIND codec). |
| Initialization files | A TFTP server on port 59 (configurable) serves `<THW_ID_POS>.LCI`, `.LUI`, `.LND` and `.LNO` with an "accepted" status file and protocol version A4. It honors the loader's options (block size, and the `port` option that lets the loader use a control port of its own). With an injection it answers WAIT or refuses the operation with 0x1000 ("operation denied"), so the loader's error paths can be shown. The engine can also refuse with 0x1002 and serve an initialization file of an invalid structure (`Config::refusal`, `Config::garbage_init`); the window has no control for either. |
| Information | Writes the configuration status files (`.LCS`), then the configuration list (`.LCL`, hardware with parts), then the final status 0x0003, on the DLP status cadence. |
| Upload | Reads the upload request (`.LUR`), then each load's header, data and support files from the loader, checking each against the check value the header file records; writes upload status files (`.LUS`) with per-load progress; stores what it read to `<storage folder>/<load part number>/<file name>`; ends 0x0003, or 0x0002 naming the failed loads. |
| Download | Both modes: for a media defined download it serves the files the loader's `.LNR` asks for; for an operator defined one it writes the list of files it holds (`.LNL`) and serves only the ones the loader's `.LNA` chooses. The files it can serve are read **once, when Start is pressed** (the session), from `<storage folder>/holds`, so a file put there while the target is running is not served until it is stopped and started again; with no storage folder or nothing in `holds` it holds one demonstration file, `DEMO.TXT`. Download status files (`.LNS`) carry the progress; it ends 0x0003. |
| Aborts | When the loader sends ABORT, the mock target stops its transfers and ends the operation with a final status file carrying the loader's code, which is what makes the loader report the abort as confirmed. |
| Injections | Answer the first request with WAIT for N seconds, answer every request with WAIT, refuse one chosen operation (Information, Upload, Download media defined, Download operator defined), drop every Nth status file, a bit flipped in every served file (the protocol files included, so a download ends at once as "unsupported protocol version"), a wrong checksum option (reported as a notice; the file is kept and the operation completes), an abort (status 0x1003) after N status files, and silence after the first status file (the loader then runs into its 13 second data load protocol time-out). Think time (0 to 60 000 ms, default 1000) and the status file period (0 to 3600 s, default 4) shape the pace of a dialogue. Defaults: behave. |
| The log | Every status file, file read or written, WAIT and abort as a line with a severity, shown in the window like the loader's activity log. |

## Layout

```text
src/mock_target/
  main.cpp              creates the QGuiApplication, one MockTargetModel and the QML engine
  qml/                  TargetWindow.qml (the window)
  core/                 adl_mock_core: everything but the window, Qt Core only
    engine              the 615A target behaviour: no Qt, no socket, one thread, driven by
                        datagrams and timers (like the loader's own engine, the other way round)
    link                the seam the engine speaks through: RealTargetLink over adl::platform's
                        sockets, and the tests' FakeTargetLink on the fake network
    model               MockTargetModel: the worker thread, the properties and signals the
                        window binds to
tests/mock_target/      the engine against the loader's real engine on the fake platform, the
                        model, and the window loaded offscreen
src/ui/                 the shared design system (Theme, controls, header, footer, log)
```

The window (1280×900, at least 1120×760) follows the desktop's light or dark scheme, is English only and remembers its size and place. The header owns the run state and Start/Stop: a `SERVING` / `STOPPED` chip and the two buttons (Start needs a hardware ID; ports must be 1 to 65535). The primary button is the one that applies: Start while stopped, Stop while serving. Every field is disabled while the target is serving, so a configuration is changed by stopping, editing and starting again.
The left rail is the configuration, numbered (identity, ports, storage, behaviour, fault
injection), and the right is the evidence (the operation, the activity log and the stored files).
Fault injection is its own block with an `N ACTIVE` / `BEHAVING` chip and `Reset all`, and, because
that block is below the fold at 1280×900, any armed injection also shows as a "N faults injected" chip in
the header and the footer; pressing either scrolls the rail to the block, so a demo cannot quietly run
with a stale fault. The state is named once, in the header; the operation panel says what the target is
doing ("Waiting for a request", the operation's name, "Not started") and the footer says whether and
where it listens. The progress bar moves only while the target is working on a request, not while it
merely listens, and an error from the target is an `AppBanner`. The derived `THW_ID_POS` is shown and
copyable; an unset storage folder is called out as "still serves, not kept".

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
  copied Qt values. Stopping reaches the session through the link's flag, which the run loop polls (a
  queued call cannot reach a thread that is inside the loop). The model counts the sessions it queues, and
  a stop covers the sessions queued up to the moment it is asked for, under the mutex that also guards the
  link a stop is delivered to: a stop that comes before the session has begun, while its link is being
  made, while it runs, or while it is taken down is delivered once and to the right session, never to a link
  that is being destroyed, and a stop between two quick starts is not lost (a single flag cleared by the next
  start lost it, and left the second session serving with the window showing it stopped). A stress test
  starts and stops the model 300 times with a different gap each time.
- **What the target stores is named safely.** A file the loader sends is stored only under a plain name (letters,
  digits and `. _ -`) that `adl::arinc665::is_portable_file_name` also accepts, so a colon, a device name
  (`NUL`, `COM1`...) or a trailing dot is refused on every platform; the load's folder is its part number
  with anything else turned into `_`, and a device name or trailing dot made harmless.
- **The files a download serves are read when Start is pressed** (the session), not when a download
  begins: a file put in `holds` afterwards is served after a stop and a start.
- **The window** lets the operator edit the identity, the ports, the storage folder and the
  injections before pressing Start, and shows the operation, its progress, the log and the
  files an upload stored while it runs.

## What has and has not been checked

Checked: the engine against the loader's real engine (`DataLoader` and `Runner` on the fake
platform, both ends on one fake network): FIND, a full Information operation, an Upload in which
every file arrives byte for byte, both Download modes, an operator abort with the target's
confirmation, and every injection the loader has a path for (denied, DLP time-out, protocol
error, checksum mismatch, a lost status file, a target-side abort, WAIT). The same dialogues
over **real sockets**, through the loopback integration test (`loopback.integration`): the
loader's runner over the real platform against the engine over `RealTargetLink` on 127.0.0.1 —
Information, an Upload stored byte for byte, both Download modes and an abort. The model on its
worker thread, storage written to disk, and the window loaded offscreen with no QML complaints,
in the light and the dark scheme. The platform-isolation check covers the new sources.

Also checked, by hand, on Windows, with the three real programs on one machine and a script driving
the windows ([the screenshots](screenshots.md)): the whole demonstration of
[the user's guide](user-guide/demonstration.md) (Information, an Upload stored byte for byte, both
Downloads), a refused upload, a silent target with an abort that is not confirmed, a target that is not
running, WAIT, dropped status files and an abort by the target. In every operation the loader logs
`status file transfer failed: <file> (timed out)` after each status file the mock target writes, while
the status itself is received and the operation completes; the cause has not been found (the loader's
TFTP server end of a status file, or the mock target's last packet), and it is listed as an open
question in the user's guide.

Not checked: FIND between the two applications on one machine does **not** work (the loader's
broadcast is answered by nothing: "0 targets answered", on Windows; FIND is fixed to port 1001 on
both ends by the standard, and two sockets sharing a port on one machine is not reliable) — typing
the target's address is the single-host path; on Linux and macOS binding ports 59 and 1001 may need
privileges (`Config::bind_address`, the window's **This computer only** setting, binds 127.0.0.1
instead of every interface: no firewall prompt on Windows, but on macOS a low port on a single
address may still need root; see the user's guide, Installing). The loader binds loopback too when
its target is a loopback address (`RunOptions::local_address`). The window at scales other than 100%; keyboard use and screen readers beyond the
accessible names.
