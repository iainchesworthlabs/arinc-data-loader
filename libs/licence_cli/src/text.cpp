// SPDX-License-Identifier: GPL-3.0-or-later
#include "text.hpp"

#include <cstddef>

namespace adl::licence_cli
{

namespace
{

// The length of the UTF-8 character that starts at `text[at]`, and its code point, or 0 for bytes that are not
// a character: a stray continuation byte, an overlong form, a surrogate, a value above U+10FFFF, or a character
// cut short.
[[nodiscard]] std::size_t character_at(std::string_view text, std::size_t at, char32_t& code) noexcept
{
    const auto lead = static_cast<unsigned char>(text[at]);
    std::size_t length = 0;
    if (lead < 0x80)
    {
        code = lead;
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        length = 2;
        code = lead & 0x1Fu;
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        length = 3;
        code = lead & 0x0Fu;
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        length = 4;
        code = lead & 0x07u;
    }
    else
    {
        return 0;
    }
    if (at + length > text.size())
    {
        return 0;
    }
    for (std::size_t i = 1; i < length; ++i)
    {
        const auto next = static_cast<unsigned char>(text[at + i]);
        if ((next & 0xC0u) != 0x80u)
        {
            return 0;
        }
        code = (code << 6) | (next & 0x3Fu);
    }
    const bool overlong = (length == 3 && code < 0x800) || (length == 4 && code < 0x10000);
    const bool surrogate = code >= 0xD800 && code <= 0xDFFF;
    if (overlong || surrogate || code > 0x10FFFF)
    {
        return 0;
    }
    return length;
}

[[nodiscard]] bool is_unsafe(char32_t code) noexcept
{
    return code < 0x20 || (code >= 0x7F && code <= 0x9F) || code == 0x061C || code == 0x200E || code == 0x200F ||
           code == 0x2028 || code == 0x2029 || (code >= 0x202A && code <= 0x202E) || (code >= 0x2066 && code <= 0x2069);
}

} // namespace

std::string plain(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t at = 0; at < text.size();)
    {
        char32_t code = 0;
        const std::size_t length = character_at(text, at, code);
        if (length == 0)
        {
            out.push_back('?');
            ++at;
            continue;
        }
        if (is_unsafe(code))
        {
            out.push_back('?');
        }
        else
        {
            out.append(text.substr(at, length));
        }
        at += length;
    }
    return out;
}

std::string date_of(licence::TimePoint value)
{
    return licence::format_time(value).substr(0, 10);
}

std::string joined(const std::vector<std::string>& items, std::string_view empty)
{
    if (items.empty())
    {
        return std::string(empty);
    }
    std::string out;
    for (const std::string& item : items)
    {
        if (!out.empty())
        {
            out += ", ";
        }
        out += plain(item);
    }
    return out;
}

} // namespace adl::licence_cli
