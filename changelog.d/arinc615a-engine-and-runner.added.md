**The 615A engine and runner** (`adl::arinc615a`, `adl::arinc615a_runner`,
[docs/arinc615a-engine.md](docs/arinc615a-engine.md),
[docs/arinc615a-coverage.md](docs/arinc615a-coverage.md)): the data loader's side of ARINC
615A-3 as state machines that are told the time and the bytes that arrived and answer with the
bytes to send, with no socket, thread, clock or file system inside, no allocation, exceptions or RTTI.
A TFTP transfer in both roles and both directions (options, block numbers wrapping 65535 to 1, the
2 s time-out with one retry, the Sorcerer's Apprentice fix, WAIT, ABORT, `tsize` and checksum checks
that never fail a transfer for a bad checksum); the Information, Upload (short and full load) and
Download (media defined and operator defined) operations with the status counters, the 13 s data load
time-out, the exception timer, the DLP retry, and the abort paths of 5.4.5.2; FIND. The runner opens
the sockets, waits on them from one thread and drives the engine over `adl::platform`; a file source
and sink over a directory of the platform's files come with it. Tested against a simulated target
on the fake platform (a full load of a media set made with `build_media_set`, downloads back, and loss,
duplication, reordering, WAIT, ABORT and silence from fixed seeds) whose TFTP shares no code with the
engine. Not yet checked against another implementation, or over real sockets.
