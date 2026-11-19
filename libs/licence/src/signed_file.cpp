// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence/signed_file.hpp"

#include "json.hpp"

#include <monocypher-ed25519.h>
#include <monocypher.h>

#include <algorithm>
#include <string>
#include <utility>

namespace adl::licence::signed_file
{

namespace
{

constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// The name of the format, so that the signature of a file of this format is not the signature of
// anything else. The number is the version of the format, as `format` in the envelope is.
constexpr std::string_view message_tag = "ICL-SIGNED-FILE-1";

constexpr std::size_t max_purpose_length = 32;
constexpr std::size_t max_key_id_length = 64;
// base64url of `max_file_size` bytes is longer than any file that passes the size check, so these only
// bound the two fields against each other.
constexpr std::size_t max_payload_text = max_file_size;
constexpr std::size_t signature_text_length = 86;
constexpr std::size_t envelope_fields = 5;
constexpr std::int64_t max_format = 1'000;

int sextet(char c) noexcept
{
    const auto at = alphabet.find(c);
    return at == std::string_view::npos ? -1 : static_cast<int>(at);
}

// Whether 32 bytes are the canonical encoding of a curve point's coordinates (RFC 8032, section 5.1.3): y
// below the field prime 2^255 - 19, and no sign bit on a point whose x is zero. Only y = 1 and y = -1
// have x = 0. Whether the point is on the curve is Monocypher's check. This looks at bytes only and does
// no arithmetic.
bool canonical_encoding(std::span<const std::uint8_t, 32> point) noexcept
{
    const std::uint8_t last = point.last<1>()[0];
    const std::uint8_t top = last & 0x7fU;
    const bool sign = (last & 0x80U) != 0;
    const auto middle = point.subspan(1, 30);
    const bool middle_ones = std::ranges::all_of(middle, [](std::uint8_t byte) { return byte == 0xffU; });
    const bool middle_zero = std::ranges::all_of(middle, [](std::uint8_t byte) { return byte == 0U; });

    // The prime is ed ff .. ff 7f, least significant byte first.
    if (top == 0x7fU && middle_ones && point[0] >= 0xedU)
    {
        return false;
    }
    const bool y_is_one = top == 0U && middle_zero && point[0] == 1U;
    const bool y_is_minus_one = top == 0x7fU && middle_ones && point[0] == 0xecU;
    return !(sign && (y_is_one || y_is_minus_one));
}

} // namespace

bool verify_ed25519(const Signature& signature, const PublicKey& key, std::span<const std::uint8_t> message)
{
    if (!canonical_encoding(std::span<const std::uint8_t, 32>(key)) ||
        !canonical_encoding(std::span(signature).first<32>()))
    {
        return false;
    }
    return crypto_ed25519_check(signature.data(), key.data(), message.data(), message.size()) == 0;
}

std::string base64url_encode(std::span<const std::uint8_t> data)
{
    std::string text;
    text.reserve((data.size() * 4 + 2) / 3);
    std::uint32_t bits = 0;
    unsigned count = 0;
    for (const std::uint8_t byte : data)
    {
        bits = (bits << 8U) | byte;
        count += 8;
        while (count >= 6)
        {
            count -= 6;
            text.push_back(alphabet[(bits >> count) & 0x3fU]);
        }
    }
    if (count > 0)
    {
        text.push_back(alphabet[(bits << (6U - count)) & 0x3fU]);
    }
    return text;
}

Result<std::vector<std::uint8_t>> base64url_decode(std::string_view text)
{
    // A length of 4n+1 cannot come from whole bytes.
    if (text.size() % 4 == 1)
    {
        return std::unexpected(Error::malformed);
    }
    std::vector<std::uint8_t> data;
    data.reserve(text.size() * 3 / 4);
    std::uint32_t bits = 0;
    unsigned count = 0;
    for (const char c : text)
    {
        const int value = sextet(c);
        if (value < 0)
        {
            // The text may be a private key, so what was decoded is wiped before the vector frees it.
            crypto_wipe(data.data(), data.size());
            return std::unexpected(Error::malformed);
        }
        bits = ((bits << 6U) | static_cast<std::uint32_t>(value)) & 0xffffU;
        count += 6;
        if (count >= 8)
        {
            count -= 8;
            data.push_back(static_cast<std::uint8_t>((bits >> count) & 0xffU));
        }
    }
    // The bits left over must be zero, so that each value has one spelling.
    if ((bits & ((1U << count) - 1U)) != 0)
    {
        crypto_wipe(data.data(), data.size());
        return std::unexpected(Error::malformed);
    }
    return data;
}

Result<Envelope> parse(std::string_view text)
{
    if (text.size() > max_file_size)
    {
        return std::unexpected(Error::too_large);
    }
    const auto document = detail::parse_strict(text);
    const detail::JsonObject* object = document ? document->as_object() : nullptr;
    if (object == nullptr)
    {
        return std::unexpected(Error::malformed);
    }

    // The format comes first: a later format may have other fields.
    const auto format = detail::required_integer(*object, "format", 1, max_format);
    if (!format)
    {
        return std::unexpected(Error::malformed);
    }
    if (*format != current_format)
    {
        return std::unexpected(Error::newer_version);
    }
    if (object->size() != envelope_fields)
    {
        return std::unexpected(Error::malformed);
    }

    const auto purpose = detail::required_string(*object, "purpose", max_purpose_length);
    const auto key_id = detail::required_string(*object, "key_id", max_key_id_length);
    const auto payload_text = detail::required_string(*object, "payload", max_payload_text);
    const auto signature_text = detail::required_string(*object, "signature", signature_text_length);
    if (!purpose || !key_id || !payload_text || !signature_text ||
        !detail::is_token(*purpose, max_purpose_length, "-") || !detail::is_token(*key_id, max_key_id_length, "-"))
    {
        return std::unexpected(Error::malformed);
    }

    auto payload = base64url_decode(*payload_text);
    const auto signature = base64url_decode(*signature_text);
    if (!payload || !signature || signature->size() != Signature{}.size())
    {
        return std::unexpected(Error::malformed);
    }

    Envelope file;
    file.format = static_cast<int>(*format);
    file.purpose = *purpose;
    file.key_id = *key_id;
    file.payload = std::move(*payload);
    std::ranges::copy(*signature, file.signature.begin());
    return file;
}

std::string write(const Envelope& file)
{
    std::string text = "{\"format\":";
    text += std::to_string(file.format);
    text += ",\"purpose\":" + detail::quote(file.purpose);
    text += ",\"key_id\":" + detail::quote(file.key_id);
    text += ",\"payload\":" + detail::quote(base64url_encode(file.payload));
    text += ",\"signature\":" + detail::quote(base64url_encode(file.signature));
    text += "}\n";
    return text;
}

std::vector<std::uint8_t> signed_message(std::string_view purpose, std::span<const std::uint8_t> payload)
{
    std::vector<std::uint8_t> message;
    message.reserve(message_tag.size() + purpose.size() + payload.size() + 2);
    message.insert(message.end(), message_tag.begin(), message_tag.end());
    message.push_back(0);
    message.insert(message.end(), purpose.begin(), purpose.end());
    message.push_back(0);
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

Result<const TrustedKey*> verify(const Envelope& file, std::string_view expected_purpose,
                                 std::span<const TrustedKey> keys)
{
    if (file.format != current_format)
    {
        return std::unexpected(Error::newer_version);
    }
    if (file.purpose != expected_purpose)
    {
        return std::unexpected(Error::wrong_purpose);
    }
    const auto key = std::ranges::find_if(keys, [&](const TrustedKey& k)
                                          { return k.key_id == file.key_id && k.purpose == file.purpose; });
    if (key == keys.end())
    {
        return std::unexpected(Error::unknown_key);
    }
    // An optional equals a value only when it holds one.
    if (key->revoked_from == TimePoint::min())
    {
        return std::unexpected(Error::key_not_valid);
    }
    const auto message = signed_message(file.purpose, file.payload);
    if (!verify_ed25519(file.signature, key->key, message))
    {
        return std::unexpected(Error::bad_signature);
    }
    return &*key;
}

Result<void> check_key_window(const TrustedKey& key, TimePoint issued_at)
{
    if (issued_at < key.not_before || issued_at > key.not_after)
    {
        return std::unexpected(Error::key_not_valid);
    }
    if (key.revoked_from && issued_at >= *key.revoked_from)
    {
        return std::unexpected(Error::key_not_valid);
    }
    return {};
}

std::vector<TrustedKey> without_test_keys(std::span<const TrustedKey> keys)
{
    std::vector<TrustedKey> kept;
    for (const TrustedKey& key : keys)
    {
        if (!key.key_id.starts_with(test_key_prefix))
        {
            kept.push_back(key);
        }
    }
    return kept;
}

std::vector<TrustedKey> product_trust_list()
{
    return without_test_keys(builtin_keys());
}

} // namespace adl::licence::signed_file
