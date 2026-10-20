# Installing and starting the programs

## Getting the programs

There is no installer or download package yet. The programs are built from the source code, once,
on the machine that will use them; [Building](../building.md) lists what is needed (CMake, Ninja,
a C++23 compiler and the Qt 6.12 libraries) and gives the commands. A Release build is the one to
use for work: `windows-msvc-release` on Windows, `linux-gcc-release` on Linux and
`macos-clang-release` on a Mac. The Debug builds are for developers and, on Windows, cannot open
the system's file dialogs (see [Building](../building.md#presets)).

The build produces these programs in `build/<preset>/bin`:

| File | Program |
| --- | --- |
| `adl_app` | ARINC Data Loader |
| `adl_media_gen` | ARINC Media Set Generator |
| `adl_mock_target` | ARINC Mock Target |
| `adl_write_payloads` | A command-line tool that writes sample media sets to a folder, for trying other loaders |

On Windows the build copies the Qt libraries next to the programs, so each can be started from
anywhere: double-click it, or run it from a shell. On Linux they find Qt the way the build did; on a
Mac they are application bundles and are not yet packaged for moving to another Mac.

Starting a program opens its window; there are no command-line options. Closing the window ends the
program (if the loader is busy it asks first, see
[the loader](loader.md#closing-the-window-during-an-operation)).

## The first time you start each program

**The loader** opens on a *first-run* screen: a short statement of what the window does, a button
**Start loading** that opens the workbench, a second button **Choose a media set...** that
opens the workbench and the folder chooser together, and a folded **Defaults** section (**Set up
defaults**) where you can type a default target address, a default `THW_ID_POS` and a download
folder. Everything can be changed later in [Preferences](preferences.md). You see the first-run
screen once; every later start goes straight to the workbench.

![The loader's first-run screen](../images/app-first-run.png)

**The generator and the mock target** open straight on their only window.

## What the firewall asks

The programs talk over UDP. The mock target listens on ports 59 and 1001 for as long as it is
serving; the loader listens only while it works (on port 1001 during a FIND, and on its control
port during an operation, because the target comes back to the loader for the files of an
upload). The first time a program opens such a port, Windows Defender Firewall (or the firewall of
your system) may ask whether to allow it to communicate on private and public networks.

- Traffic between two programs on **the same machine** (the demonstration) does not need the
  firewall to allow anything, because it never leaves the machine. If the prompt appears, you can
  answer it either way for the demonstration.
- To load a **target on another machine**, the loader must be able to send to the target's
  address, and the target's answer must be allowed back in. Allow the loader on the network the
  target is on (normally *private*). The firewall prompt is the operating system's decision and
  belongs to you; the programs never change firewall settings themselves.
- UDP port **59** (control) and **1001** (FIND) are the standard's ports. On Linux and macOS a
  program may need extra privilege to listen below port 1024; the mock target's ports and the
  loader's control port can be changed, see [Troubleshooting](troubleshooting.md).

## Windows that remember

Each program remembers its window size and position between runs, and the loader remembers its
[Preferences](preferences.md), the last media set folder and the last target. Where this is kept
depends on the system (on Windows, the registry under
`HKEY_CURRENT_USER\Software\iainchesworthlabs\`); see
[Where files go](files-and-limits.md#where-files-go).

## The colours

The windows follow the desktop's light or dark setting: there is no switch inside the programs.
See [Languages and colour](languages-and-colour.md).

Next: [the demonstration](demonstration.md).
