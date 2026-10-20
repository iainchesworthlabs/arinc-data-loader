**A fake platform** (`tests/fake`): an in-memory clock, network, file system,
memory and threads on a clock that moves only when told to, so the engine can be
tested without a socket or a real second. The network loses, duplicates,
corrupts and reorders datagrams from a seed or by a rule, and a simulated target
can answer at an address.
