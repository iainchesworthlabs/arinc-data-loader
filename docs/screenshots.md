# The screenshots

The pictures in the [user's guide](user-guide/index.md), the README and the application pages are in
[`docs/images`](images). They are pictures of the **real programs**, taken by a script, so that
they can be made again when a window changes and so that nobody has to remember how.

```powershell
# Windows only. Build the Release preset first, from a Developer PowerShell:
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release

# then, from the repository root (PowerShell 7), and leave the machine alone while it runs:
pwsh tools/docs/capture_screenshots.ps1 -WorkDir 'C:\demo'
```

The scratch folder shows in the pictures (the media set's path appears in the footer and the rail),
so the published ones were made with `C:\demo`. It takes about 45 minutes for both colour schemes and
writes about 60 pictures, 2.8 MB in all.

What it does:

1. Writes some sample files (fixed content), starts `adl_media_gen`, fills the form, adds the
   files through the system's file dialog, generates a media set and takes its pictures.
2. Starts `adl_mock_target` with a storage folder, then `adl_app`, and works through the whole
   demonstration of the [user's guide](user-guide/demonstration.md): first run, preferences, FIND,
   choosing and checking the set, Information, Upload, both Downloads, the dialogs.
3. Runs the failures: a refused upload, a target that is not there, a silent target with the window
   closed during the operation, and every fault the mock target can inject.
4. Opens a broken copy of the set, the sample set with a warning, and the loader in Hebrew.
5. Does it twice, in the light colours and in the dark.

The pictures are the **client area of the window** (`PrintWindow`), so they carry no title bar
and no other program. The windows are 1280 by 900 pixels; the generator is 1280 by 1200 and the mock
target 1280 by 1320 so that their whole rail is visible. File names are stable
(`app-upload-complete.png`, `gen-ready.png`, `mock-faults.png`; the dark ones end in `-dark`).

## How the windows are driven

`tools/docs/ui_automation.ps1` is the library: Windows UI Automation finds every control by the
accessible name the program gave it (so the script reads like the guide: "press *Upload the ticked
loads*"), presses buttons, types into fields the way an operator does, and drives the system's file
and folder dialogs through the clipboard (typing long paths into them loses characters). Keys are
sent only while a window of the program under test, or one of its own dialogs, is in front; the
script refuses to type into anything else.

Things to know:

- **Use a Release build.** The Debug presets run under AddressSanitizer, which makes the system file
  dialogs fail, so the script never gets past "Choose...".
- **Settings are saved and put back.** The programs keep their settings in the registry under
  `HKEY_CURRENT_USER\Software\iainchesworthlabs`. The script exports them, empties them so that every
  run starts at the first launch, and restores them at the end, even when it fails. Your clipboard
  text is restored too.
- **Light and dark.** The programs follow the desktop. The light pictures start them with
  `QT_QPA_PLATFORM=windows:darkmode=0`; the dark ones need the desktop itself to be in dark mode
  (the script checks and skips them otherwise). Only the pictures marked as such in the script
  are taken in the dark.
- **One pixel per pixel.** `QT_ENABLE_HIGHDPI_SCALING=0` and `QT_FONT_DPI=96` are set, so the
  pictures do not depend on the display's scaling.
- **A window of another program coming to the front** while a dialog opens can lose the keys; the
  script retries the file dialogs a few times and otherwise stops with a picture of where it was
  (`<work folder>/failure.png`). A firewall prompt for a program that opens a port has to be
  answered by you first, and the script cannot do it.
- **Parts.** `-Only generator`, `demo`, `faults`, `sets` or `language` (several with commas:
  `-Only 'faults,sets'`) run a part (the later ones reuse the media set the generator made); `-Scheme light` or `dark` runs one colour scheme; `-OutDir` and
  `-WorkDir` move the output and the scratch folder (default `build/docs-screenshots`).
- **Size.** A window comes out as a PNG of 100 to 180 KB. When Python with Pillow is installed
  the script re-saves each with a 256-colour palette (`tools/docs/optimise_png.py`), which keeps
  the flat colours and the text sharp at 40 to 70 KB; `-NoOptimise` leaves them as they were taken.

## Pictures from the test programs

The offscreen QML test programs can also save a window's picture
(`adl_app_qml_test <workbench.png> <first-run.png> <right-to-left.png> <preferences.png> <about.png>`,
`ADL_DARK=1` for the dark scheme; see [Testing](testing.md)). They draw the windows over simulated data
without a desktop, which is handy in CI, but they are not what the guide shows: the guide uses the
real programs.

## Adding a picture

Add a `Save-Shot $process 'name'` call where the window shows what you want (add `-Dark` if the
dark one is wanted), run the part (`-Only`), and reference `docs/images/name.png` from the page. The
`docs.links` test fails if a page points at a picture that is not there.
