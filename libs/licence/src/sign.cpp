// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence/sign.hpp"

#include <monocypher-ed25519.h>
#include <monocypher.h>

#include <algorithm>

namespace adl::licence::signing
{

namespace
{

// The 64 byte key Monocypher signs with, wiped when it goes out of scope.
class ExpandedKey
{
public:
    explicit ExpandedKey(const Seed& private_key)
    {
        // crypto_ed25519_key_pair wipes the seed it is given, so it gets a copy.
        Seed copy = private_key;
        crypto_ed25519_key_pair(secret_.data(), public_half_.data(), copy.data());
    }
    ExpandedKey(const ExpandedKey&) = delete;
    ExpandedKey& operator=(const ExpandedKey&) = delete;
    ~ExpandedKey()
    {
        crypto_wipe(secret_.data(), secret_.size());
    }

    [[nodiscard]] const std::array<std::uint8_t, 64>& secret() const noexcept
    {
        return secret_;
    }
    [[nodiscard]] const signed_file::PublicKey& public_half() const noexcept
    {
        return public_half_;
    }

private:
    std::array<std::uint8_t, 64> secret_{};
    signed_file::PublicKey public_half_{};
};

} // namespace

signed_file::PublicKey public_key_of(const Seed& private_key)
{
    const ExpandedKey key(private_key);
    return key.public_half();
}

signed_file::Envelope sign(std::string_view purpose, std::string_view key_id, std::span<const std::uint8_t> payload,
                           const Seed& private_key)
{
    signed_file::Envelope file;
    file.purpose = purpose;
    file.key_id = key_id;
    file.payload.assign(payload.begin(), payload.end());

    const auto message = signed_file::signed_message(purpose, payload);
    const ExpandedKey key(private_key);
    crypto_ed25519_sign(file.signature.data(), key.secret().data(), message.data(), message.size());
    return file;
}

Result<Seed> parse_seed(std::string_view text)
{
    constexpr std::string_view space = " \t\r\n";
    const auto first = text.find_first_not_of(space);
    if (first == std::string_view::npos)
    {
        return std::unexpected(Error::malformed);
    }
    const auto last = text.find_last_not_of(space);
    auto bytes = signed_file::base64url_decode(text.substr(first, last - first + 1));
    if (!bytes)
    {
        return std::unexpected(Error::malformed);
    }
    Seed value{};
    const bool right_size = bytes->size() == value.size();
    if (right_size)
    {
        std::ranges::copy(*bytes, value.begin());
    }
    crypto_wipe(bytes->data(), bytes->size());
    if (!right_size)
    {
        return std::unexpected(Error::malformed);
    }
    // The result is a copy of `value`, so the local is wiped before it goes.
    Result<Seed> parsed = value;
    wipe(value);
    return parsed;
}

std::string format_seed(const Seed& private_key)
{
    return signed_file::base64url_encode(private_key);
}

void wipe(Seed& private_key) noexcept
{
    crypto_wipe(private_key.data(), private_key.size());
}

void wipe(std::string& text) noexcept
{
    crypto_wipe(text.data(), text.size());
    text.clear();
}

} // namespace adl::licence::signing
