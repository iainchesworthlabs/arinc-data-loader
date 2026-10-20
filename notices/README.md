# Third-party notices

What About > Licences shows in the three applications, and what the build places beside
the executables as `NOTICES.txt`. It is generated, not written by hand:
`cmake/Notices.cmake` assembles it at configure time from

- `fragments/*.txt`: one section per component, with `{{TOKEN}}` markers for the versions
  CMake knows (`{{QT_VERSION}}`, `{{VERSION}}`, `{{PLATFORM}}`, ...) and `{{FILE:NAME}}` markers
  for the text of a licence;
- `licences/*.txt`: the licence texts the fragments quote, byte for byte (the typeface's
  OFL is `src/ui/fonts/OFL.txt`, which ships beside the faces as its licence requires).

`cmake/AdlUi.cmake` (`adl_generate_ui_notices`) picks the fragments for the platform and
options, embeds the result in every application as `:/adl/notices/NOTICES.txt`, and copies it
and `LICENSE` to the bin directory. `adl::ui::licence_notices()` reads the resource and
`LicencesDialog.qml` shows it.

## Adding a component

1. Put its licence text in `licences/` (keep the copyright line; do not reflow it).
2. Add `fragments/<name>.txt`: what it is, where it is used, its terms, and
   `{{FILE:<NAME>}}` where the text belongs.
3. Name the fragment in `adl_generate_ui_notices()` (in the position it should appear, and
   under the platform or option that brings it in) and give the `FILES` entry for its text.
4. Reconfigure. A missing fragment, a missing licence file or a `{{TOKEN}}` nobody filled
   stops the configure and names it; `system.notices` checks the generated file.

Anything the project ships that is not its own belongs here: a bundled font, an icon set, a
library linked into a release build. The tools it is built and tested with (CMake, vcpkg,
Catch2) are not distributed and are not listed.
