#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Tells the macOS application firewall to let the ARINC Data Loader programs accept incoming
# UDP, so that "Do you want the application to accept incoming network connections?" is not
# asked (it is asked once per program, the first time it listens on the network, when the
# firewall is on and the program is not signed by a trusted developer).
#
#   sudo ./adl_macos_firewall.sh add    /path/to/adl_app.app [/path/to/adl_mock_target.app ...]
#   sudo ./adl_macos_firewall.sh remove /path/to/adl_app.app ...
#   ./adl_macos_firewall.sh status
#
# Give the .app bundle or the executable inside it. This only registers the program with the
# firewall (socketfilterfw --add, --unblockapp); it does not turn the firewall on or off, and it
# opens no port. Not needed for a loader and a mock target on the same Mac that both use
# 127.0.0.1: the firewall does not see loopback traffic.
#
# NOT TESTED by the project's authors on a Mac: written from Apple's socketfilterfw manual page.
set -eu

fw=/usr/libexec/ApplicationFirewall/socketfilterfw
[ -x "$fw" ] || { echo "$fw not found: this is not macOS" >&2; exit 1; }

action=${1:-status}
[ $# -gt 0 ] && shift

case "$action" in
    status)
        "$fw" --getglobalstate
        "$fw" --getstealthmode
        "$fw" --listapps | grep -i -A1 'adl_' || echo "no adl_ program is listed"
        ;;
    add|remove)
        [ $# -gt 0 ] || { echo "name at least one program" >&2; exit 2; }
        [ "$(id -u)" -eq 0 ] || { echo "run as root (sudo)" >&2; exit 1; }
        for app in "$@"; do
            [ -e "$app" ] || { echo "not found: $app" >&2; exit 1; }
            if [ "$action" = add ]; then
                "$fw" --add "$app"
                "$fw" --unblockapp "$app"
            else
                "$fw" --remove "$app"
            fi
        done
        ;;
    *)
        echo "usage: $0 add|remove <program>... | status" >&2
        exit 2
        ;;
esac
