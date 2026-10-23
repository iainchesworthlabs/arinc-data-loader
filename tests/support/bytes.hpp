#pragma once

// Helpers the tests share for building and comparing byte strings.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

// "0A FF 12" or "0AFF12" (spaces and newlines ignored) as bytes.
inline std::vector<std::byte> from_hex(std::string_view hex)
{
    std::vector<std::byte> bytes;
    int high = -1;
    for (const char c : hex)
    {
        int digit = -1;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = c - 'A' + 10;
        }
        else
        {
            continue;
        }
        if (high < 0)
        {
            high = digit;
        }
        else
        {
            bytes.push_back(static_cast<std::byte>((high << 4) | digit));
            high = -1;
        }
    }
    return bytes;
}

// Lower-case hex with no separators.
inline std::string to_hex(std::span<const std::byte> bytes)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text;
    for (const std::byte byte : bytes)
    {
        const auto value = std::to_integer<unsigned>(byte);
        text.push_back(digits[value >> 4]);
        text.push_back(digits[value & 0xFU]);
    }
    return text;
}

inline std::vector<std::byte> bytes_of(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text)
    {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    return bytes;
}

inline std::vector<std::byte> repeated(std::byte value, std::size_t count)
{
    return std::vector<std::byte>(count, value);
}

// `pattern` repeated `times` over.
inline std::vector<std::byte> repeated(std::span<const std::byte> pattern, std::size_t times)
{
    std::vector<std::byte> bytes;
    for (std::size_t i = 0; i < times; ++i)
    {
        bytes.insert(bytes.end(), pattern.begin(), pattern.end());
    }
    return bytes;
}

// The 256 bytes 00 01 ... FF.
inline std::vector<std::byte> ascending()
{
    std::vector<std::byte> bytes(256);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(i);
    }
    return bytes;
}

} // namespace adl::test
