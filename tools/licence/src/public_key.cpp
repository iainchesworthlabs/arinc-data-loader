// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "public_key.hpp"

#include "messages.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace adl::licence_owner
{

namespace sf = adl::licence::signed_file;

namespace
{

constexpr std::size_t max_key_id_length = 64;
constexpr std::string_view head = "{\"key_id\":\"";
constexpr std::string_view middle = "\",\"purpose\":\"";
constexpr std::string_view tail = "\",\"public_key\":\"";
constexpr std::string_view end = "\"}";

[[nodiscard]] std::unexpected<std::string> bad_form()
{
    return std::unexpected(std::string(message::trust_bad_form));
}

// The text up to the next occurrence of `marker`, which is then skipped. Nothing if there is none.
[[nodiscard]] bool take(std::string_view& text, std::string_view marker, std::string_view& before)
{
    const std::size_t at = text.find(marker);
    if (at == std::string_view::npos)
    {
        return false;
    }
    before = text.substr(0, at);
    text.remove_prefix(at + marker.size());
    return true;
}

} // namespace

bool is_key_id(std::string_view text) noexcept
{
    return !text.empty() && text.size() <= max_key_id_length &&
           std::ranges::all_of(text,
                               [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

std::string format_public_key(const sf::TrustedKey& key)
{
    std::string text(head);
    text += key.key_id;
    text += middle;
    text += key.purpose;
    text += tail;
    text += sf::base64url_encode(key.key);
    text += end;
    text.push_back('\n');
    return text;
}

std::expected<sf::TrustedKey, std::string> parse_public_key(std::string_view text)
{
    if (text.ends_with('\n'))
    {
        text.remove_suffix(1);
    }
    if (!text.starts_with(head))
    {
        return bad_form();
    }
    text.remove_prefix(head.size());

    std::string_view id;
    std::string_view purpose;
    std::string_view key_text;
    if (!take(text, middle, id) || !take(text, tail, purpose) || !take(text, end, key_text) || !text.empty())
    {
        return bad_form();
    }
    if (!is_key_id(id) || (purpose != "licence" && purpose != "revocations"))
    {
        return bad_form();
    }
    const auto bytes = sf::base64url_decode(key_text);
    if (!bytes || bytes->size() != sf::PublicKey{}.size())
    {
        return bad_form();
    }

    sf::TrustedKey key;
    key.key_id = std::string(id);
    key.purpose = std::string(purpose);
    std::ranges::copy(*bytes, key.key.begin());
    return key;
}

} // namespace adl::licence_owner
