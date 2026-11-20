---
template: marketing.html
title: Privacy
title_tag: "Privacy: ARINC Data Loader"
description: What the ARINC Data Loader programs send on the network, what they keep on your computer, and what they do not collect.
layout: prose
---
# Privacy

## In one minute

The programs collect nothing. They have no account, no telemetry, no update check and no crash reporting. The data loader sends data to the target you choose, and sends a FIND request on your local network or to the address you typed. The programs keep a few settings on your computer, and the files you ask them to write where you ask. Nothing is sent to Iain Chesworth Labs or to anyone else by the programs' own code.

These pages describe what the code does. The source is open, and [How to check this](#how-to-check-this) says where to look.

## What the programs send

**The media set generator** does not use the network.

**The data loader** uses the network only when you press a button that needs it: Find, Read information, Upload or Download. Nothing is sent when it starts.

- FIND is a request of four bytes on UDP port 1001. With no address typed it goes to the broadcast address 255.255.255.255, which reaches the local network. With an address typed it goes to that address. The loader listens for answers for three seconds.
- Information, Upload and Download are TFTP over UDP, to the one target you typed or chose, on port 59. The loader sends the target's identifier (`THW_ID_POS`) as you typed it, in file names. In an upload it sends the files of the loads you ticked, and the file names and part numbers of those loads. In a download it sends the names of the files you asked for. It also answers the target on a port of its own, which is 59 unless you change it in Preferences, and on a port the system chooses for each transfer.
- It reads and sends no computer name, user name, operating system, network adapter address or program version. As with any network traffic, the target can see the address and ports the loader sends from.

**The mock target** listens only after you press Start. It answers FIND and the loader's requests on UDP ports 1001 and 59 (you can change both), and sends fixed sample data and the five FIND fields you set in its window. With "This computer only (127.0.0.1)" ticked it listens on no network.

**None of this is encrypted or authenticated.** ARINC 615A over TFTP has neither, so anything on the path between the loader and the target can read or change what is sent, and the loader does not decide that a target is genuine.

**Web pages.** A program opens a web page only when you click the licence link or the repository link in its About dialog. Your browser then makes the request, and that is between you and that site.

## What the programs keep on your computer

| What | Where | Until |
| --- | --- | --- |
| Settings. All three programs keep the position and size of the window and the language. The loader also keeps whether the first-run screen has been passed, the folder of the last media set, the default and the last target address and identifier, the control port, the download folder, whether FIND runs before an operation and whether a media set is checked when chosen, and how many lines the log keeps. | The system's per-user settings store. On Windows this is the registry key `HKEY_CURRENT_USER\Software\iainchesworthlabs\` and the program's name. On Linux and macOS the place is the one Qt uses, which has not been checked. If the environment variable `ADL_SETTINGS_DIR` names a folder when a program starts, the programs keep their settings in an ini file in it instead. | You delete them. |
| Licence files, the clock file and the notice you put away | A licence file you import is copied into your licence folder (`licences` below the system's per-user data folder, or below `ADL_SETTINGS_DIR`). It holds the licensee's name, perhaps a contact and an email address, the licence number, the edition and the dates. The clock file, `licence-clock.json` beside that folder, holds the latest time the program has seen, and is written only when there is a licence file. The notice you put away is a line in the program's settings. The loader's log and receipt, which stay in memory, and the operation record `adl_load` writes when you ask for one, say which edition the program ran as and the number of the licence in force, and nothing else of the licence. [Your licence and edition](user-guide/licence.md) says where | You remove the licence in the Licence window or with `adl_licence remove`, or delete the files. The clock file and the settings stay until you delete them |
| Media sets the generator writes | The folder you choose. It writes into a temporary folder inside it first, then renames it. | You delete them. |
| Files a download brings back | A new folder, `DNLD_DATA_<THW_ID_POS>_<number>`, in the download folder. The default download folder is `ArincDownloads` in your Documents folder, and the loader creates it at the first download. | You delete them. |
| What an upload leaves in the mock target | The storage folder you choose. It is not remembered between runs. | You delete it. |
| The activity log and the receipt | In the program's memory only. They are never written to a file. They are lost when the program closes. The Copy button puts the log on the clipboard when you press it. | The program closes. |

The settings hold paths and the last target's address. They do not hold file contents, the log or a receipt. The programs' own code writes no cache and no temporary file apart from the generator's temporary folder. Qt and your operating system may keep files of their own, for example a list of recent files for a file dialog, and these pages do not cover them.

## What the programs do not collect

No account. No telemetry or analytics. No crash reports. No check for updates. No licence check over the network: a program reads a licence file on this computer, if there is one, and sends nothing. No advertising. The only command-line option of the three programs with windows is `--smoke-test`, which draws a window once and exits. `adl_licence`, which has no window, reads and writes licence files on this computer and nothing else, and uses no network ([the command line](user-guide/command-line.md#adl_licence)).

A profiling build made from source with `ADL_ENABLE_TRACY` switched on opens a listener on the network. It is off by default, no release is built with it, and its licence notice appears only in such a build. See [Profiling](profiling.md).

## This website

These pages are served from the company's own website. The site sets no cookies, runs no analytics and loads nothing from other sites. The company's own statement is on its <a href="/privacy/">privacy page</a>. The contact form on the [Support](support.md#talk-to-us) page is the one place where the website collects what you type; <a href="/privacy/#contact-form">the privacy page</a> says what it keeps and for how long.

## How to check this

- The only sockets in the programs are the IPv4 UDP sockets of the platform layer (`libs/platform/`). No code of the programs uses a Qt network class. The windows are built on Qt's QML module, which links Qt's network library itself, and nothing in this project calls it.
- Each program's settings keys are in `apps/app/core/src/settings.cpp` and `libs/ui/window_state.cpp`.
- The messages are described in [the ARINC 615A codec](arinc615a-codec.md) and [the file reference](file-reference.md). [The security page](security.md) says what untrusted data the programs read.

## Questions

Use [Support](support.md): GitHub issues for a problem or a question, and the contact form for the paid editions.
