#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
"""An independent writer for licence files, used to make the golden data the C++ tests compare against.

It shares no code with the library. The JSON is Python's, the base64url is Python's, and the Ed25519
signatures are made by the `cryptography` package (OpenSSL), not by Monocypher. A mistake in the library has
to be matched by the same mistake here to go unseen. The format it writes is the one in
libs/licence/include/adl/licence/signed_file.hpp and licence.hpp: an envelope of five fields, and a payload
that is signed as the exact bytes it travels in, behind the message "ICL-SIGNED-FILE-1", a zero byte, the
purpose, a zero byte and the payload.

Every file is signed with the key of test vector 1 of RFC 8032, whose private half is printed in the RFC and
protects nothing. No real key is used or needed.

Run it to rewrite tests/golden/golden_licence.hpp (needs `pip install cryptography`):

    python tests/golden/make_golden_licence.py

The scenarios below are mirrored, input for input, by tests/licence/golden_test.cpp.
"""

import base64
import json
import pathlib

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

# RFC 8032, section 7.1, test 1.
SEED = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")
KEY_ID = "test-rfc8032-1"
TAG = b"ICL-SIGNED-FILE-1"


def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def compact(value):
    """The JSON the library writes: no white space, members in the order given."""
    return json.dumps(value, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def sign(purpose, payload, key_id=KEY_ID, format_number=1):
    message = TAG + b"\x00" + purpose.encode("ascii") + b"\x00" + payload
    signature = Ed25519PrivateKey.from_private_bytes(SEED).sign(message)
    envelope = {
        "format": format_number,
        "purpose": purpose,
        "key_id": key_id,
        "payload": b64url(payload),
        "signature": b64url(signature),
    }
    return compact(envelope) + b"\n"


# ---- The scenarios ------------------------------------------------------------------------------------

def annual():
    """An annual Pro licence for 13 November 2026 to 13 November 2027."""
    return {
        "product": "adl",
        "licence_id": "ADL-2026-000123",
        "kind": "annual",
        "edition": "pro",
        "licensee": {"name": "Example Lab Pty Ltd", "contact": "Test team"},
        "issued_at": "2026-11-13T02:00:00Z",
        "starts_at": "2026-11-13T00:00:00Z",
        "expires_at": "2027-11-13T00:00:00Z",
        "grace_days": 28,
        "seats": 3,
        "features": ["support"],
        "machines": [],
        "critical": [],
    }


def full():
    """An Enterprise licence with every optional field."""
    return {
        "product": "adl",
        "licence_id": "ADL-2026-000124",
        "kind": "enterprise",
        "edition": "enterprise",
        "licensee": {
            "name": "Example Avionics Ltd",
            "contact": "Certification team",
            "email": "tests@example-lab.invalid",
        },
        "issued_at": "2026-11-13T02:10:05Z",
        "starts_at": "2026-11-13T00:00:00Z",
        "expires_at": "2028-11-13T23:59:59Z",
        "grace_days": 0,
        "seats": 0,
        "features": ["support", "pro.*", "enterprise.*", "data_packs"],
        "machines": ["LAB-RIG-*", "bench-3"],
        "updates_until": "2028-11-13T23:59:59Z",
        "replaces": "ADL-2025-000007",
        "clock_reset": "reset-2026-11-13-a",
        "critical": [],
    }


def revocations():
    return {
        "sequence": 7,
        "issued_at": "2027-02-01T00:00:00Z",
        "licences": ["ADL-2026-000123"],
        "keys": [{"key_id": "lic-2026-a", "from": "2027-01-15T00:00:00Z"}, {"key_id": "lic-2025-a"}],
        "critical": [],
    }


def scenarios():
    annual_file = sign("licence", compact(annual()))
    out = {
        "licence_annual": annual_file,
        "licence_full": sign("licence", compact(full())),
        "revocations_list": sign("revocations", compact(revocations())),
    }

    # One character of the signed payload changed; the signature is the old one.
    envelope = json.loads(annual_file)
    text = envelope["payload"]
    payload = bytearray(base64.urlsafe_b64decode(text + "=" * (-len(text) % 4)))
    payload[payload.index(b"28")] = ord("9")
    envelope["payload"] = b64url(bytes(payload))
    out["licence_bad_signature"] = compact(envelope) + b"\n"

    # The format number is one this version does not read.
    out["licence_newer_format"] = sign("licence", compact(annual()), format_number=2)

    # Signed, as a licence, by a key the trust list has not got.
    out["licence_unknown_key"] = sign("licence", compact(annual()), key_id="lic-2099-z")

    # Payloads on their own, for the readers that sit behind the signature.
    text = compact(annual()).decode("ascii")
    out["licence_duplicate_key_payload"] = text.replace('"seats":3', '"seats":3,"seats":900').encode("ascii")
    out["licence_critical_unknown_payload"] = text.replace(
        '"critical":[]', '"critical":["max_ports"],"max_ports":4').encode("ascii")
    out["licence_annual_payload"] = compact(annual())
    out["revocations_list_payload"] = compact(revocations())
    out["licence_time_text"] = b"2026-11-13T02:10:05Z"

    # The clock file the store keeps (libs/licence store.hpp): what the program has seen, and the reset tokens
    # it has used. Written here with the standard JSON writer, in the order and the spacing the library uses.
    clock = {"format": 1, "latest_seen": "2026-11-13T02:10:05Z",
             "applied_resets": ["reset-2026-11-13-a", "reset-2027-01-05-b"]}
    out["clock_seen"] = (json.dumps(clock) + "\n").encode("ascii")
    out["clock_empty"] = (json.dumps({"format": 1, "applied_resets": []}) + "\n").encode("ascii")
    return out


def cpp_literal(name, data):
    """`inline constexpr std::string_view <name>_hex = "...";`, wrapped at 100 columns."""
    hex_text = data.hex()
    one_line = f'inline constexpr std::string_view {name}_hex = "{hex_text}";'
    if len(one_line) <= 120:  # clang-format joins a constant that fits, so the file is written that way
        return one_line + "\n"
    indent = "    "
    width = 100 - len(indent) - 2
    lines = [hex_text[i:i + width] for i in range(0, len(hex_text), width)] or [""]
    body = "\n".join(indent + '"' + line + '"' for line in lines)
    return f"inline constexpr std::string_view {name}_hex =\n{body};\n"


def main():
    out = pathlib.Path(__file__).resolve().parent / "golden_licence.hpp"
    # The header's own licence line is written in two pieces, so that the licence checker, which reads a
    # line like it in any file, does not take the text of this script for the licence of the script.
    spdx = "// SPDX-" + "License-Identifier: GPL-3.0-or-later OR BSD-2-Clause\n"
    parts = [
        spdx + "#pragma once\n\n"
        "// Generated by tests/golden/make_golden_licence.py. Do not edit by hand.\n\n"
        "// Each is the hexadecimal of a licence file, a revocation list, a clock file or a payload written\n"
        "// independently of the library. The signed ones are signed with the published test key of RFC 8032\n"
        "// (test vector 1).\n\n"
        "#include <string_view>\n\n"
        "namespace adl::test::golden\n{\n"
    ]
    for name, data in scenarios().items():
        parts.append(cpp_literal(name, data))
    parts.append("} // namespace adl::test::golden\n")
    out.write_text("\n".join(parts), encoding="utf-8", newline="\n")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
