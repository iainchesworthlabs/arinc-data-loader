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
so use a short one such as `C:\demo`. The pictures now in `docs/images` were made in a folder with a
longer name, and that name shows in the footer and the rail of the loader's pictures. They were
also taken before the footer of the generator and the mock target got the same state mark as the
loader's (a play mark or a dot, in place of a 6 px round dot), so their footers show the older
mark. Neither has been retaken. `docs/images` holds 64 pictures, about 6.9 MB in all.

What it does:

1. Writes some sample files (fixed content), starts `adl_media_gen`, fills the form, adds the
   files through the system's file dialog, generates a media set and takes its pictures.
2. Starts `adl_mock_target` with a storage folder, then `adl_app`, and works through the whole
   demonstration of the [user's guide](user-guide/demonstration.md): first run, preferences, FIND,
   choosing and checking the set, Information, Upload, both Downloads, the dialogs.
3. Runs the failures: a refused upload, a target that is not there, a silent target with the window
   closed during the operation, and every fault the mock target can inject.
4. Opens a broken copy of the set, the sample set with a warning, the loader in Hebrew (the window mirrors), in Japanese and in Chinese (the system's fonts), and the generator and the mock target in Hebrew.
5. Does it twice, in the light colours and in the dark; only the pictures marked for it in the script
   are taken in the dark (there is no dark picture of the Hebrew, Japanese and Chinese windows).

The pictures are the **client area of the window** (`PrintWindow`), so they carry no title bar
and no other program. The windows are 1280 by 900 pixels; the generator is 1280 by 1200 and the mock
target 1280 by 1320 so that their whole rail is visible. File names are stable
(`app-upload-complete.png`, `gen-ready.png`, `mock-faults.png`; the dark ones end in `-dark`).

## How the windows are driven

`tools/docs/ui_automation.ps1` is the library: Windows UI Automation finds a control by the
accessible name the program gave it or, for many of the loader's controls, by the object name the
program gives it (a table in the script maps the phrase the script uses, so that it reads like the
guide: "press *Upload the ticked loads*", to the object name or to the text now on the control),
presses buttons, types into fields the way an operator does, and drives the system's file
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
  `-Only 'faults,sets'`) run a part (the later ones reuse the media set the generator made); the
  default is all five. The script's own help describes each part (`Get-Help` with the script's
  path and `-Parameter Only`). `-Scheme light` or `dark` runs one colour scheme; `-OutDir` and
  `-WorkDir` move the output and the scratch folder (default `build/docs-screenshots`).
- **Programs.** `-BuildDir` is the directory that holds `adl_app.exe`, `adl_media_gen.exe`,
  `adl_mock_target.exe` and `adl_write_payloads.exe`; the default is
  `build/windows-msvc-release/bin`, which is where the Release preset puts them. The script stops
  with a message if one of the four is missing.
- **Size.** When Python with Pillow is installed the script re-saves each picture with a
  256-colour palette (`tools/docs/optimise_png.py`), which keeps the flat colours and the text
  sharp; `-NoOptimise` leaves them as they were taken. In `docs/images` 24 pictures are palette
  images of 36 to 57 KB (the 19 dark ones, and the loader in Hebrew, Japanese and Chinese and the
  generator and mock target in Hebrew) and 40 are truecolour images of 107 to 190 KB (the other
  light ones).

## Pictures from the test programs

The offscreen QML test programs can also save a window's picture
(`adl_app_qml_test <workbench.png> <first-run.png> <right-to-left.png> <preferences.png> <about.png>`,
`adl_gen_qml_test <window.png> <right-to-left.png>`,
`adl_mock_qml_test <window.png> <right-to-left.png>`, and `ADL_DARK=1` for the dark scheme; see
[Testing](testing.md)). The right-to-left picture of each program is the window drawn in Hebrew with
Noto Sans Hebrew. These pictures draw the windows over simulated data without a desktop, which is
handy in CI, but they are not what the guide and the pages show: those use the real programs. The
Hebrew pictures in [Translations](translations.md#right-to-left) (`app-workbench-hebrew.png`,
`gen-hebrew.png` and `mock-hebrew.png`) come from the capture script, in its `language` part
(`Invoke-LanguagePass`), not from the test programs; so do the Japanese and Chinese ones.

## Adding a picture

Add a `Save-Shot $process 'name'` call where the window shows what you want (add `-Dark` if the
dark one is wanted), run the part (`-Only`), and reference `docs/images/name.png` from the page. The
`docs.links` test fails if a page points at a picture that is not there.
