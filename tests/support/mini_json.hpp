// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A strict JSON reader for the tests of adl_load: what the program writes has to be JSON, with its keys in
// the order the contract gives, and a test has to be able to look at it. It keeps the order of an object's
// members, keeps a number as the text it was written as, and refuses everything JSON refuses (a trailing
// comma, a control character in a string, a lone surrogate, text after the value, invalid UTF-8). It is not a
// library for anything else.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::test
{

struct Json
{
    enum class Kind
    {
        null,
        boolean,
        number,
        string,
        array,
        object
    };

    Kind kind = Kind::null;
    bool boolean = false;
    std::string text; // a string's characters, or a number as written
    std::vector<Json> elements;
    std::vector<std::pair<std::string, Json>> members;

    [[nodiscard]] bool is(Kind wanted) const noexcept
    {
        return kind == wanted;
    }
    [[nodiscard]] const Json* find(std::string_view key) const noexcept
    {
        for (const auto& [name, value] : members)
        {
            if (name == key)
            {
                return &value;
            }
        }
        return nullptr;
    }
    // The member called `key`, which has to be there (an empty null value otherwise).
    [[nodiscard]] const Json& at(std::string_view key) const noexcept
    {
        static const Json missing;
        const Json* found = find(key);
        return found != nullptr ? *found : missing;
    }
    [[nodiscard]] std::vector<std::string> keys() const
    {
        std::vector<std::string> names;
        for (const auto& member : members)
        {
            names.push_back(member.first);
        }
        return names;
    }
    [[nodiscard]] std::uint64_t as_number() const
    {
        return std::stoull(text);
    }
};

namespace detail
{

class MiniJsonReader
{
public:
    explicit MiniJsonReader(std::string_view text) : text_(text) {}

    [[nodiscard]] std::optional<Json> read_document()
    {
        std::optional<Json> value = read_value(0);
        skip_space();
        if (!value || at_ != text_.size())
        {
            return std::nullopt;
        }
        return value;
    }

private:
    [[nodiscard]] bool done() const noexcept
    {
        return at_ >= text_.size();
    }
    [[nodiscard]] char peek() const noexcept
    {
        return done() ? '\0' : text_[at_];
    }
    void skip_space() noexcept
    {
        while (!done() && (peek() == ' ' || peek() == '\n' || peek() == '\r' || peek() == '\t'))
        {
            ++at_;
        }
    }
    [[nodiscard]] bool take(std::string_view word) noexcept
    {
        if (text_.substr(at_, word.size()) != word)
        {
            return false;
        }
        at_ += word.size();
        return true;
    }

    [[nodiscard]] std::optional<Json> read_value(int depth)
    {
        if (depth > 64)
        {
            return std::nullopt;
        }
        skip_space();
        Json value;
        if (take("null"))
        {
            return value;
        }
        if (take("true") || take("false"))
        {
            value.kind = Json::Kind::boolean;
            value.boolean = text_[at_ - 1] == 'e' && text_.substr(at_ - 4, 4) == "true";
            return value;
        }
        if (peek() == '"')
        {
            value.kind = Json::Kind::string;
            if (!read_string(value.text))
            {
                return std::nullopt;
            }
            return value;
        }
        if (peek() == '[')
        {
            return read_array(depth);
        }
        if (peek() == '{')
        {
            return read_object(depth);
        }
        return read_number();
    }

    [[nodiscard]] std::optional<Json> read_number()
    {
        const std::size_t begin = at_;
        if (peek() == '-')
        {
            ++at_;
        }
        if (peek() == '0')
        {
            ++at_;
        }
        else if (peek() >= '1' && peek() <= '9')
        {
            while (peek() >= '0' && peek() <= '9')
            {
                ++at_;
            }
        }
        else
        {
            return std::nullopt;
        }
        if (peek() == '.')
        {
            ++at_;
            if (!(peek() >= '0' && peek() <= '9'))
            {
                return std::nullopt;
            }
            while (peek() >= '0' && peek() <= '9')
            {
                ++at_;
            }
        }
        Json value;
        value.kind = Json::Kind::number;
        value.text = std::string{text_.substr(begin, at_ - begin)};
        return value;
    }

    [[nodiscard]] static bool append_code_point(std::string& out, unsigned code)
    {
        if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
        {
            return false;
        }
        if (code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        return true;
    }

    [[nodiscard]] std::optional<unsigned> read_hex4()
    {
        unsigned code = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = peek();
            ++at_;
            code <<= 4;
            if (c >= '0' && c <= '9')
            {
                code |= static_cast<unsigned>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                code |= static_cast<unsigned>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                code |= static_cast<unsigned>(c - 'A' + 10);
            }
            else
            {
                return std::nullopt;
            }
        }
        return code;
    }

    // The length of the valid UTF-8 sequence at `at_`, or 0.
    [[nodiscard]] std::size_t utf8_length() const noexcept
    {
        const auto byte = [this](std::size_t i) { return static_cast<unsigned char>(text_[at_ + i]); };
        const auto has = [this](std::size_t n) { return at_ + n <= text_.size(); };
        const auto cont = [&](std::size_t i, unsigned char low = 0x80, unsigned char high = 0xBF)
        { return byte(i) >= low && byte(i) <= high; };
        const unsigned char lead = byte(0);
        if (lead < 0x80)
        {
            return 1;
        }
        if (lead >= 0xC2 && lead <= 0xDF)
        {
            return has(2) && cont(1) ? 2 : 0;
        }
        if (lead >= 0xE0 && lead <= 0xEF)
        {
            return has(3) && cont(1, lead == 0xE0 ? 0xA0 : 0x80, lead == 0xED ? 0x9F : 0xBF) && cont(2) ? 3 : 0;
        }
        if (lead >= 0xF0 && lead <= 0xF4)
        {
            return has(4) && cont(1, lead == 0xF0 ? 0x90 : 0x80, lead == 0xF4 ? 0x8F : 0xBF) && cont(2) && cont(3) ? 4
                                                                                                                   : 0;
        }
        return 0;
    }

    [[nodiscard]] bool read_string(std::string& out)
    {
        ++at_; // the opening quote
        while (!done())
        {
            const char c = text_[at_];
            if (c == '"')
            {
                ++at_;
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20)
            {
                return false;
            }
            if (c != '\\')
            {
                const std::size_t length = utf8_length();
                if (length == 0)
                {
                    return false;
                }
                out.append(text_.substr(at_, length));
                at_ += length;
                continue;
            }
            ++at_;
            const char escape = peek();
            ++at_;
            switch (escape)
            {
            case '"':
            case '\\':
            case '/':
                out += escape;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u':
            {
                std::optional<unsigned> code = read_hex4();
                if (!code)
                {
                    return false;
                }
                if (*code >= 0xD800 && *code <= 0xDBFF)
                {
                    if (!take("\\u"))
                    {
                        return false;
                    }
                    const std::optional<unsigned> low = read_hex4();
                    if (!low || *low < 0xDC00 || *low > 0xDFFF)
                    {
                        return false;
                    }
                    code = 0x10000 + ((*code - 0xD800) << 10) + (*low - 0xDC00);
                }
                if (!append_code_point(out, *code))
                {
                    return false;
                }
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    [[nodiscard]] std::optional<Json> read_array(int depth)
    {
        ++at_;
        Json value;
        value.kind = Json::Kind::array;
        skip_space();
        if (peek() == ']')
        {
            ++at_;
            return value;
        }
        while (true)
        {
            std::optional<Json> element = read_value(depth + 1);
            if (!element)
            {
                return std::nullopt;
            }
            value.elements.push_back(std::move(*element));
            skip_space();
            if (peek() == ',')
            {
                ++at_;
                continue;
            }
            if (peek() == ']')
            {
                ++at_;
                return value;
            }
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<Json> read_object(int depth)
    {
        ++at_;
        Json value;
        value.kind = Json::Kind::object;
        skip_space();
        if (peek() == '}')
        {
            ++at_;
            return value;
        }
        while (true)
        {
            skip_space();
            if (peek() != '"')
            {
                return std::nullopt;
            }
            std::string key;
            if (!read_string(key))
            {
                return std::nullopt;
            }
            skip_space();
            if (peek() != ':')
            {
                return std::nullopt;
            }
            ++at_;
            std::optional<Json> member = read_value(depth + 1);
            if (!member)
            {
                return std::nullopt;
            }
            value.members.emplace_back(std::move(key), std::move(*member));
            skip_space();
            if (peek() == ',')
            {
                ++at_;
                continue;
            }
            if (peek() == '}')
            {
                ++at_;
                return value;
            }
            return std::nullopt;
        }
    }

    std::string_view text_;
    std::size_t at_ = 0;
};

} // namespace detail

// The value `text` is, or nothing if `text` is not one JSON value (and nothing after it but white space).
[[nodiscard]] inline std::optional<Json> parse_json(std::string_view text)
{
    return detail::MiniJsonReader(text).read_document();
}

// The lines of `text`, each parsed; a line that is not JSON (or is not an object) is a failure of the caller's
// to report, so it comes back as a null value with the line in `text`.
[[nodiscard]] inline std::vector<Json> parse_json_lines(std::string_view text)
{
    std::vector<Json> events;
    std::size_t begin = 0;
    while (begin < text.size())
    {
        std::size_t end = text.find('\n', begin);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        std::string_view line = text.substr(begin, end - begin);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        std::optional<Json> value = parse_json(line);
        if (value && value->kind == Json::Kind::object)
        {
            events.push_back(std::move(*value));
        }
        else
        {
            Json bad;
            bad.text = std::string{line};
            events.push_back(std::move(bad));
        }
        begin = end + 1;
    }
    return events;
}

} // namespace adl::test
