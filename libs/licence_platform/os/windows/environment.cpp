// SPDX-License-Identifier: GPL-3.0-or-later
// The environment as UTF-16 (Windows), converted to UTF-8, so that the code page is never involved. The C
// run-time's own copy of the variable is read with _wdupenv_s, which needs no operating system header and does
// not warn as getenv does. CMake compiles this file and not os/posix/environment.cpp, chosen by the platform's
// name (libs/licence_platform/CMakeLists.txt).

#include "adl/licence_platform/environment.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>

namespace adl::licence_platform
{

namespace
{

void append_utf8(std::string& out, char32_t code)
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
    else if (code < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

// UTF-16 as UTF-8. A surrogate that is not half of a pair becomes U+FFFD.
std::string utf16_to_utf8(const wchar_t* units)
{
    std::string out;
    for (const wchar_t* unit = units; *unit != L'\0'; ++unit)
    {
        const auto first = static_cast<char32_t>(static_cast<char16_t>(*unit));
        if (first >= 0xD800 && first <= 0xDBFF)
        {
            const auto second = static_cast<char32_t>(static_cast<char16_t>(unit[1]));
            if (second >= 0xDC00 && second <= 0xDFFF)
            {
                append_utf8(out, 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00));
                ++unit;
                continue;
            }
            append_utf8(out, 0xFFFD);
        }
        else if (first >= 0xDC00 && first <= 0xDFFF)
        {
            append_utf8(out, 0xFFFD);
        }
        else
        {
            append_utf8(out, first);
        }
    }
    return out;
}

} // namespace

std::optional<std::string> environment_variable(std::string_view name)
{
    std::wstring wide_name;
    for (const char letter : name)
    {
        wide_name.push_back(static_cast<wchar_t>(static_cast<unsigned char>(letter)));
    }
    wchar_t* value = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&value, &length, wide_name.c_str()) != 0 || value == nullptr)
    {
        return std::nullopt;
    }
    std::string text = utf16_to_utf8(value);
    std::free(value);
    return text;
}

} // namespace adl::licence_platform
