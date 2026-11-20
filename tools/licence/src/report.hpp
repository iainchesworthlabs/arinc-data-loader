// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// What the commands print about a file: `Name: value` lines, one fact on each, so that a script can read
// them. The names of the lines are part of the tool's interface (docs/licence-owner-tool.md).

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

// How a signature stands against the keys the tool was given.
struct SignatureCheck
{
    enum class Kind
    {
        verifies,
        does_not_verify, // `detail` says why
        no_key,          // no key with this id and purpose is known
    };
    Kind kind = Kind::no_key;
    std::string detail;
    // The key that verified it, a copy; meaningful when `kind` is `verifies`.
    adl::licence::signed_file::TrustedKey key;
};

// Checks the envelope against `keys` for `purpose`, then the key's window against `issued_at`, which the
// caller read from the payload (null when the payload is not readable: the window is then not checked).
[[nodiscard]] SignatureCheck check_signature(const adl::licence::signed_file::Envelope& file,
                                             std::string_view purpose,
                                             std::span<const adl::licence::signed_file::TrustedKey> keys,
                                             const adl::licence::TimePoint* issued_at);

// Whether the product's own trust list (no test key, no --trust key) has this key.
[[nodiscard]] bool trusted_by_this_build(const adl::licence::signed_file::TrustedKey& key);

// The name of a reason for a program to read: ok, expired, revoked, wrong_computer ...
[[nodiscard]] std::string_view reason_name(adl::licence::Reason value) noexcept;

void print_signature(std::ostream& out, const SignatureCheck& check);
void print_licence(std::ostream& out, const adl::licence::Licence& value);
void print_revocations(std::ostream& out, const adl::licence::RevocationList& value);

} // namespace adl::licence_owner
