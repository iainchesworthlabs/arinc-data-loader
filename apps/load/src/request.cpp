// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/request.hpp"

#include <algorithm>
#include <istream>
#include <vector>

namespace adl::load
{

namespace
{

constexpr std::size_t max_id_length = 64;

[[nodiscard]] bool is_continuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

// Whether `text` is valid UTF-8: the shortest form of each character, no surrogates, nothing above U+10FFFF.
[[nodiscard]] bool is_valid_utf8(std::string_view text) noexcept
{
    std::size_t i = 0;
    while (i < text.size())
    {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80)
        {
            ++i;
            continue;
        }
        std::size_t length = 0;
        unsigned lowest = 0;
        if (lead >= 0xC2 && lead <= 0xDF)
        {
            length = 2;
            lowest = 0x80;
        }
        else if (lead >= 0xE0 && lead <= 0xEF)
        {
            length = 3;
            lowest = 0x800;
        }
        else if (lead >= 0xF0 && lead <= 0xF4)
        {
            length = 4;
            lowest = 0x10000;
        }
        else
        {
            return false;
        }
        if (i + length > text.size())
        {
            return false;
        }
        unsigned code = lead & (0xFFU >> (length + 1));
        for (std::size_t k = 1; k < length; ++k)
        {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if (!is_continuation(next))
            {
                return false;
            }
            code = (code << 6) | (next & 0x3FU);
        }
        if (code < lowest || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
        {
            return false;
        }
        i += length;
    }
    return true;
}

[[nodiscard]] std::vector<std::string_view> split_fields(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    for (;;)
    {
        const std::size_t tab = line.find('\t', begin);
        if (tab == std::string_view::npos)
        {
            fields.push_back(line.substr(begin));
            return fields;
        }
        fields.push_back(line.substr(begin, tab - begin));
        begin = tab + 1;
    }
}

[[nodiscard]] std::string in_quotes(std::string_view text)
{
    return '"' + std::string{text} + '"';
}

// A sentence about a field that is not shown whole, so that a long one does not fill the line.
[[nodiscard]] std::string shown(std::string_view text)
{
    constexpr std::size_t most = 40;
    if (text.size() <= most)
    {
        return in_quotes(text);
    }
    // Cut where a character begins, so that the sentence stays UTF-8.
    std::size_t cut = most;
    while (cut > 0 && is_continuation(static_cast<unsigned char>(text[cut])))
    {
        --cut;
    }
    return in_quotes(text.substr(0, cut)) + "...";
}

} // namespace

bool is_valid_id(std::string_view id) noexcept
{
    if (id.empty() || id.size() > max_id_length)
    {
        return false;
    }
    return std::ranges::all_of(id,
                               [](char c)
                               {
                                   return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                          c == '.' || c == '_' || c == '-';
                               });
}

std::expected<Request, RequestError> parse_request(std::string_view line)
{
    if (!is_valid_utf8(line))
    {
        return std::unexpected(RequestError{std::nullopt, std::nullopt, "The line is not valid UTF-8."});
    }
    if (line.find('\0') != std::string_view::npos)
    {
        return std::unexpected(RequestError{std::nullopt, std::nullopt, "The line has a NUL character in it."});
    }
    const std::vector<std::string_view> fields = split_fields(line);
    const std::string_view word = fields.front();

    if (word.empty())
    {
        return std::unexpected(RequestError{std::nullopt, std::nullopt,
                                            line.empty() ? "The line is empty."
                                                         : "The line does not begin with a request: run, "
                                                           "cancel or stop."});
    }
    if (word == "stop")
    {
        if (fields.size() != 1)
        {
            return std::unexpected(RequestError{std::nullopt, "stop", "stop takes no fields."});
        }
        return Request{RequestKind::stop, {}, {}};
    }
    if (word != "run" && word != "cancel")
    {
        return std::unexpected(
            RequestError{std::nullopt, std::nullopt, "Unknown request " + shown(word) + ": run, cancel or stop."});
    }

    // run and cancel both name an operation.
    if (fields.size() < 2 || !is_valid_id(fields[1]))
    {
        return std::unexpected(
            RequestError{std::nullopt, std::string{word},
                         std::string{word} + (fields.size() < 2
                                                  ? " needs an id."
                                                  : " needs an id of 1 to 64 of the characters A to Z, a to z, 0 to 9, "
                                                    "dot, underscore and hyphen, not " +
                                                        shown(fields[1]) + ".")});
    }
    const std::string id{fields[1]};

    if (word == "cancel")
    {
        if (fields.size() != 2)
        {
            return std::unexpected(RequestError{id, "cancel", "cancel takes an id and nothing else."});
        }
        return Request{RequestKind::cancel, id, {}};
    }

    if (fields.size() < 3)
    {
        return std::unexpected(
            RequestError{id, "run", "run needs a command after the id: information, upload or download."});
    }
    const std::vector<std::string_view> arguments(fields.begin() + 2, fields.end());
    auto options = parse_run_arguments(arguments);
    if (!options)
    {
        return std::unexpected(RequestError{id, "run", std::move(options.error())});
    }
    return Request{RequestKind::run, id, std::move(*options)};
}

LineEnd read_line(std::istream& in, std::string& text, std::size_t max_bytes)
{
    text.clear();
    bool too_long = false;
    bool any = false;
    for (;;)
    {
        const int c = in.get();
        if (c == std::istream::traits_type::eof())
        {
            return any ? LineEnd::unfinished : LineEnd::end;
        }
        any = true;
        if (c == '\n')
        {
            if (!too_long && !text.empty() && text.back() == '\r')
            {
                text.pop_back();
            }
            if (too_long || text.size() > max_bytes)
            {
                text.clear();
                return LineEnd::too_long;
            }
            return LineEnd::line;
        }
        if (too_long)
        {
            continue;
        }
        if (text.size() > max_bytes)
        {
            // Past the longest line and its carriage return: what is left of it is read and dropped.
            too_long = true;
            text.clear();
            continue;
        }
        text.push_back(static_cast<char>(c));
    }
}

} // namespace adl::load
