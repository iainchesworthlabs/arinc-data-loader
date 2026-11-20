# Licensing

An engineering inventory of what the project is licensed under, what it uses,
and what is still open. It is not legal advice, and it does not change the
licence.

## Summary

1. **The project is GPL-3.0-or-later** (`LICENSE`, `vcpkg.json`). Source files
   (C++, QML, CMake, Python, shell and PowerShell) carry a one-line SPDX comment naming
   GPL-3.0-or-later; `REUSE.toml` gives every other file the same licence and the copyright
   holder, and the fonts their own. `reuse lint` checks it,
   and the licence texts are in `LICENSES/`. One part, the licence library, is also offered
   under the BSD 2-Clause licence (item 7).
2. **The third-party code in use is a build tool (vcpkg) and a test framework
   (Catch2, BSL-1.0).** Neither is linked into anything that is distributed. The two RTOS test programs
   (`tests/platform/zephyr`, `tests/platform/freertos`) fetch, at build time, and link into those
   test programs only, Zephyr (Apache-2.0) and its littlefs module (BSD-3-Clause), FreeRTOS-Kernel
   (MIT), lwIP (BSD-3-Clause) and littlefs (BSD-3-Clause), none of it copied into this repository.
   A product that links the platform layer to any of them distributes that component under its own
   licence beside this project's GPL-3.0-or-later, and carries its notice.
   One library is copied into the repository: Monocypher 4.0.2 (`third_party/monocypher`), unmodified,
   under the BSD 2-Clause licence or CC0 1.0 at your choice (`REUSE.toml` and `LICENSES/` name both). It does
   the Ed25519 signature check of a licence file in `libs/licence`. No program links that library yet. When
   one does, Monocypher joins the notices it shows, with the notice that is already written
   (`notices/fragments/monocypher.txt`). The vendored directory is outside the rule that holds the
   preprocessor count at zero (`tools/preprocessor-exclude.txt`), because the code is C and uses it.
   What the applications ship from others is Qt (item 3), the Archivo and
   Noto Sans Hebrew typefaces (item 6) and, on Windows, the runtime files Qt's
   deployment tool places beside Qt. All of it is listed, with the text of its
   terms, in the notices the applications show (below).
3. **Qt 6.12 is linked by the three applications**, dynamically, under the GNU
   Lesser General Public License, version 3. The offer of the source, the right to
   replace the libraries and the LGPL text are in the notices. One module the
   deployment places, `Qt6Quick3DUtils` (loaded by the QML profiler plugin), is
   offered under the GPL 3 rather than the LGPL; the project's own GPL-3.0-or-later
   satisfies that.
4. **Tracy is opt-in** (`ADL_ENABLE_TRACY`) and is not part of a release build.
5. **The ARINC standards are not this project's to license, and are not in the
   repository.** See below.
6. **Archivo and Noto Sans Hebrew are bundled with the applications**, both under
   the SIL Open Font License 1.1.
   - Archivo: `libs/ui/fonts/Archivo-Regular.ttf`, `-Medium.ttf`, `-SemiBold.ttf`
     and `-ExtraBold.ttf`, with the licence text in `libs/ui/fonts/OFL.txt`. The
     faces are resources of each application and are not modified.
   - Noto Sans Hebrew, for the Hebrew and Yiddish windows (Archivo has no Hebrew
     letters): `libs/ui/fonts/NotoSansHebrew-Regular.ttf`, `-Medium.ttf`,
     `-SemiBold.ttf` and `-ExtraBold.ttf`, with the licence text in
     `libs/ui/fonts/NotoSansHebrew-OFL.txt`. They are static instances cut from the
     variable font the Noto Project publishes; the letters are not otherwise
     changed. They are resources of each application too.

   The About dialogs name Archivo and its licence, and the Licences dialog
   carries both OFL texts.

7. **The licence library is offered under two licences.** Its own files carry
   `GPL-3.0-or-later OR BSD-2-Clause`, and you may use them under either one. They are the headers,
   the sources (the signed-file reader, the strict JSON reader, the Ed25519 wrapper, the licence states
   and the words) and the build files of `libs/licence`; its tests in `tests/licence`; the golden data
   its tests compare against (`tests/golden/golden_licence.hpp` and the script that writes it,
   `tests/golden/make_golden_licence.py`); the fuzz harness in `libs/licence/fuzz` and its seeds. The
   texts are `LICENSES/GPL-3.0-or-later.txt` and `LICENSES/BSD-2-Clause.txt`. The reason is that
   software kept outside this repository, which is not under the GPL, uses the same files at a pinned
   commit. Three things in that part are not under the pair:
   - Monocypher (`third_party/monocypher`, item 2) keeps its own terms, BSD-2-Clause or CC0-1.0.
   - The Project Wycheproof vectors (`tests/licence/data/wycheproof`) are Apache-2.0, unchanged,
     from Google and the Wycheproof contributors.
   - Nothing outside the paths above changes. `libs/platform`, the other libraries, the programs and
     the rest of the tests stay GPL-3.0-or-later.

   The library includes nothing from this repository: it takes the C++ standard library and Monocypher
   (`tests/licence` also takes Catch2, which is BSL-1.0). Its CMake files use this repository's
   warning, sanitizer and coverage targets and `adl_add_fuzz_target`; a user of the code under the
   BSD terms writes their own build for it. The seeds are made from the golden data by
   `tools/fuzz/make_seeds.py`, which is GPL-3.0-or-later like the other tools.

## The notices the applications carry

About > Licences (all three applications) shows `NOTICES.txt`, and the build places the same
file, with `LICENSE.txt`, beside the executables. It is assembled at configure time from
`notices/fragments` and `notices/licences` by `cmake/Notices.cmake`, so the versions are the
ones the build used, and a missing fragment or licence file stops the configure. See
[notices/README.md](../notices/README.md) for how to add a component; `system.notices` checks
the generated file.

Every package carries `LICENSE.txt` and `NOTICES.txt` ([Releasing](releasing.md#what-a-release-contains)),
and the Qt part of `NOTICES.txt` says that the Qt libraries may be replaced with a modified Qt 6 that is binary-compatible with the version shipped.

## The standards

`docs/specs/665-3.pdf` and `docs/specs/615A-3.pdf` are ARINC Report 665-3 and
ARINC Report 615A-3, published by Aeronautical Radio, Inc. They are third-party
documents under their publisher's terms, **not** covered by the project's
licence. Whatever terms they were obtained under, check that they allow
redistribution before assuming they do; most such standards do not.

A developer keeps them in `docs/specs/` so that the code and the comments can
cite them. `.gitignore` excludes `docs/specs/*.pdf`, so they never enter the
repository or its history; only `docs/specs/README.md`, which names them, is
tracked. To work on the code, put your own copies there. Do not force them in
with `git add -f`. In addition:

- do not copy their text into the source, tests or documentation beyond a
  section number and a value needed to check an implementation (the CRC check
  values in the tests are of that kind);
- cite them by section, as the code does.

## Contributions

Contributions are accepted under the project's licence, GPL-3.0-or-later (see
[CONTRIBUTING.md](../CONTRIBUTING.md)).
