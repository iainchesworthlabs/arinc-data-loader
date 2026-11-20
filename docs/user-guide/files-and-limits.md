# Where files go, and limits

## Where files go

| What | Where |
| --- | --- |
| A media set the **generator** writes | `<output folder>/<media set part number>/member_001/...` (one `member_NNN` per member). If that folder is not empty, `<part number>_2`, `_3` ... is used. Nothing is ever overwritten |
| Files a **download** fetches | `<download folder>/DNLD_DATA_<THW_ID_POS>_<number>/`. The download folder is a [Preference](preferences.md) (default `Documents/ArincDownloads`). Each download makes a new folder; `<number>` is the lowest free one starting at 1, and a download that brings back no file removes its empty folder again, so its number is free again. A file is written under a temporary name ending in `.part` and renamed when it has arrived whole; an existing file is never overwritten |
| What an upload leaves in the **mock target** | `<storage folder>/<load part number>/<file name>` |
| What a download can fetch from the mock target | the files in `<storage folder>/holds`, read when the target is started. You make the `holds` folder yourself; the mock target does not |
| The loader's **activity log** | In memory only. It is not written to a file, it is lost when the program closes, and it keeps the last 2000 lines unless you change that in Preferences. Copy what you need before closing |
| **Settings** (Preferences, last media set, last target, language, window size and position, whether the first-run screen has been passed) | The system's per-user settings store, one per program. On Windows: `HKEY_CURRENT_USER\Software\iainchesworthlabs\ARINC Data Loader` (and `ARINC Media Set Generator`, `ARINC Mock Target`). To start from the first launch again, delete the key (`reg delete "HKCU\Software\iainchesworthlabs\ARINC Data Loader" /f`) while the program is closed. Elsewhere the place is the one Qt's settings use on that system, and has not been checked. If the environment variable `ADL_SETTINGS_DIR` names a folder when a program starts, all three programs keep their settings in an ini file below that folder instead, and write nothing to the usual place |

The programs do not send anything anywhere except to the target (and the FIND broadcast on the
local network): there is no telemetry, no update check and no account. They open a web page (the
GNU licence or the project's repository) only when you click such a link in About.

## Limits

The limits of the programs (the members in a media set, the length of a file name and of a `THW_ID_POS`, the targets one FIND lists, the time-outs, the ports and the rest) are in one table on [Limits and known issues](limits.md#limits).

## What has not been checked

This is an early release. Nothing has been checked against real equipment, another vendor's loader or target, or another tool's media sets. [What has been checked](../status.md) says what has been checked at each level, and [Limits and known issues](limits.md#not-checked) lists what has not been, with the choices where the standards are unclear. [Safety of use](safety-of-use.md) says what not to rely on the programs for.
