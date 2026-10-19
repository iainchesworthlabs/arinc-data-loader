#!/usr/bin/env bash
#
# Writes SHA256SUMS for a directory of release files and signs every file in it with the release
# key: one detached, ASCII-armoured PGP signature per file, <file>.asc.
#
#   GPG_PRIVATE_KEY=<armoured secret key> GPG_PASSPHRASE=<its passphrase> \
#       tools/release/sign_release.sh <directory> <public key file>
#
# The key and passphrase come from the environment (the release workflow's signing environment), never
# from arguments, so they are not in a process list; the key is imported into a throw-away keyring that
# is removed on exit. Before anything is signed the imported key's fingerprint must be the one of the
# public key committed to the repository (docs/release-signing-key.asc), so a secret that holds some
# other key signs nothing. After signing, every signature is checked in a second, empty keyring that has
# only the public key, which is what a user does.
#
# Signed: every regular file in <directory>, SHA256SUMS included. Exit 0 = all signed and verified.

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: sign_release.sh <directory> <public key file>" >&2
    exit 2
fi
directory=$1
public_key=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
: "${GPG_PRIVATE_KEY:?GPG_PRIVATE_KEY is not set}"
: "${GPG_PASSPHRASE:?GPG_PASSPHRASE is not set}"

cd "$directory"

work=$(mktemp -d)
cleanup() {
    GNUPGHOME="$work/signing" gpgconf --kill gpg-agent 2>/dev/null || true
    GNUPGHOME="$work/verifying" gpgconf --kill gpg-agent 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT
mkdir -m 700 "$work/signing" "$work/verifying"

expected=$(GNUPGHOME="$work/verifying" gpg --batch --show-keys --with-colons "$public_key" | awk -F: '/^fpr/ { print $10; exit }')
if [ -z "$expected" ]; then
    echo "::error::no key in $public_key" >&2
    exit 1
fi

# The checksums first, over everything that is to be published; signatures are made over the files
# and over the checksums, and are not themselves listed.
find . -maxdepth 1 -type f ! -name 'SHA256SUMS' ! -name '*.asc' -printf '%f\n' | LC_ALL=C sort > "$work/files"
if [ ! -s "$work/files" ]; then
    echo "::error::no files in $directory" >&2
    exit 1
fi
xargs -d '\n' sha256sum -- < "$work/files" > SHA256SUMS
sha256sum --check --strict --quiet SHA256SUMS

# Import the secret key (its passphrase is asked for when it is used, not when it is imported).
printf '%s\n' "$GPG_PRIVATE_KEY" | GNUPGHOME="$work/signing" gpg --batch --quiet --import
actual=$(GNUPGHOME="$work/signing" gpg --batch --list-secret-keys --with-colons | awk -F: '/^fpr/ { print $10; exit }')
if [ "$actual" != "$expected" ]; then
    echo "::error::the signing key is $actual, not $expected, the key in $2" >&2
    exit 1
fi

find . -maxdepth 1 -type f ! -name '*.asc' -printf '%f\n' | LC_ALL=C sort > "$work/to-sign"
while IFS= read -r file; do
    printf '%s' "$GPG_PASSPHRASE" | GNUPGHOME="$work/signing" gpg --batch --yes --quiet \
        --pinentry-mode loopback --passphrase-fd 0 \
        --local-user "$expected" --armor --detach-sign --output "$file.asc" -- "$file"
done < "$work/to-sign"

# What a user does: only the public key, nothing else.
GNUPGHOME="$work/verifying" gpg --batch --quiet --import "$public_key"
failed=0
while IFS= read -r file; do
    if ! GNUPGHOME="$work/verifying" gpg --batch --quiet --status-fd 1 --verify "$file.asc" "$file" 2>/dev/null \
            | grep -q "^\[GNUPG:\] VALIDSIG $expected "; then
        echo "::error::the signature of $file does not verify with $expected" >&2
        failed=1
    fi
done < "$work/to-sign"
[ "$failed" -eq 0 ] || exit 1

echo "signed $(wc -l < "$work/to-sign") file(s) with $expected"
