# Monocypher

[Monocypher](https://monocypher.org) 4.0.2, by Loup Vaillant and contributors, under BSD-2-Clause or
CC0-1.0 (`LICENCE.md`). It provides the Ed25519 signature check behind licence files
(`libs/licence`). No cryptography is written in this project.

The four source files are byte for byte those of the release archive. Do not edit them. To update,
replace them with the files of a newer release and update the table and the version here and in the
notice (`notices/fragments/monocypher.txt`).

| File | From the archive | SHA-256 |
|---|---|---|
| `monocypher.c` | `src/monocypher.c` | `afe2b098c8569577a84488e0b98d276d1fba6506adea68bb9241a52111734c59` |
| `monocypher.h` | `src/monocypher.h` | `f78bb31255cfb7beba66afd2137f5194c8a025cf40488b6cc1e295234d43f374` |
| `monocypher-ed25519.c` | `src/optional/monocypher-ed25519.c` | `7c9b16056cbd27521919e8a6f56a228808b9e718afc42e3d33f28c08e5abdee2` |
| `monocypher-ed25519.h` | `src/optional/monocypher-ed25519.h` | `bd546edcd468d64e28caa3dbf4b1d6bfad7435c0ce994723fd81aae26405121b` |

The archive is `https://monocypher.org/download/monocypher-4.0.2.tar.gz`
(SHA-512 `bf275d4c53ff94af6cdc723a4e002e9f080f4d1436c86c76bb37870b34807f1d7b32331d8ff8a1aeb369e946f3769021e03e63efac25b82efc5abf54dc084714`).
Virtual Serial Studio vendors the same four files, and the table is the one in its copy of this page.
The files were not compared with the archive again for this repository; the hashes were compared
with that table.

The tests run the RFC 8032 vectors and the Project Wycheproof Ed25519 vectors against this code
(`tests/licence/signature_test.cpp`).

The notice for About > Licences is `notices/fragments/monocypher.txt`, with the licence text in
`notices/licences/Monocypher.txt` (a copy of `LICENCE.md`). It is in the notices the programs show:
`NOTICES.txt` lists what a program ships, and the three Qt applications link `adl::licence`, so
`adl_generate_ui_notices` (`cmake/AdlUi.cmake`) names the fragment.

This directory is outside the rules the first-party code follows: the code is C, it uses the
preprocessor, and it is built with its own warnings. `tools/preprocessor-exclude.txt` leaves it out of
the count of preprocessor lines, its own `.clang-format` turns formatting off, and it is not a coverage subject.
