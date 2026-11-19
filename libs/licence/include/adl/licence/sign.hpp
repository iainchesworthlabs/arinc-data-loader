// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Signing a file. Only the owner's tool and the tests link this part (adl_licence_sign); the programs
// verify and never sign.

#include "adl/licence/error.hpp"
#include "adl/licence/signed_file.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace adl::licence::signing
{

// An Ed25519 private key in its 32 byte seed form (RFC 8032).
using Seed = std::array<std::uint8_t, 32>;

// The public half of a private key.
[[nodiscard]] signed_file::PublicKey public_key_of(const Seed& private_key);

// Signs `payload` for `purpose` and returns the envelope, ready to write with `signed_file::write`. The
// expanded key is wiped before returning.
[[nodiscard]] signed_file::Envelope sign(std::string_view purpose, std::string_view key_id,
                                         std::span<const std::uint8_t> payload, const Seed& private_key);

// A private key as text: base64url of the 32 bytes, 43 characters. White space around it is ignored, so a
// key file may end with a new line.
[[nodiscard]] Result<Seed> parse_seed(std::string_view text);

// The text `parse_seed` reads. The caller wipes it after use.
[[nodiscard]] std::string format_seed(const Seed& private_key);

// Overwrites key material so that it does not stay in memory.
void wipe(Seed& private_key) noexcept;
void wipe(std::string& text) noexcept;

} // namespace adl::licence::signing
