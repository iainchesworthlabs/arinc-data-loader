// SPDX-License-Identifier: GPL-3.0-or-later
#include "wire.hpp"

#include "adl/arinc665/crc.hpp"

#include <limits>

namespace adl::arinc665::detail
{

namespace
{

std::byte high_byte(std::uint16_t value)
{
    return static_cast<std::byte>(value >> 8);
}

std::byte low_byte(std::uint16_t value)
{
    return static_cast<std::byte>(value & 0xFFU);
}

} // namespace

void WordWriter::u16(std::uint16_t value)
{
    bytes_.push_back(high_byte(value));
    bytes_.push_back(low_byte(value));
}

void WordWriter::u32(std::uint32_t value)
{
    u16(static_cast<std::uint16_t>(value >> 16));
    u16(static_cast<std::uint16_t>(value & 0xFFFFU));
}

void WordWriter::u64(std::uint64_t value)
{
    u32(static_cast<std::uint32_t>(value >> 32));
    u32(static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
}

void WordWriter::u8_pair(std::uint8_t first, std::uint8_t second)
{
    bytes_.push_back(static_cast<std::byte>(first));
    bytes_.push_back(static_cast<std::byte>(second));
}

void WordWriter::bytes(std::span<const std::byte> data)
{
    bytes_.insert(bytes_.end(), data.begin(), data.end());
}

void WordWriter::string(std::string_view text)
{
    if (text.size() > max_field_value)
    {
        fail(Error::field_too_large);
        return;
    }
    u16(static_cast<std::uint16_t>(text.size()));
    for (const char character : text)
    {
        bytes_.push_back(static_cast<std::byte>(character));
    }
    if (text.size() % 2 != 0)
    {
        bytes_.push_back(std::byte{0});
    }
}

std::uint16_t WordWriter::fit_u16(std::size_t value) noexcept
{
    if (value > max_field_value)
    {
        fail(Error::field_too_large);
        return 0;
    }
    return static_cast<std::uint16_t>(value);
}

std::uint32_t WordWriter::fit_u32(std::size_t value) noexcept
{
    if (value > std::numeric_limits<std::uint32_t>::max())
    {
        fail(Error::field_too_large);
        return 0;
    }
    return static_cast<std::uint32_t>(value);
}

void WordWriter::count(std::size_t value)
{
    u16(fit_u16(value));
}

void WordWriter::check_value(const OptionalCheckValue& value)
{
    if (!value)
    {
        u16(0);
        return;
    }
    // The stored value is whole words, so the 16-bit fields after it stay aligned.
    if (value->bytes.size() % 2 != 0)
    {
        fail(Error::invalid_value);
        return;
    }
    // The length counts its own word and the type's as well as the value.
    u16(fit_u16(value->bytes.size() + 4));
    u16(static_cast<std::uint16_t>(value->type));
    bytes(value->bytes);
}

void WordWriter::patch_u16(std::size_t word, std::uint16_t value) noexcept
{
    bytes_[2 * word] = high_byte(value);
    bytes_[2 * word + 1] = low_byte(value);
}

void WordWriter::patch_u32(std::size_t word, std::uint32_t value) noexcept
{
    patch_u16(word, static_cast<std::uint16_t>(value >> 16));
    patch_u16(word + 1, static_cast<std::uint16_t>(value & 0xFFFFU));
}

void WordReader::seek(std::size_t word) noexcept
{
    if (word > size_words())
    {
        fail(Error::bad_pointer);
        return;
    }
    position_ = word * 2;
}

std::span<const std::byte> WordReader::bytes(std::size_t count) noexcept
{
    if (!ok())
    {
        return {};
    }
    if (count > data_.size() - position_)
    {
        fail(Error::truncated);
        return {};
    }
    const auto result = data_.subspan(position_, count);
    position_ += count;
    return result;
}

std::uint16_t WordReader::u16() noexcept
{
    const auto raw = bytes(2);
    if (raw.size() != 2)
    {
        return 0;
    }
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(raw[0]) << 8) | std::to_integer<unsigned>(raw[1]));
}

std::uint32_t WordReader::u32() noexcept
{
    const std::uint32_t high = u16();
    const std::uint32_t low = u16();
    return (high << 16) | low;
}

std::uint64_t WordReader::u64() noexcept
{
    const std::uint64_t high = u32();
    const std::uint64_t low = u32();
    return (high << 32) | low;
}

std::pair<std::uint8_t, std::uint8_t> WordReader::u8_pair() noexcept
{
    const auto raw = bytes(2);
    if (raw.size() != 2)
    {
        return {std::uint8_t{0}, std::uint8_t{0}};
    }
    return {std::to_integer<std::uint8_t>(raw[0]), std::to_integer<std::uint8_t>(raw[1])};
}

std::string WordReader::string()
{
    const std::size_t length = u16();
    const auto raw = bytes(length);
    std::string text;
    text.reserve(raw.size());
    for (const std::byte character : raw)
    {
        text.push_back(static_cast<char>(std::to_integer<unsigned char>(character)));
    }
    // An odd length is followed by a NUL that fills the word; its value is not
    // checked, so a file with another filler still reads.
    if (length % 2 != 0)
    {
        static_cast<void>(bytes(1));
    }
    return text;
}

OptionalCheckValue WordReader::check_value()
{
    const std::size_t length = u16();
    if (!ok() || length == 0)
    {
        return std::nullopt;
    }
    // The length covers itself and the type, so it is at least four bytes, and
    // an even number because the value is stored in whole words.
    if (length < 4 || length % 2 != 0)
    {
        fail(Error::bad_length);
        return std::nullopt;
    }
    const std::uint16_t type = u16();
    const auto raw = bytes(length - 4);
    if (!ok())
    {
        return std::nullopt;
    }
    CheckValue value;
    value.type = static_cast<CheckValueType>(type);
    value.bytes.assign(raw.begin(), raw.end());
    return value;
}

std::uint16_t crc16(std::span<const std::byte> data) noexcept
{
    return crc_of<Crc16>(data);
}

std::uint32_t crc32(std::span<const std::byte> data) noexcept
{
    return crc_of<Crc32>(data);
}

} // namespace adl::arinc665::detail
