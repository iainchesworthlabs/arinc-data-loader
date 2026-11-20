# Configuration reference

The programs keep a few settings between runs. This page lists every one: its name in the settings store, what it holds, its default and its range. The windows in which you change them are described in [Preferences and the other dialogs](preferences.md).

## Where the settings are kept

Each program keeps its settings under the organisation name `iainchesworthlabs` and its own name:

| Program | Name |
| --- | --- |
| The data loader | `ARINC Data Loader` |
| The media set generator | `ARINC Media Set Generator` |
| The mock target | `ARINC Mock Target` |

The place is the system's per-user settings store:

- **Windows:** the registry key `HKEY_CURRENT_USER\Software\iainchesworthlabs\` and the program's name.
- **Linux and macOS:** the place that Qt uses for the system. This has not been checked.
- **A folder of your choosing:** if the environment variable `ADL_SETTINGS_DIR` names a folder when a program starts, all three programs read and write an ini file below that folder, and nothing in the usual place. Set it before the program starts. The programs make the folder if it is not there.

Every change is written at once.

| To | Do this |
| --- | --- |
| Start a program from its first launch again | Close it, then delete its key. On Windows, in a command prompt: `reg delete "HKCU\Software\iainchesworthlabs\ARINC Data Loader" /f` |
| Use no settings of your own | Set `ADL_SETTINGS_DIR` to an empty folder. |

## What all three programs keep

| Key | Holds |
| --- | --- |
| `window/x`, `window/y`, `window/width`, `window/height` | The position and size of the window, written when it closes |
| `window/maximized`, `window/placed` | Whether the window was maximised, and whether it has been placed |
| `language/code` | The language you chose |
| `licence/dismissed` | The licence notice you put away: the licence number and its state, such as `ADL-2026-000123\|expiring` |
| `licence/remindAfter` | The day on which you chose **Remind me tomorrow** for a licence in its grace period |

## What the data loader also keeps

| Preferences name | Key | Default | Range |
| --- | --- | --- | --- |
| (the first-run screen) | `app/firstRunComplete` | false | true or false |
| Last media set | `mediaSet/lastDirectory` | empty | A folder path |
| Default address | `target/defaultAddress` | empty | An IPv4 address, as text |
| Default THW_ID_POS | `target/defaultThwIdPos` | empty | Text, 1 to 240 characters when used |
| (the last target) | `target/lastAddress`, `target/lastThwIdPos` | empty until an operation has been started | As above |
| Control port | `loader/controlPort` | 59 | 1 to 65535 |
| Download folder | `download/directory` | `ArincDownloads` in your Documents folder (in your home folder if there is no Documents folder) | A folder path |
| Find the target automatically | `target/autoFindBeforeOperation` | off | on or off |
| Check the media set as soon as it is chosen | `mediaSet/validateOnOpen` | on | on or off |
| Activity lines kept | `log/capacity` | 2000 | 100 to 1 000 000 |

A value outside its range is not accepted by the window. If one in the store is outside its range, the default is used.

## What the generator and the mock target do not keep

The generator and the mock target keep only the window and the language. Their other fields, such as the output folder, the ports and the faults, are set each time you start them. [Make a media set](make-a-media-set.md) and [Run the mock target](mock-target.md) list them, with their defaults.

## Licence files

The licence folders, the clock file and the limits on them (64 files from each folder, 64 KiB for a file) are on
[Your licence and edition](licence.md#where-licence-files-go). A licence is never read from anywhere else.

## Environment variables

| Variable | Used by | What it does |
| --- | --- | --- |
| `ADL_SETTINGS_DIR` | All three programs | Keeps the settings in an ini file below this folder, as above, and keeps the [licence folders and the clock file](licence.md#where-licence-files-go) there too: `licences`, `machine-licences` and `licence-clock.json` |
| `QT_QUICK_CONTROLS_STYLE` | All three programs | If it is not set, the programs use Qt's Fusion style. If it is set, Qt's own value is used |

The programs read no other environment variable. Qt reads its own.

## Command-line options

See [Command line and exit codes](command-line.md).
