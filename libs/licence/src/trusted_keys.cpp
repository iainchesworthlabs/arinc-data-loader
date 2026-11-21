// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "adl/licence/signed_file.hpp"

#include <array>

// The public keys this program trusts for a licence and for a revocation list. Public halves only: a
// private key is never in this repository.
//
// ARINC Data Loader has two signing keys and two standbys, made by the owner with `adl_licence_owner keygen`
// (docs/licence-owner-tool.md): `lic-2026-a` signs licences and `rev-2026-a` signs revocation lists, and
// `lic-2026-b` and `rev-2026-b` are the standbys, in the list from the start so that a leak of the first
// pair can be answered with a release that marks it `revoked_from`, at once and without a new key having to
// reach every program first.
//
// When a key is rotated out the entry gets a `not_after`, and when one leaks it gets a `revoked_from`
// (signed_file.hpp). The keys of another product that shares this library are not here: each product
// has keys of its own, so that a leak of one cannot touch the other.

namespace adl::licence::signed_file
{

std::span<const TrustedKey> builtin_keys()
{
    static const std::array<TrustedKey, 4> keys{{
        // lic-2026-a: licence. Made on 2026-11-21 with adl_licence_owner keygen; the private half is the owner's alone.
        TrustedKey{"lic-2026-a", "licence", {0x8e, 0x4d, 0xa2, 0x74, 0xde, 0x8d, 0xbf, 0xa6, 0x2c, 0x64, 0xb6,
                                             0x4a, 0x0b, 0x6a, 0x36, 0x9b, 0x1b, 0xf7, 0xf6, 0x60, 0xfc, 0x00,
                                             0xf6, 0xaa, 0x1b, 0x63, 0x5a, 0xd1, 0x80, 0xea, 0x53, 0x55}},
        // rev-2026-a: revocations. Made on 2026-11-21 with adl_licence_owner keygen; the private half is the owner's
        // alone.
        TrustedKey{"rev-2026-a", "revocations", {0xac, 0xe6, 0x66, 0x57, 0x83, 0x67, 0x05, 0x1b, 0x5e, 0xc7, 0x37,
                                                 0x90, 0x8e, 0xde, 0xa9, 0xb2, 0x41, 0xe4, 0x5a, 0x40, 0x31, 0xb4,
                                                 0x93, 0x44, 0xa7, 0x6a, 0x8a, 0xb2, 0x0a, 0xa3, 0x19, 0xf6}},
        // lic-2026-b: licence. Made on 2026-11-21 with adl_licence_owner keygen; the private half is the owner's alone.
        TrustedKey{"lic-2026-b", "licence", {0x46, 0x9e, 0xc3, 0x9a, 0xd1, 0xf8, 0x90, 0x09, 0x42, 0xdc, 0x99,
                                             0x64, 0x69, 0xbc, 0x9e, 0xff, 0xf5, 0x95, 0x11, 0x26, 0x3f, 0xb9,
                                             0x32, 0x0b, 0x90, 0x70, 0x27, 0x82, 0x1d, 0xb2, 0x8c, 0x06}},
        // rev-2026-b: revocations. Made on 2026-11-21 with adl_licence_owner keygen; the private half is the owner's
        // alone.
        TrustedKey{"rev-2026-b", "revocations", {0x07, 0x97, 0x72, 0x4f, 0xfe, 0xa1, 0xa5, 0x23, 0xd6, 0x4a, 0xb4,
                                                 0xc5, 0x0f, 0xe1, 0x00, 0x7b, 0x77, 0x2e, 0xf2, 0x9f, 0x6b, 0xa6,
                                                 0x51, 0x0d, 0x34, 0x50, 0x66, 0x81, 0xe2, 0x8d, 0x17, 0x37}},
    }};
    return keys;
}

} // namespace adl::licence::signed_file
