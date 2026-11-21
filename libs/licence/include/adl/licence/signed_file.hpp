// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The signed envelope that licence files and revocation lists share.
//
//   { "format": 1, "purpose": "...", "key_id": "...",
//     "payload": "<base64url>", "signature": "<base64url>" }
//
// The payload travels as the exact bytes that were signed. A caller verifies first and parses the payload
// second, so no parser reads unsigned data beyond these five fields.

#include "adl/licence/error.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence::signed_file
{

// A file larger than this is refused before it is parsed.
inline constexpr std::size_t max_file_size = std::size_t{64} * 1024;

// The envelope format this version reads and writes.
inline constexpr int current_format = 1;

// A key id with this prefix is a throwaway for tests. product_trust_list never returns one.
inline constexpr std::string_view test_key_prefix = "test-";

using PublicKey = std::array<std::uint8_t, 32>;
using Signature = std::array<std::uint8_t, 64>;
using TimePoint = std::chrono::sys_seconds;

// A public key the program trusts, for one purpose only.
struct TrustedKey
{
    std::string key_id;
    std::string purpose; // "licence" or "revocations"
    PublicKey key{};
    // The window of signing times (`issued_at`) the key may sign for.
    TimePoint not_before = TimePoint::min();
    TimePoint not_after = TimePoint::max();
    // Set by a later release when the key leaked: files signed at or after this time are refused.
    // TimePoint::min() withdraws the key outright.
    std::optional<TimePoint> revoked_from;
};

struct Envelope
{
    int format = current_format;
    std::string purpose;
    std::string key_id;
    std::vector<std::uint8_t> payload;
    Signature signature{};
};

// Reads an envelope. Checks the size, the JSON, the five fields and the base64url. Does not check the
// signature.
[[nodiscard]] Result<Envelope> parse(std::string_view text);

// The text of an envelope, as `parse` reads it.
[[nodiscard]] std::string write(const Envelope& file);

// The bytes a signature covers: "ICL-SIGNED-FILE-1" 0x00 purpose 0x00 payload. The name of the format and
// the purpose are inside, so a signature made for one kind of file never passes as another.
[[nodiscard]] std::vector<std::uint8_t> signed_message(std::string_view purpose, std::span<const std::uint8_t> payload);

// Ed25519 verification as RFC 8032 defines it. Monocypher does the arithmetic. It accepts a point that is
// not written in its one canonical form, which the RFC does not (section 5.1.3), so those encodings of the
// key and of the first half of the signature are refused here first. No honest signer produces one.
[[nodiscard]] bool verify_ed25519(const Signature& signature, const PublicKey& key,
                                  std::span<const std::uint8_t> message);

// Checks the envelope against the trusted keys: the format is known, the purpose is the expected one, a
// key with that id and purpose exists and is not withdrawn outright, and the signature verifies. Returns
// the key, which the caller then gives to `check_key_window` with the signing time it read from the
// payload.
[[nodiscard]] Result<const TrustedKey*> verify(const Envelope& file, std::string_view expected_purpose,
                                               std::span<const TrustedKey> keys);

// Whether `key` may have signed a file at `issued_at`.
[[nodiscard]] Result<void> check_key_window(const TrustedKey& key, TimePoint issued_at);

// The keys built into this program, without any test key: the public keys of ARINC Data Loader's licences
// and of its lists of withdrawn licences (trusted_keys.cpp). A file signed with another key, a test key
// included, is refused as made for a newer version. A build whose list is empty accepts no file at all, and
// its programs are the Community edition.
[[nodiscard]] std::vector<TrustedKey> product_trust_list();

// The keys compiled in (trusted_keys.cpp). Use `product_trust_list` instead; this is exposed for its test.
[[nodiscard]] std::span<const TrustedKey> builtin_keys();

// `keys` without the test keys.
[[nodiscard]] std::vector<TrustedKey> without_test_keys(std::span<const TrustedKey> keys);

// base64url without padding (RFC 4648 section 5). `decode` refuses padding, white space, other alphabets
// and spare bits that are not zero.
[[nodiscard]] std::string base64url_encode(std::span<const std::uint8_t> data);
[[nodiscard]] Result<std::vector<std::uint8_t>> base64url_decode(std::string_view text);

} // namespace adl::licence::signed_file
