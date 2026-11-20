// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/make_set/json.hpp"

#include <cstddef>

namespace adl::make_set
{

namespace
{

constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_tag = 0x80;

bool is_continuation(unsigned char byte) noexcept
{
    return (byte & continuation_mask) == continuation_tag;
}

// The length of the well-formed UTF-8 sequence that starts at `pos`, or 0 (RFC 3629: no overlong form, no
// surrogate, nothing above U+10FFFF).
std::size_t sequence_length(std::string_view text, std::size_t pos) noexcept
{
    const auto at = [&](std::size_t offset) { return static_cast<unsigned char>(text[pos + offset]); };
    const std::size_t left = text.size() - pos;
    const unsigned char first = at(0);
    if (first >= 0xC2 && first <= 0xDF)
    {
        return left >= 2 && is_continuation(at(1)) ? 2 : 0;
    }
    if (first >= 0xE0 && first <= 0xEF)
    {
        if (left < 3 || !is_continuation(at(1)) || !is_continuation(at(2)))
        {
            return 0;
        }
        if ((first == 0xE0 && at(1) < 0xA0) || (first == 0xED && at(1) > 0x9F))
        {
            return 0;
        }
        return 3;
    }
    if (first >= 0xF0 && first <= 0xF4)
    {
        if (left < 4 || !is_continuation(at(1)) || !is_continuation(at(2)) || !is_continuation(at(3)))
        {
            return 0;
        }
        if ((first == 0xF0 && at(1) < 0x90) || (first == 0xF4 && at(1) > 0x8F))
        {
            return 0;
        }
        return 4;
    }
    return 0;
}

} // namespace

std::string json_string(std::string_view text)
{
    constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (std::size_t i = 0; i < text.size();)
    {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte >= 0x80)
        {
            const std::size_t length = sequence_length(text, i);
            if (length == 0)
            {
                out += "\xEF\xBF\xBD"; // U+FFFD
                ++i;
            }
            else
            {
                out.append(text.substr(i, length));
                i += length;
            }
            continue;
        }
        switch (text[i])
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
            if (byte < 0x20)
            {
                out += "\\u00";
                out.push_back(hex[byte >> 4U]);
                out.push_back(hex[byte & 0x0FU]);
            }
            else
            {
                out.push_back(text[i]);
            }
        }
        ++i;
    }
    out.push_back('"');
    return out;
}

std::string seconds_text(std::chrono::milliseconds value)
{
    const std::int64_t total = value.count();
    const std::int64_t magnitude = total < 0 ? -total : total;
    std::string fraction = std::to_string(magnitude % 1000);
    fraction.insert(0, 3 - fraction.size(), '0');
    return (total < 0 ? "-" : "") + std::to_string(magnitude / 1000) + "." + fraction;
}

void JsonWriter::before_value()
{
    if (after_key_)
    {
        after_key_ = false;
        return;
    }
    if (!has_member_.empty())
    {
        if (has_member_.back())
        {
            text_ += ',';
        }
        has_member_.back() = true;
    }
}

JsonWriter& JsonWriter::begin_object()
{
    before_value();
    text_ += '{';
    has_member_.push_back(false);
    return *this;
}

JsonWriter& JsonWriter::end_object()
{
    text_ += '}';
    has_member_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::begin_array()
{
    before_value();
    text_ += '[';
    has_member_.push_back(false);
    return *this;
}

JsonWriter& JsonWriter::end_array()
{
    text_ += ']';
    has_member_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::key(std::string_view name)
{
    if (!has_member_.empty())
    {
        if (has_member_.back())
        {
            text_ += ',';
        }
        has_member_.back() = true;
    }
    text_ += json_string(name);
    text_ += ':';
    after_key_ = true;
    return *this;
}

JsonWriter& JsonWriter::string(std::string_view value)
{
    before_value();
    text_ += json_string(value);
    return *this;
}

JsonWriter& JsonWriter::integer(std::int64_t value)
{
    before_value();
    text_ += std::to_string(value);
    return *this;
}

JsonWriter& JsonWriter::boolean(bool value)
{
    before_value();
    text_ += value ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::null()
{
    before_value();
    text_ += "null";
    return *this;
}

JsonWriter& JsonWriter::seconds(std::chrono::milliseconds value)
{
    before_value();
    text_ += seconds_text(value);
    return *this;
}

JsonWriter& JsonWriter::member(std::string_view name, std::string_view value)
{
    return key(name).string(value);
}

JsonWriter& JsonWriter::member(std::string_view name, const char* value)
{
    return key(name).string(value);
}

JsonWriter& JsonWriter::member_int(std::string_view name, std::int64_t value)
{
    return key(name).integer(value);
}

JsonWriter& JsonWriter::member_bool(std::string_view name, bool value)
{
    return key(name).boolean(value);
}

JsonWriter& JsonWriter::member_null(std::string_view name)
{
    return key(name).null();
}

JsonWriter& JsonWriter::member(std::string_view name, const std::vector<std::string>& values)
{
    key(name).begin_array();
    for (const std::string& value : values)
    {
        string(value);
    }
    return end_array();
}

} // namespace adl::make_set
