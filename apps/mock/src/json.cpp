// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/cli/json.hpp"

#include <cstddef>

namespace adl::mock::cli
{

namespace
{

constexpr std::string_view replacement = "\xEF\xBF\xBD"; // U+FFFD, as UTF-8

[[nodiscard]] bool is_continuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

// How many bytes at the start of `text` are one well-formed UTF-8 sequence (Unicode's Table 3-7: no
// overlong forms, no surrogates, nothing above U+10FFFF), or 0 if they are not.
[[nodiscard]] std::size_t well_formed_length(std::string_view text) noexcept
{
    const auto at = [&text](std::size_t index) { return static_cast<unsigned char>(text[index]); };
    const unsigned char lead = at(0);
    if (lead < 0x80U)
    {
        return 1;
    }
    std::size_t length = 0;
    unsigned char second_low = 0x80U;
    unsigned char second_high = 0xBFU;
    if (lead >= 0xC2U && lead <= 0xDFU)
    {
        length = 2;
    }
    else if (lead >= 0xE0U && lead <= 0xEFU)
    {
        length = 3;
        if (lead == 0xE0U)
        {
            second_low = 0xA0U; // not overlong
        }
        else if (lead == 0xEDU)
        {
            second_high = 0x9FU; // not a surrogate
        }
    }
    else if (lead >= 0xF0U && lead <= 0xF4U)
    {
        length = 4;
        if (lead == 0xF0U)
        {
            second_low = 0x90U; // not overlong
        }
        else if (lead == 0xF4U)
        {
            second_high = 0x8FU; // not above U+10FFFF
        }
    }
    else
    {
        return 0;
    }
    if (text.size() < length || at(1) < second_low || at(1) > second_high)
    {
        return 0;
    }
    for (std::size_t i = 2; i < length; ++i)
    {
        if (!is_continuation(at(i)))
        {
            return 0;
        }
    }
    return length;
}

} // namespace

std::string json_string(std::string_view text)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    while (!text.empty())
    {
        const auto byte = static_cast<unsigned char>(text.front());
        if (byte >= 0x80U)
        {
            const std::size_t length = well_formed_length(text);
            if (length == 0)
            {
                out += replacement;
                text.remove_prefix(1);
            }
            else
            {
                out.append(text.substr(0, length));
                text.remove_prefix(length);
            }
            continue;
        }
        text.remove_prefix(1);
        switch (byte)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (byte < 0x20U)
            {
                out += "\\u00";
                out.push_back(hex[byte >> 4U]);
                out.push_back(hex[byte & 0x0FU]);
            }
            else
            {
                out.push_back(static_cast<char>(byte));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

std::string seconds_text(std::chrono::milliseconds value)
{
    const std::int64_t total = value.count();
    const std::uint64_t magnitude =
        total < 0 ? 0 - static_cast<std::uint64_t>(total) : static_cast<std::uint64_t>(total);
    std::string out = total < 0 ? "-" : "";
    out += std::to_string(magnitude / 1000);
    out.push_back('.');
    const std::uint64_t milliseconds = magnitude % 1000;
    out += std::to_string(milliseconds / 100);
    out += std::to_string(milliseconds / 10 % 10);
    out += std::to_string(milliseconds % 10);
    return out;
}

void JsonObject::key(std::string_view name)
{
    members_ += members_.empty() ? "" : ",";
    members_ += json_string(name);
    members_.push_back(':');
}

JsonObject& JsonObject::string(std::string_view name, std::string_view value)
{
    key(name);
    members_ += json_string(value);
    return *this;
}

JsonObject& JsonObject::number(std::string_view name, std::int64_t value)
{
    key(name);
    members_ += std::to_string(value);
    return *this;
}

JsonObject& JsonObject::boolean(std::string_view name, bool value)
{
    key(name);
    members_ += value ? "true" : "false";
    return *this;
}

JsonObject& JsonObject::null(std::string_view name)
{
    key(name);
    members_ += "null";
    return *this;
}

JsonObject& JsonObject::seconds(std::string_view name, std::chrono::milliseconds value)
{
    key(name);
    members_ += seconds_text(value);
    return *this;
}

JsonObject& JsonObject::strings(std::string_view name, std::span<const std::string> values)
{
    key(name);
    members_.push_back('[');
    bool first = true;
    for (const std::string& value : values)
    {
        members_ += first ? "" : ",";
        members_ += json_string(value);
        first = false;
    }
    members_.push_back(']');
    return *this;
}

JsonObject& JsonObject::numbers(std::string_view name, std::span<const std::int64_t> values)
{
    key(name);
    members_.push_back('[');
    bool first = true;
    for (const std::int64_t value : values)
    {
        members_ += first ? "" : ",";
        members_ += std::to_string(value);
        first = false;
    }
    members_.push_back(']');
    return *this;
}

std::string JsonObject::text() const
{
    return "{" + members_ + "}";
}

} // namespace adl::mock::cli
