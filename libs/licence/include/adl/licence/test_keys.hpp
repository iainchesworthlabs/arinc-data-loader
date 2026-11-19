// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A throwaway key pair for the tests and for the owner's tool in its test mode. It is the key of test
// vector 1 of RFC 8032, whose private half is printed in the RFC, so it protects nothing. Its key id
// starts with "test-", which `signed_file::product_trust_list` never returns. No program links this part
// (adl_licence_test_keys).

#include "adl/licence/sign.hpp"
#include "adl/licence/signed_file.hpp"

#include <span>
#include <string_view>

namespace adl::licence::test_keys
{

inline constexpr std::string_view key_id = "test-rfc8032-1";

// THROWAWAY TEST KEY. The private key of RFC 8032, section 7.1, test 1.
[[nodiscard]] signing::Seed private_key();

// The test key as a trusted key for the `licence` and `revocations` purposes.
[[nodiscard]] std::span<const signed_file::TrustedKey> trusted();

} // namespace adl::licence::test_keys
