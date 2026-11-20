// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What turns a command line into the UTF-8 arguments of a program, as functions of the text the
// operating system gave: the split that the C run-time of Windows does, and the conversions between
// UTF-8 and UTF-16 with U+FFFD for what is not valid text. They read no system state and throw
// nothing for bad text, so every case, including the odd ones, is tested on every host. Only
// windows/arguments_windows.cpp, which fetches the wide command line from the system, is Windows'
// own; the other platforms use copy_arguments and nothing else of this file.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace adl::platform::detail
{

// What stands for a byte or a unit that is not valid text.
inline constexpr char32_t replacement_character = 0xFFFD;

// The arguments main was given, as they are: the bytes the system passed, which on a POSIX system
// are the bytes of the names the user typed. A null argv, or a null entry, ends the list.
[[nodiscard]] inline std::vector<std::string> copy_arguments(int argc, const char* const* argv)
{
    std::vector<std::string> arguments;
    for (int i = 0; argv != nullptr && i < argc && argv[i] != nullptr; ++i)
    {
        arguments.emplace_back(argv[i]);
    }
    return arguments;
}

inline void append_utf8(std::string& text, char32_t code_point)
{
    if (code_point < 0x80)
    {
        text += static_cast<char>(code_point);
    }
    else if (code_point < 0x800)
    {
        text += static_cast<char>(0xC0 | (code_point >> 6));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    }
    else if (code_point < 0x10000)
    {
        text += static_cast<char>(0xE0 | (code_point >> 12));
        text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    }
    else
    {
        text += static_cast<char>(0xF0 | (code_point >> 18));
        text += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    }
}

[[nodiscard]] constexpr bool is_high_surrogate(char16_t unit) noexcept
{
    return unit >= 0xD800 && unit <= 0xDBFF;
}

[[nodiscard]] constexpr bool is_low_surrogate(char16_t unit) noexcept
{
    return unit >= 0xDC00 && unit <= 0xDFFF;
}

// UTF-16 as UTF-8. A surrogate that has no partner (half of a pair, as Windows allows in a name)
// is not text and becomes U+FFFD.
[[nodiscard]] inline std::string utf16_to_utf8(std::u16string_view text)
{
    std::string utf8;
    utf8.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char16_t unit = text[i];
        if (is_high_surrogate(unit) && i + 1 < text.size() && is_low_surrogate(text[i + 1]))
        {
            const char32_t high = static_cast<char32_t>(unit) - 0xD800;
            const char32_t low = static_cast<char32_t>(text[i + 1]) - 0xDC00;
            append_utf8(utf8, 0x10000 + (high << 10) + low);
            ++i;
        }
        else if (is_high_surrogate(unit) || is_low_surrogate(unit))
        {
            append_utf8(utf8, replacement_character);
        }
        else
        {
            append_utf8(utf8, static_cast<char32_t>(unit)); // not a surrogate, so the unit is the code point
        }
    }
    return utf8;
}

// The code point that starts at `index`, and `index` moved past it. A byte sequence that is not
// UTF-8 (a stray continuation byte, an overlong form, a surrogate, a value above U+10FFFF, a
// sequence cut short) gives U+FFFD, once for the longest start of a sequence that was valid, as
// Unicode recommends.
[[nodiscard]] inline char32_t next_code_point(std::string_view text, std::size_t& index) noexcept
{
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    ++index;
    if (lead < 0x80)
    {
        return lead;
    }

    std::size_t continuations = 0;
    char32_t code_point = 0;
    unsigned char lowest = 0x80;
    unsigned char highest = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        continuations = 1;
        code_point = lead & 0x1Fu;
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        continuations = 2;
        code_point = lead & 0x0Fu;
        lowest = lead == 0xE0 ? 0xA0 : 0x80;  // not an overlong form
        highest = lead == 0xED ? 0x9F : 0xBF; // not a surrogate
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        continuations = 3;
        code_point = lead & 0x07u;
        lowest = lead == 0xF0 ? 0x90 : 0x80;  // not an overlong form
        highest = lead == 0xF4 ? 0x8F : 0xBF; // not above U+10FFFF
    }
    else
    {
        return replacement_character;
    }

    for (std::size_t i = 0; i < continuations; ++i)
    {
        if (index >= text.size())
        {
            return replacement_character;
        }
        const unsigned char next = static_cast<unsigned char>(text[index]);
        if (next < lowest || next > highest)
        {
            return replacement_character;
        }
        code_point = (code_point << 6) | (next & 0x3Fu);
        ++index;
        lowest = 0x80;
        highest = 0xBF;
    }
    return code_point;
}

// UTF-8 as UTF-16, with U+FFFD for what is not UTF-8.
[[nodiscard]] inline std::u16string utf8_to_utf16(std::string_view text)
{
    std::u16string utf16;
    utf16.reserve(text.size());
    for (std::size_t index = 0; index < text.size();)
    {
        const char32_t code_point = next_code_point(text, index);
        if (code_point >= 0x10000)
        {
            const char32_t offset = code_point - 0x10000;
            utf16 += static_cast<char16_t>(0xD800 + (offset >> 10));
            utf16 += static_cast<char16_t>(0xDC00 + (offset & 0x3FF));
        }
        else
        {
            utf16 += static_cast<char16_t>(code_point);
        }
    }
    return utf16;
}

// The arguments of a Windows command line, program name first, as UTF-8. The split is the one the
// C run-time makes before it calls main, so that the arguments are those main would have been given
// (CommandLineToArgvW makes the same split of the arguments, but ends a quoted program name at its
// closing quote, so that `"a"b c` is two arguments there and one here, as it is for main):
//   - the program name ends at the first space or tab outside quotes; a quote switches quoting on or
//     off and is not kept, and a backslash is nothing special in it;
//   - an argument ends at a space or a tab that is not inside quotes; a quote starts or ends a quoted
//     part and is not itself kept, and inside a quoted part two quotes in a row make one quote;
//   - backslashes mean themselves unless a quote follows: 2n backslashes and a quote are n backslashes
//     and a quote that starts or ends a quoted part, 2n+1 backslashes and a quote are n backslashes and a
//     quote that is kept as text;
//   - an unclosed quote ends at the end of the line, and a pair of quotes alone is an empty argument.
// The text ends at the first NUL, as it does for the system. An empty line has no arguments, not
// even a program name. A unit that is half of a surrogate pair becomes U+FFFD in the argument.
[[nodiscard]] inline std::vector<std::string> arguments_from_command_line(std::u16string_view command_line)
{
    constexpr char16_t space = u' ';
    constexpr char16_t tab = u'\t';
    constexpr char16_t quote = u'"';
    constexpr char16_t backslash = u'\\';

    command_line = command_line.substr(0, command_line.find(u'\0'));
    const std::size_t end = command_line.size();
    std::size_t i = 0;
    const auto is_blank = [](char16_t unit) { return unit == space || unit == tab; };

    std::vector<std::string> arguments;
    if (end == 0)
    {
        return arguments;
    }

    // The program name is a legal file name, so it is not given the backslash rules of an argument: quotes
    // switch quoting on and off, are not kept, and a blank outside them ends the name.
    std::u16string name;
    bool name_in_quotes = false;
    for (; i < end && (name_in_quotes || !is_blank(command_line[i])); ++i)
    {
        if (command_line[i] == quote)
        {
            name_in_quotes = !name_in_quotes;
        }
        else
        {
            name += command_line[i];
        }
    }
    arguments.push_back(utf16_to_utf8(name));

    for (;;)
    {
        while (i < end && is_blank(command_line[i]))
        {
            ++i;
        }
        if (i >= end)
        {
            return arguments;
        }

        std::u16string argument;
        bool in_quotes = false;
        for (;;)
        {
            std::size_t backslashes = 0;
            for (; i < end && command_line[i] == backslash; ++i)
            {
                ++backslashes;
            }
            bool keep = true;
            if (i < end && command_line[i] == quote)
            {
                if (backslashes % 2 == 0)
                {
                    if (in_quotes && i + 1 < end && command_line[i + 1] == quote)
                    {
                        ++i; // two quotes inside a quoted part: the second one is kept
                    }
                    else
                    {
                        keep = false;
                        in_quotes = !in_quotes;
                    }
                }
                backslashes /= 2;
            }
            argument.append(backslashes, backslash);
            if (i >= end || (!in_quotes && is_blank(command_line[i])))
            {
                break;
            }
            if (keep)
            {
                argument += command_line[i];
            }
            ++i;
        }
        arguments.push_back(utf16_to_utf8(argument));
    }
}

} // namespace adl::platform::detail
