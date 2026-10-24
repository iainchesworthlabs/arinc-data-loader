#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks a downloaded release the way docs/verifying-releases.md describes: the PGP signatures against
# the organisation's release key, the SHA-512 sums, and (with the GitHub CLI) the build-provenance
# attestation of every file.
#
#   tools/release/verify_release.sh <directory> [<public key file>] [--no-attestation]
#
# <directory> holds the files of one release as downloaded: the packages, SHA512SUMS, the SBOMs, and a
# .asc beside each. The public key defaults to docs/signing-key.asc beside this script's
# repository; its fingerprint is printed so it can be compared with SECURITY.md by eye. The attestation
# check needs `gh` and a network; the release workflow runs the same script on what it just produced.
#
# Exit 0 = everything checked is good.

set -euo pipefail

repository=${ADL_REPOSITORY:-iainchesworthlabs/arinc-data-loader}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
attestation=1
args=()
for argument in "$@"; do
    case "$argument" in
        --no-attestation) attestation=0 ;;
        *) args+=("$argument") ;;
    esac
done
if [ "${#args[@]}" -lt 1 ]; then
    echo "usage: verify_release.sh <directory> [<public key file>] [--no-attestation]" >&2
    exit 2
fi
directory=${args[0]}
public_key=${args[1]:-$here/../../docs/signing-key.asc}

work=$(mktemp -d)
trap 'GNUPGHOME="$work" gpgconf --kill gpg-agent 2>/dev/null || true; rm -rf "$work"' EXIT
chmod 700 "$work"
export GNUPGHOME=$work

gpg --batch --quiet --import "$public_key"
fingerprint=$(gpg --batch --list-keys --with-colons | awk -F: '/^fpr/ { print $10; exit }')
echo "release key: $fingerprint"

cd "$directory"
failed=0

echo "-- signatures"
for signature in *.asc; do
    file=${signature%.asc}
    if gpg --batch --quiet --status-fd 1 --verify "$signature" "$file" 2>/dev/null | grep -q "^\[GNUPG:\] VALIDSIG $fingerprint "; then
        echo "good: $file"
    else
        echo "BAD signature: $file" >&2
        failed=1
    fi
done

echo "-- checksums"
if ! sha512sum --check --strict SHA512SUMS; then
    failed=1
fi
# Every file that is not a signature or the sums themselves must be listed in them.
for file in *; do
    case "$file" in SHA512SUMS | *.asc) continue ;; esac
    if ! grep -q -- "  $file\$" SHA512SUMS; then
        echo "NOT LISTED in SHA512SUMS: $file" >&2
        failed=1
    fi
done

if [ "$attestation" -eq 1 ]; then
    echo "-- build-provenance attestations"
    for file in *; do
        case "$file" in SHA512SUMS | *.asc) continue ;; esac
        if gh attestation verify "$file" --repo "$repository" >/dev/null 2>"$work/attestation.log"; then
            echo "attested: $file"
        else
            echo "NOT ATTESTED: $file" >&2
            cat "$work/attestation.log" >&2
            failed=1
        fi
    done
fi

if [ "$failed" -ne 0 ]; then
    echo "verification FAILED" >&2
    exit 1
fi
echo "everything checked is good"
