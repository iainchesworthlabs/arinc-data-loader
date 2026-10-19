# Licensing

An engineering inventory of what the project is licensed under, what it uses,
and what is still open. It is not legal advice, and it does not change the
licence.

## Summary

1. **The project is GPL-3.0-or-later** (`LICENSE`, `vcpkg.json`). The files carry
   no per-file headers; `LICENSE` and `vcpkg.json` are the statement.
2. **The third-party code in use is a build tool (vcpkg) and a test framework
   (Catch2, BSL-1.0).** Neither is linked into anything that is distributed.
   The one third-party asset shipped in the applications is the Archivo
   typeface (item 6).
3. **Qt 6.12 is planned for the application.** The intent is to link only
   modules that are also offered under the LGPL, so that "or later" stays true
   of the whole program. This is to be checked when the application is built,
   not before.
4. **Tracy is opt-in** (`ADL_ENABLE_TRACY`) and is not part of a release build.
5. **The ARINC standards are not this project's to license, and are not in the
   repository.** See below.
6. **Archivo is bundled with the applications** under the SIL Open Font
   License 1.1: `src/ui/fonts/Archivo-Regular.ttf`, `-Medium.ttf`,
   `-SemiBold.ttf` and `-ExtraBold.ttf`, with the licence text in
   `src/ui/fonts/OFL.txt`. The faces are resources of each application and are
   not modified. The About dialog names the face and its licence.

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
