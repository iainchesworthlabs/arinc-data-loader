// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The public key file that keygen writes (`<key id>.adlownerpub`): one line of JSON,
//
//   {"key_id":"lic-2026-a","purpose":"licence","public_key":"<43 characters of base64url>"}
//
// It holds nothing secret. `inspect` and `verify` take such a file with --trust. The reader accepts exactly
// the form the writer produces (and one line end after it), so that two readers cannot disagree about it.

#include "adl/licence/signed_file.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

// The key id of a key file: lower-case letters, digits and hyphens, 64 characters at most.
[[nodiscard]] bool is_key_id(std::string_view text) noexcept;

// The text of the file for `key`, with its line end.
[[nodiscard]] std::string format_public_key(const adl::licence::signed_file::TrustedKey& key);

// The key a file holds, or why not (a sentence from messages.hpp).
[[nodiscard]] std::expected<adl::licence::signed_file::TrustedKey, std::string> parse_public_key(std::string_view text);

} // namespace adl::licence_owner
