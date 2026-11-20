// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A strict JSON reader for the tests of adl_mock's output (RFC 8259: no comments, no trailing commas,
// no raw control characters in a string, only well-formed UTF-8). It is here, apart from the program,
// so that what the program writes is read by something that did not write it.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace adl::test
{

struct Json;
using JsonMembers = std::vector<std::pair<std::string, Json>>;
using JsonItems = std::vector<Json>;

struct Json
{
    std::variant<std::nullptr_t, bool, double, std::string, JsonItems, JsonMembers> value;
    std::string number_text; // the text of a number, as written

    [[nodiscard]] bool is_null() const noexcept
    {
        return std::holds_alternative<std::nullptr_t>(value);
    }
    [[nodiscard]] bool is_object() const noexcept
    {
        return std::holds_alternative<JsonMembers>(value);
    }
    [[nodiscard]] const std::string& string() const
    {
        return std::get<std::string>(value);
    }
    [[nodiscard]] double number() const
    {
        return std::get<double>(value);
    }
    [[nodiscard]] bool boolean() const
    {
        return std::get<bool>(value);
    }
    [[nodiscard]] const JsonItems& items() const
    {
        return std::get<JsonItems>(value);
    }
    [[nodiscard]] const JsonMembers& members() const
    {
        return std::get<JsonMembers>(value);
    }
    // The value of `key` in an object, or null when there is no such key.
    [[nodiscard]] const Json* find(std::string_view key) const
    {
        if (!is_object())
        {
            return nullptr;
        }
        for (const auto& [name, member] : members())
        {
            if (name == key)
            {
                return &member;
            }
        }
        return nullptr;
    }
    // The value of `key` in an object. A test that asks for a key that is not there fails (Catch2 reports the
    // exception), where a dereference of the null `find` gives would be undefined, and GCC says so at -O2.
    [[nodiscard]] const Json& at(std::string_view key) const
    {
        const Json* const found = find(key);
        if (found == nullptr)
        {
            throw std::out_of_range("the JSON value has no member '" + std::string(key) + "'");
        }
        return *found;
    }
};

class JsonReader
{
public:
    explicit JsonReader(std::string_view text) : text_(text) {}

    // The one value `text` holds, or nothing when it is not exactly one valid JSON value.
    [[nodiscard]] static std::optional<Json> read(std::string_view text)
    {
        JsonReader reader(text);
        auto value = reader.value(0);
        reader.skip_space();
        if (!value || reader.position_ != text.size())
        {
            return std::nullopt;
        }
        return value;
    }

private:
    [[nodiscard]] bool at_end() const noexcept
    {
        return position_ >= text_.size();
    }
    [[nodiscard]] char peek() const noexcept
    {
        return text_[position_];
    }
    void skip_space() noexcept
    {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
        {
            ++position_;
        }
    }
    [[nodiscard]] bool take(std::string_view word) noexcept
    {
        if (text_.substr(position_, word.size()) != word)
        {
            return false;
        }
        position_ += word.size();
        return true;
    }

    [[nodiscard]] std::optional<Json> value(int depth)
    {
        if (depth > 32)
        {
            return std::nullopt;
        }
        skip_space();
        if (at_end())
        {
            return std::nullopt;
        }
        if (peek() == '{')
        {
            return object(depth);
        }
        if (peek() == '[')
        {
            return array(depth);
        }
        if (peek() == '"')
        {
            auto text = string();
            return text ? std::optional<Json>{Json{std::move(*text), {}}} : std::nullopt;
        }
        if (take("true"))
        {
            return Json{true, {}};
        }
        if (take("false"))
        {
            return Json{false, {}};
        }
        if (take("null"))
        {
            return Json{nullptr, {}};
        }
        return number();
    }

    [[nodiscard]] std::optional<Json> object(int depth)
    {
        ++position_;
        JsonMembers members;
        skip_space();
        if (!at_end() && peek() == '}')
        {
            ++position_;
            return Json{std::move(members), {}};
        }
        while (true)
        {
            skip_space();
            if (at_end() || peek() != '"')
            {
                return std::nullopt;
            }
            auto key = string();
            skip_space();
            if (!key || at_end() || peek() != ':')
            {
                return std::nullopt;
            }
            ++position_;
            auto member = value(depth + 1);
            if (!member)
            {
                return std::nullopt;
            }
            members.emplace_back(std::move(*key), std::move(*member));
            skip_space();
            if (at_end())
            {
                return std::nullopt;
            }
            if (peek() == ',')
            {
                ++position_;
                continue;
            }
            if (peek() == '}')
            {
                ++position_;
                return Json{std::move(members), {}};
            }
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<Json> array(int depth)
    {
        ++position_;
        JsonItems items;
        skip_space();
        if (!at_end() && peek() == ']')
        {
            ++position_;
            return Json{std::move(items), {}};
        }
        while (true)
        {
            auto item = value(depth + 1);
            if (!item)
            {
                return std::nullopt;
            }
            items.push_back(std::move(*item));
            skip_space();
            if (at_end())
            {
                return std::nullopt;
            }
            if (peek() == ',')
            {
                ++position_;
                continue;
            }
            if (peek() == ']')
            {
                ++position_;
                return Json{std::move(items), {}};
            }
            return std::nullopt;
        }
    }

    [[nodiscard]] static int hex(char c) noexcept
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    }

    // How long the well-formed UTF-8 sequence at the front of `rest` is, or 0.
    [[nodiscard]] static std::size_t sequence_length(std::string_view rest) noexcept
    {
        const auto at = [&rest](std::size_t i) { return static_cast<unsigned char>(rest[i]); };
        const unsigned char lead = at(0);
        std::size_t length = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF)
        {
            length = 2;
        }
        else if (lead >= 0xE0 && lead <= 0xEF)
        {
            length = 3;
            low = lead == 0xE0 ? 0xA0 : 0x80;
            high = lead == 0xED ? 0x9F : 0xBF;
        }
        else if (lead >= 0xF0 && lead <= 0xF4)
        {
            length = 4;
            low = lead == 0xF0 ? 0x90 : 0x80;
            high = lead == 0xF4 ? 0x8F : 0xBF;
        }
        else
        {
            return 0;
        }
        if (rest.size() < length || at(1) < low || at(1) > high)
        {
            return 0;
        }
        for (std::size_t i = 2; i < length; ++i)
        {
            if ((at(i) & 0xC0U) != 0x80U)
            {
                return 0;
            }
        }
        return length;
    }

    static void append_code_point(std::string& out, unsigned code)
    {
        if (code < 0x80)
        {
            out.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    [[nodiscard]] std::optional<std::string> string()
    {
        ++position_;
        std::string out;
        while (!at_end())
        {
            const auto byte = static_cast<unsigned char>(peek());
            if (byte == '"')
            {
                ++position_;
                return out;
            }
            if (byte < 0x20)
            {
                return std::nullopt; // a raw control character
            }
            if (byte >= 0x80)
            {
                const std::size_t length = sequence_length(text_.substr(position_));
                if (length == 0)
                {
                    return std::nullopt;
                }
                out.append(text_.substr(position_, length));
                position_ += length;
                continue;
            }
            ++position_;
            if (byte != '\\')
            {
                out.push_back(static_cast<char>(byte));
                continue;
            }
            if (at_end())
            {
                return std::nullopt;
            }
            const char escape = peek();
            ++position_;
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
                unsigned code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    if (at_end() || hex(peek()) < 0)
                    {
                        return std::nullopt;
                    }
                    code = code * 16 + static_cast<unsigned>(hex(peek()));
                    ++position_;
                }
                if (code >= 0xD800 && code <= 0xDFFF)
                {
                    return std::nullopt; // the tests never write a surrogate pair
                }
                append_code_point(out, code);
                break;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt; // no closing quote
    }

    [[nodiscard]] std::optional<Json> number()
    {
        const std::size_t start = position_;
        if (!at_end() && peek() == '-')
        {
            ++position_;
        }
        const auto digits = [this]
        {
            const std::size_t from = position_;
            while (!at_end() && peek() >= '0' && peek() <= '9')
            {
                ++position_;
            }
            return position_ - from;
        };
        if (at_end())
        {
            return std::nullopt;
        }
        if (peek() == '0')
        {
            ++position_;
        }
        else if (digits() == 0)
        {
            return std::nullopt;
        }
        if (!at_end() && peek() == '.')
        {
            ++position_;
            if (digits() == 0)
            {
                return std::nullopt;
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E'))
        {
            ++position_;
            if (!at_end() && (peek() == '+' || peek() == '-'))
            {
                ++position_;
            }
            if (digits() == 0)
            {
                return std::nullopt;
            }
        }
        const std::string written{text_.substr(start, position_ - start)};
        return Json{std::strtod(written.c_str(), nullptr), written};
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

} // namespace adl::test
