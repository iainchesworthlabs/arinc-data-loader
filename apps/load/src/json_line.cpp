// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/json_line.hpp"

#include <utility>

namespace adl::load
{

namespace
{

constexpr std::string_view replacement_character = "\xEF\xBF\xBD"; // U+FFFD

// How many bytes the UTF-8 sequence at the start of `text` takes, or 0 when what is there is not one (a lead
// byte that starts nothing, a sequence that ends early, an overlong form, a surrogate, a number above
// U+10FFFF). `text` is not empty.
[[nodiscard]] std::size_t utf8_sequence_length(std::string_view text) noexcept
{
    const auto at = [&text](std::size_t index) { return static_cast<unsigned char>(text[index]); };
    const auto continues = [&](std::size_t index, unsigned char low = 0x80, unsigned char high = 0xBF)
    { return index < text.size() && at(index) >= low && at(index) <= high; };

    const unsigned char lead = at(0);
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        return continues(1) ? 2 : 0;
    }
    if (lead >= 0xE0 && lead <= 0xEF)
    {
        // E0 and ED have a narrower second byte: no overlong form, no surrogate.
        const unsigned char low = lead == 0xE0 ? 0xA0 : 0x80;
        const unsigned char high = lead == 0xED ? 0x9F : 0xBF;
        return continues(1, low, high) && continues(2) ? 3 : 0;
    }
    if (lead >= 0xF0 && lead <= 0xF4)
    {
        const unsigned char low = lead == 0xF0 ? 0x90 : 0x80;
        const unsigned char high = lead == 0xF4 ? 0x8F : 0xBF;
        return continues(1, low, high) && continues(2) && continues(3) ? 4 : 0;
    }
    return 0;
}

} // namespace

void append_json_escaped(std::string& out, std::string_view text)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    for (std::size_t i = 0; i < text.size();)
    {
        const char c = text[i];
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
            ++i;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            switch (c)
            {
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
                out += "\\u00";
                out += hex[static_cast<unsigned char>(c) >> 4];
                out += hex[static_cast<unsigned char>(c) & 0x0F];
                break;
            }
            ++i;
        }
        else
        {
            const std::size_t length = utf8_sequence_length(text.substr(i));
            if (length == 0)
            {
                out += replacement_character;
                ++i;
            }
            else
            {
                out.append(text.substr(i, length));
                i += length;
            }
        }
    }
}

JsonLine::JsonLine()
{
    out_ += '{';
    first_.push_back(true);
}

void JsonLine::member(std::string_view key)
{
    if (!first_.back())
    {
        out_ += ',';
    }
    first_.back() = false;
    if (!key.empty())
    {
        out_ += '"';
        append_json_escaped(out_, key);
        out_ += "\":";
    }
}

void JsonLine::begin_object(std::string_view key)
{
    member(key);
    out_ += '{';
    first_.push_back(true);
}

void JsonLine::end_object()
{
    out_ += '}';
    first_.pop_back();
}

void JsonLine::begin_array(std::string_view key)
{
    member(key);
    out_ += '[';
    first_.push_back(true);
}

void JsonLine::end_array()
{
    out_ += ']';
    first_.pop_back();
}

void JsonLine::string(std::string_view key, std::string_view value)
{
    member(key);
    out_ += '"';
    append_json_escaped(out_, value);
    out_ += '"';
}

void JsonLine::string_element(std::string_view value)
{
    string({}, value);
}

void JsonLine::number(std::string_view key, std::uint64_t value)
{
    member(key);
    out_ += std::to_string(value);
}

void JsonLine::milliseconds_as_seconds(std::string_view key, std::uint64_t milliseconds)
{
    member(key);
    out_ += std::to_string(milliseconds / 1000);
    out_ += '.';
    const std::string fraction = std::to_string(milliseconds % 1000);
    out_.append(3 - fraction.size(), '0');
    out_ += fraction;
}

void JsonLine::boolean(std::string_view key, bool value)
{
    member(key);
    out_ += value ? "true" : "false";
}

void JsonLine::null(std::string_view key)
{
    member(key);
    out_ += "null";
}

std::string JsonLine::finish()
{
    out_ += '}';
    out_ += '\n';
    return std::move(out_);
}

} // namespace adl::load
