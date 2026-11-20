// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "adl/licence/signed_file.hpp"

// The public keys this program trusts for a licence and for a revocation list. Public halves only: a
// private key is never in this repository.
//
// The list is empty. ARINC Data Loader has no licence key yet: the owner makes a signing key and a
// standby key on their own computer, and their public halves are added here, one entry for each purpose
// (`licence` and `revocations`). Until then every licence file is refused and the programs are the
// Community edition, which is how a program with no licence behaves in any case.
//
// When a key is rotated out the entry gets a `not_after`, and when one leaks it gets a `revoked_from`
// (signed_file.hpp). The keys of another product that shares this library are not here: each product
// has keys of its own, so that a leak of one cannot touch the other.

namespace adl::licence::signed_file
{

std::span<const TrustedKey> builtin_keys()
{
    return {};
}

} // namespace adl::licence::signed_file
