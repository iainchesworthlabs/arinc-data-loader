// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace adl::licence_owner
{

namespace
{

void append_code_point(std::string& out, std::uint32_t code_point)
{
    if (code_point < 0x80)
    {
        out.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else if (code_point < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

} // namespace

std::optional<std::string> utf16_to_utf8(std::u16string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const std::uint32_t unit = text[i];
        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            // A high surrogate must be followed by a low one.
            if (i + 1 >= text.size() || text[i + 1] < 0xDC00 || text[i + 1] > 0xDFFF)
            {
                return std::nullopt;
            }
            const std::uint32_t low = text[i + 1];
            append_code_point(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
            ++i;
        }
        else if (unit >= 0xDC00 && unit <= 0xDFFF)
        {
            return std::nullopt;
        }
        else
        {
            append_code_point(out, unit);
        }
    }
    return out;
}

std::filesystem::path path_from_utf8(std::string_view text)
{
    std::u8string wide(text.size(), u8'\0');
    std::ranges::transform(text, wide.begin(), [](char c) { return static_cast<char8_t>(c); });
    return std::filesystem::path(std::move(wide));
}

std::string path_to_utf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    std::string out(text.size(), '\0');
    std::ranges::transform(text, out.begin(), [](char8_t c) { return static_cast<char>(c); });
    return out;
}

} // namespace adl::licence_owner
