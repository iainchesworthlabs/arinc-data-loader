#!/usr/bin/env bash
#
# Self-test for sign_release.sh and verify_release.sh, with throw-away keys made here (the release key
# is never involved): a release is signed and verified; a signature on a changed file, a file missing
# from SHA256SUMS and a secret that holds some other key each fail. The release workflow runs it before
# it trusts the two scripts with the real key.
#
#   tools/release/test_signing.sh

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
work=$(mktemp -d)
trap 'for h in "$work"/home-*; do GNUPGHOME="$h" gpgconf --kill gpg-agent 2>/dev/null || true; done; rm -rf "$work"' EXIT

make_key() {
    local name=$1 passphrase=$2
    local home="$work/home-$name"
    mkdir -m 700 "$home"
    GNUPGHOME="$home" gpg --batch --quiet --pinentry-mode loopback --passphrase "$passphrase" \
        --quick-generate-key "Test $name <$name@example.invalid>" ed25519 sign,cert 1d 2>/dev/null
    GNUPGHOME="$home" gpg --batch --armor --export > "$work/$name.pub.asc"
    GNUPGHOME="$home" gpg --batch --pinentry-mode loopback --passphrase "$passphrase" \
        --armor --export-secret-keys > "$work/$name.sec.asc" 2>/dev/null
}

count=0
failures=0
expect() { # expect <description> <expected status: 0 or non-zero> <command...>
    local description=$1 expected=$2
    shift 2
    count=$((count + 1))
    local status=0
    "$@" >"$work/out.txt" 2>&1 || status=$?
    if { [ "$expected" = 0 ] && [ "$status" -eq 0 ]; } || { [ "$expected" != 0 ] && [ "$status" -ne 0 ]; }; then
        echo "  ok    $description"
    else
        echo "  FAIL  $description (exit $status)"
        sed 's/^/        /' "$work/out.txt"
        failures=$((failures + 1))
    fi
}

make_key release secret-one
make_key other secret-two

fresh_release() {
    rm -rf "$work/release" && mkdir "$work/release"
    printf 'package one\n' > "$work/release/package-one.zip"
    printf 'package two\n' > "$work/release/package-two.tar.gz"
    printf '{"sbom": true}\n' > "$work/release/sbom.spdx.json"
}

sign() { # sign <key name> <passphrase>
    (cd "$work" && GPG_PRIVATE_KEY=$(cat "$work/$1.sec.asc") GPG_PASSPHRASE=$2 \
        "$here/sign_release.sh" "$work/release" "$work/release.pub.asc")
}
verify() { "$here/verify_release.sh" "$work/release" "$work/release.pub.asc" --no-attestation; }

fresh_release
expect "signing with the release key works" 0 sign release secret-one
expect "the signed release verifies" 0 verify
[ -f "$work/release/SHA256SUMS.asc" ] && [ -f "$work/release/package-one.zip.asc" ] || { echo "  FAIL  a signature is missing"; failures=$((failures + 1)); }

printf 'tampered\n' >> "$work/release/package-one.zip"
expect "a changed file fails to verify" 1 verify

fresh_release
expect "signing again works" 0 sign release secret-one
printf 'extra\n' > "$work/release/not-listed.txt"
(cd "$work/release" && GNUPGHOME="$work/home-release" gpg --batch --quiet --pinentry-mode loopback --passphrase secret-one --armor --detach-sign --output not-listed.txt.asc not-listed.txt)
expect "a signed file that is not in SHA256SUMS fails" 1 verify

fresh_release
expect "a secret holding another key signs nothing" 1 sign other secret-two
expect "a wrong passphrase signs nothing" 1 sign release not-the-passphrase

echo
if [ "$failures" -gt 0 ]; then
    echo "$failures of $count case(s) failed"
    exit 1
fi
echo "all $count case(s) behaved"
