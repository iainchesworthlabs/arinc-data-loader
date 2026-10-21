# Network access helpers

What the operating systems ask before an ARINC Data Loader program may listen on a UDP port, and
the scripts that answer it ahead of time. The explanation, with what was and was not tested, is in
[Installing](../../docs/user-guide/installing.md#what-the-firewall-asks).

| Path | For |
| --- | --- |
| `windows/adl_firewall.ps1` | Windows Defender Firewall: one inbound UDP rule per program path, Private profile, local subnet. Run from an elevated PowerShell. `-Action Show`, `-Action Remove`, `-WhatIf` |
| `linux/install_apparmor.sh`, `linux/arinc-data-loader.apparmor` | AppArmor profiles for the three programs, installed in complain mode first. `--print` checks the profile without installing it |
| `macos/adl_macos_firewall.sh` | macOS application firewall: register the programs so it does not ask. Not tested on a Mac |

The scripts change system settings only when you run them with the rights they ask for, and none of
the programs runs them. Binding only the loopback address (the mock target's *This computer only*)
avoids the questions altogether for a demonstration on one machine.
