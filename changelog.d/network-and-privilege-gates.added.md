**Network and privilege gates.** The mock target has a **This computer only (127.0.0.1)** setting
(`Config::bind_address`), and the loader binds loopback when its target is a loopback address
(`RunOptions::local_address`): such sockets raise no firewall prompt, and the loopback integration
test uses them, so it no longer stops an unattended run on a Windows prompt. A port that is refused
with *permission denied* now says that a port below 1024 may need privilege.
`tools/network-access/` has a Windows Firewall rule helper (`adl_firewall.ps1`: UDP only, Private
profile, local subnet, per program path), an AppArmor profile and installer for Linux, and a macOS
application-firewall helper; [Installing](docs/user-guide/installing.md#what-the-firewall-asks)
explains the firewall, `CAP_NET_BIND_SERVICE` and SELinux.
