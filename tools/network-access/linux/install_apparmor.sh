#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Installs (or removes) the AppArmor profiles for adl_app, adl_mock_target and adl_media_gen.
#
#   sudo ./install_apparmor.sh --bin-dir /path/to/bin [--qt-dir /path/to/Qt/6.12.0/gcc_64]
#                              [--enforce] [--print]
#   sudo ./install_apparmor.sh --remove
#
# The profiles start in COMPLAIN mode: nothing is blocked, and what an enforced profile would
# have denied is written to the system log. Run the programs through a real session (open the
# file dialogs, start the mock target, run an operation), read the log with
#   sudo aa-logprof            or     sudo journalctl -k | grep 'apparmor="ALLOWED"'
# put what is missing in /etc/apparmor.d/local/<program>, and only then pass --enforce.
#
# --print writes the finished profile to standard output, checks that the parser accepts it and
# changes nothing.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
template="$here/arinc-data-loader.apparmor"
target=/etc/apparmor.d/arinc-data-loader
bin_dir=
qt_dir=
flags="flags=(complain)"
print_only=0
remove=0

usage() { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-2}"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --bin-dir) bin_dir=${2:?--bin-dir needs a folder}; shift 2 ;;
        --qt-dir)  qt_dir=${2:?--qt-dir needs a folder};   shift 2 ;;
        --enforce) flags=""; shift ;;
        --complain) flags="flags=(complain)"; shift ;;
        --print)   print_only=1; shift ;;
        --remove)  remove=1; shift ;;
        -h|--help) usage 0 ;;
        *) echo "unknown option: $1" >&2; usage ;;
    esac
done

if [ "$remove" -eq 1 ]; then
    [ "$(id -u)" -eq 0 ] || { echo "run as root (sudo)" >&2; exit 1; }
    if [ -f "$target" ]; then
        apparmor_parser -R "$target" 2>/dev/null || true
        rm -f "$target"
        echo "removed $target"
    else
        echo "nothing installed at $target"
    fi
    exit 0
fi

[ -n "$bin_dir" ] || { echo "--bin-dir is required" >&2; usage; }
bin_dir=$(CDPATH= cd -- "$bin_dir" && pwd -P)
for program in adl_app adl_mock_target adl_media_gen; do
    [ -x "$bin_dir/$program" ] || echo "warning: $bin_dir/$program is not there (its profile is still written)" >&2
done
case "$bin_dir" in
    *[!A-Za-z0-9_./+-]*) echo "the folder name must use only letters, digits and _ . / + -" >&2; exit 1 ;;
esac

qt_rules=
if [ -n "$qt_dir" ]; then
    qt_dir=$(CDPATH= cd -- "$qt_dir" && pwd -P)
    case "$qt_dir" in
        *[!A-Za-z0-9_./+-]*) echo "the Qt folder name must use only letters, digits and _ . / + -" >&2; exit 1 ;;
    esac
    qt_rules="  $qt_dir/** mr,"
fi

# "flags=(complain)", or nothing when enforcing.
profile=$(sed -e "s#@@ADL_BIN@@#$bin_dir#g" -e "s#@@FLAGS@@#$flags#g" "$template" | awk -v rules="$qt_rules" '
    /^@@QT_RULES@@$/ { if (rules != "") print rules; next } { print }')

check=$(mktemp)
trap 'rm -f "$check"' EXIT
printf '%s\n' "$profile" > "$check"
# -Q compiles without loading anything into the kernel, -T skips the cache. The parser can print
# an error and still exit 0 with -Q, so its output is read as well as its status.
if ! messages=$(apparmor_parser -Q -T "$check" 2>&1) || [ -n "$messages" ]; then
    printf '%s
' "$messages" >&2
    echo "the profile did not parse; nothing was installed" >&2
    exit 1
fi

if [ "$print_only" -eq 1 ]; then
    printf '%s\n' "$profile"
    echo "# (syntax checked; nothing installed)" >&2
    exit 0
fi

[ "$(id -u)" -eq 0 ] || { echo "run as root (sudo)" >&2; exit 1; }
if [ "$(cat /sys/module/apparmor/parameters/enabled 2>/dev/null || echo N)" != "Y" ]; then
    echo "warning: AppArmor is not enabled in this kernel; the profiles are installed but nothing enforces them" >&2
fi
mkdir -p /etc/apparmor.d/local
for program in adl_app adl_mock_target adl_media_gen; do
    [ -e "/etc/apparmor.d/local/$program" ] || : > "/etc/apparmor.d/local/$program"
done
cp "$check" "$target"
chmod 644 "$target"
apparmor_parser -r "$target"
echo "installed $target (${flags:+complain}${flags:-enforce} mode); status: sudo aa-status | grep adl_"
