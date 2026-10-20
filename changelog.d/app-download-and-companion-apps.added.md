**The application: the whole job, Download, and two more applications**
([docs/app.md](docs/app.md), [docs/media-gen.md](docs/media-gen.md),
[docs/mock-target.md](docs/mock-target.md)): the loader gains preferences that persist, properties for the
media set and the target, a validation gate (reading the set is checking it) and failure dialogs that say
what to do, and the Download operation in both modes into a fresh `DNLD_DATA_<THW_ID_POS>_<number>`
directory, with a dark scheme and a keyboard pass over all three windows. `adl_media_gen` generates a
media set as the directory tree the loader opens and reads it back to verify it. `adl_mock_target` is a 615A
target over real sockets (FIND, Information, Upload, both Downloads, and failure injections), on
`adl::arinc615a_mini`, the second TFTP implementation extracted from the tests so that the two ends of a
dialogue share no transfer code. `loopback.integration` runs the loader's runner against the mock target's
engine over 127.0.0.1. Not run against a real target.
