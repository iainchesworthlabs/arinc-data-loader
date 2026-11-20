# Test data for the licence library

`wycheproof/ed25519_test.json` is the Ed25519 test file of [Project Wycheproof](https://github.com/C2SP/wycheproof)
(210 vectors, `generatorVersion` and notes inside), unchanged, under Apache-2.0 (`REUSE.toml`). It was taken
from the copy that Virtual Serial Studio keeps in its tests and has the SHA-256
`ea80ffebac085e21cb9d3e903e7fa2b448d96702da5b31f061d0b83b430b74b3`.

`tests/licence/signature_test.cpp` checks every vector against `signed_file::verify_ed25519`. A vector the
file marks valid has to verify and one it marks invalid must not; the few it marks acceptable may go either
way.
