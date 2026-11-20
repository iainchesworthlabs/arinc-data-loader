// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>
#include <utility>

namespace adl::licence::detail
{

namespace
{

constexpr unsigned char utf8_continuation_mask = 0xC0;
constexpr unsigned char utf8_continuation_tag = 0x80;

[[nodiscard]] bool is_continuation(unsigned char byte) noexcept
{
    return (byte & utf8_continuation_mask) == utf8_continuation_tag;
}

[[nodiscard]] bool is_digit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

// The length of the well-formed UTF-8 sequence that starts at `pos`, or 0. No overlong form, no
// surrogate, nothing above U+10FFFF (RFC 3629).
[[nodiscard]] std::size_t utf8_length(std::string_view text, std::size_t pos) noexcept
{
    const auto byte_at = [&](std::size_t offset) { return static_cast<unsigned char>(text[pos + offset]); };
    const std::size_t left = text.size() - pos;
    const unsigned char first = byte_at(0);
    if (first >= 0xC2 && first <= 0xDF)
    {
        return left >= 2 && is_continuation(byte_at(1)) ? 2 : 0;
    }
    if (first >= 0xE0 && first <= 0xEF)
    {
        if (left < 3 || !is_continuation(byte_at(1)) || !is_continuation(byte_at(2)))
        {
            return 0;
        }
        if ((first == 0xE0 && byte_at(1) < 0xA0) || (first == 0xED && byte_at(1) > 0x9F))
        {
            return 0;
        }
        return 3;
    }
    if (first >= 0xF0 && first <= 0xF4)
    {
        if (left < 4 || !is_continuation(byte_at(1)) || !is_continuation(byte_at(2)) || !is_continuation(byte_at(3)))
        {
            return 0;
        }
        if ((first == 0xF0 && byte_at(1) < 0x90) || (first == 0xF4 && byte_at(1) > 0x8F))
        {
            return 0;
        }
        return 4;
    }
    return 0;
}

void append_utf8(std::string& out, std::uint32_t code_point)
{
    if (code_point < 0x80)
    {
        out.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6U)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3FU)));
    }
    else if (code_point < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12U)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3FU)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18U)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3FU)));
    }
}

template <typename T>
[[nodiscard]] JsonValue make_value(T&& content)
{
    JsonValue value;
    value.data = std::forward<T>(content);
    return value;
}

[[nodiscard]] bool has_duplicate_key(const JsonObject& members)
{
    // The views are taken after the last member was added, so no string moves under them.
    std::vector<std::string_view> keys;
    keys.reserve(members.size());
    for (const JsonMember& member : members)
    {
        keys.emplace_back(member.key);
    }
    std::ranges::sort(keys);
    return std::ranges::adjacent_find(keys) != keys.end();
}

class Parser
{
public:
    explicit Parser(std::string_view text) noexcept : text_(text) {}

    [[nodiscard]] Result<JsonValue> document()
    {
        skip_space();
        auto value = parse_value(0);
        if (!value)
        {
            return value;
        }
        skip_space();
        if (pos_ != text_.size())
        {
            return fail();
        }
        return value;
    }

private:
    [[nodiscard]] static std::unexpected<Error> fail()
    {
        return std::unexpected(Error::malformed);
    }

    [[nodiscard]] bool at_end() const noexcept
    {
        return pos_ >= text_.size();
    }
    [[nodiscard]] char peek() const noexcept
    {
        return at_end() ? '\0' : text_[pos_];
    }

    [[nodiscard]] bool consume(char expected) noexcept
    {
        if (!at_end() && text_[pos_] == expected)
        {
            ++pos_;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool consume_word(std::string_view word) noexcept
    {
        if (text_.substr(pos_, word.size()) == word)
        {
            pos_ += word.size();
            return true;
        }
        return false;
    }

    void skip_space() noexcept
    {
        while (!at_end() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
        {
            ++pos_;
        }
    }

    [[nodiscard]] Result<JsonValue> parse_value(std::size_t depth)
    {
        if (at_end())
        {
            return fail();
        }
        const char c = peek();
        if (c == '{')
        {
            return parse_object(depth);
        }
        if (c == '[')
        {
            return parse_array(depth);
        }
        if (c == '"')
        {
            auto text = parse_string();
            if (!text)
            {
                return std::unexpected(text.error());
            }
            return make_value(std::move(*text));
        }
        if (c == '-' || is_digit(c))
        {
            return parse_number();
        }
        if (consume_word("true"))
        {
            return make_value(true);
        }
        if (consume_word("false"))
        {
            return make_value(false);
        }
        if (consume_word("null"))
        {
            return JsonValue{};
        }
        return fail();
    }

    [[nodiscard]] Result<JsonValue> parse_object(std::size_t depth)
    {
        if (depth + 1 > max_json_depth)
        {
            return fail();
        }
        ++pos_; // the '{'
        JsonObject members;
        skip_space();
        if (consume('}'))
        {
            return make_value(std::move(members));
        }
        for (;;)
        {
            skip_space();
            if (peek() != '"')
            {
                return fail();
            }
            auto key = parse_string();
            if (!key)
            {
                return std::unexpected(key.error());
            }
            skip_space();
            if (!consume(':'))
            {
                return fail();
            }
            skip_space();
            auto value = parse_value(depth + 1);
            if (!value)
            {
                return value;
            }
            members.push_back(JsonMember{std::move(*key), std::move(*value)});
            skip_space();
            if (consume(','))
            {
                continue;
            }
            if (consume('}'))
            {
                break;
            }
            return fail();
        }
        if (has_duplicate_key(members))
        {
            return fail();
        }
        return make_value(std::move(members));
    }

    [[nodiscard]] Result<JsonValue> parse_array(std::size_t depth)
    {
        if (depth + 1 > max_json_depth)
        {
            return fail();
        }
        ++pos_; // the '['
        JsonArray items;
        skip_space();
        if (consume(']'))
        {
            return make_value(std::move(items));
        }
        for (;;)
        {
            skip_space();
            auto value = parse_value(depth + 1);
            if (!value)
            {
                return value;
            }
            items.push_back(std::move(*value));
            skip_space();
            if (consume(','))
            {
                continue;
            }
            if (consume(']'))
            {
                break;
            }
            return fail();
        }
        return make_value(std::move(items));
    }

    [[nodiscard]] Result<JsonValue> parse_number()
    {
        const std::size_t start = pos_;
        static_cast<void>(consume('-'));
        if (consume('0'))
        {
            // A leading zero stands alone: "01" is not a JSON number.
        }
        else if (is_digit(peek()))
        {
            while (is_digit(peek()))
            {
                ++pos_;
            }
        }
        else
        {
            return fail();
        }
        bool whole = true;
        if (consume('.'))
        {
            whole = false;
            if (!is_digit(peek()))
            {
                return fail();
            }
            while (is_digit(peek()))
            {
                ++pos_;
            }
        }
        if (peek() == 'e' || peek() == 'E')
        {
            whole = false;
            ++pos_;
            if (peek() == '+' || peek() == '-')
            {
                ++pos_;
            }
            if (!is_digit(peek()))
            {
                return fail();
            }
            while (is_digit(peek()))
            {
                ++pos_;
            }
        }
        if (!whole)
        {
            return make_value(JsonOtherNumber{});
        }
        std::int64_t number = 0;
        const char* const first = text_.data() + start;
        const char* const last = text_.data() + pos_;
        const auto [end, error] = std::from_chars(first, last, number);
        if (error == std::errc::result_out_of_range)
        {
            return make_value(JsonOtherNumber{});
        }
        if (error != std::errc{} || end != last)
        {
            return fail();
        }
        return make_value(number);
    }

    [[nodiscard]] Result<std::uint32_t> parse_hex4()
    {
        if (text_.size() - pos_ < 4)
        {
            return fail();
        }
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = text_[pos_++];
            std::uint32_t digit = 0;
            if (c >= '0' && c <= '9')
            {
                digit = static_cast<std::uint32_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = static_cast<std::uint32_t>(c - 'a') + 10U;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = static_cast<std::uint32_t>(c - 'A') + 10U;
            }
            else
            {
                return fail();
            }
            value = (value << 4U) | digit;
        }
        return value;
    }

    // After "\u": one code point, with the second half of a surrogate pair when there is a first.
    [[nodiscard]] Result<std::uint32_t> parse_unicode_escape()
    {
        auto first = parse_hex4();
        if (!first)
        {
            return first;
        }
        if (*first >= 0xDC00 && *first <= 0xDFFF)
        {
            return fail(); // a low surrogate with no high one before it
        }
        if (*first < 0xD800 || *first > 0xDBFF)
        {
            return first;
        }
        if (!consume('\\') || !consume('u'))
        {
            return fail();
        }
        auto second = parse_hex4();
        if (!second || *second < 0xDC00 || *second > 0xDFFF)
        {
            return fail();
        }
        return 0x10000U + ((*first - 0xD800U) << 10U) + (*second - 0xDC00U);
    }

    [[nodiscard]] Result<std::string> parse_string()
    {
        ++pos_; // the opening quote
        std::string out;
        for (;;)
        {
            if (at_end())
            {
                return fail();
            }
            const auto byte = static_cast<unsigned char>(text_[pos_]);
            if (byte == '"')
            {
                ++pos_;
                return out;
            }
            if (byte < 0x20)
            {
                return fail();
            }
            if (byte == '\\')
            {
                ++pos_;
                if (at_end())
                {
                    return fail();
                }
                const char escape = text_[pos_++];
                switch (escape)
                {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escape);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                {
                    const auto code_point = parse_unicode_escape();
                    if (!code_point)
                    {
                        return std::unexpected(code_point.error());
                    }
                    append_utf8(out, *code_point);
                    break;
                }
                default:
                    return fail();
                }
                continue;
            }
            if (byte < 0x80)
            {
                out.push_back(static_cast<char>(byte));
                ++pos_;
                continue;
            }
            const std::size_t length = utf8_length(text_, pos_);
            if (length == 0)
            {
                return fail();
            }
            out.append(text_.substr(pos_, length));
            pos_ += length;
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace

bool JsonValue::is_null() const noexcept
{
    return std::holds_alternative<std::monostate>(data);
}

const std::string* JsonValue::as_string() const noexcept
{
    return std::get_if<std::string>(&data);
}

const std::int64_t* JsonValue::as_integer() const noexcept
{
    return std::get_if<std::int64_t>(&data);
}

const JsonArray* JsonValue::as_array() const noexcept
{
    return std::get_if<JsonArray>(&data);
}

const JsonObject* JsonValue::as_object() const noexcept
{
    return std::get_if<JsonObject>(&data);
}

Result<JsonValue> parse_strict(std::string_view text)
{
    return Parser(text).document();
}

const JsonValue* find(const JsonObject& object, std::string_view key) noexcept
{
    const auto it = std::ranges::find_if(object, [key](const JsonMember& member) { return member.key == key; });
    return it == object.end() ? nullptr : &it->value;
}

Result<std::string> required_string(const JsonObject& object, std::string_view key, std::size_t max_length)
{
    const JsonValue* value = find(object, key);
    const std::string* text = value == nullptr ? nullptr : value->as_string();
    if (text == nullptr || text->empty() || text->size() > max_length)
    {
        return std::unexpected(Error::malformed);
    }
    return *text;
}

Result<std::string> optional_string(const JsonObject& object, std::string_view key, std::size_t max_length)
{
    const JsonValue* value = find(object, key);
    if (value == nullptr || value->is_null())
    {
        return std::string();
    }
    return required_string(object, key, max_length);
}

Result<std::int64_t> required_integer(const JsonObject& object, std::string_view key, std::int64_t minimum,
                                      std::int64_t maximum)
{
    const JsonValue* value = find(object, key);
    const std::int64_t* number = value == nullptr ? nullptr : value->as_integer();
    if (number == nullptr || *number < minimum || *number > maximum)
    {
        return std::unexpected(Error::malformed);
    }
    return *number;
}

Result<std::vector<std::string>> string_list(const JsonObject& object, std::string_view key, std::size_t max_entries,
                                             std::size_t max_length)
{
    std::vector<std::string> list;
    const JsonValue* value = find(object, key);
    if (value == nullptr)
    {
        return list;
    }
    const JsonArray* items = value->as_array();
    if (items == nullptr || items->size() > max_entries)
    {
        return std::unexpected(Error::malformed);
    }
    for (const JsonValue& entry : *items)
    {
        const std::string* text = entry.as_string();
        if (text == nullptr || text->empty() || text->size() > max_length)
        {
            return std::unexpected(Error::malformed);
        }
        list.push_back(*text);
    }
    return list;
}

bool is_token(std::string_view text, std::size_t max_length, std::string_view extra) noexcept
{
    if (text.empty() || text.size() > max_length)
    {
        return false;
    }
    return std::ranges::all_of(
        text, [extra](char c)
        { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || extra.find(c) != std::string_view::npos; });
}

std::string quote(std::string_view text)
{
    constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (const char c : text)
    {
        const auto byte = static_cast<unsigned char>(c);
        switch (c)
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
                out.push_back(hex[static_cast<std::size_t>(byte >> 4U)]);
                out.push_back(hex[static_cast<std::size_t>(byte & 0x0FU)]);
            }
            else
            {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace adl::licence::detail
