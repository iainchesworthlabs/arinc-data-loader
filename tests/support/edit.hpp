#pragma once

// Helpers for damaging files on purpose: changing a field, then putting the CRC
// right again so that the test reaches the check it is about.

#include "adl/arinc665/crc.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace adl::test
{

inline std::uint16_t get_u16(std::span<const std::byte> bytes, std::size_t word)
{
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(bytes[2 * word]) << 8) |
                                      std::to_integer<unsigned>(bytes[2 * word + 1]));
}

inline std::uint32_t get_u32(std::span<const std::byte> bytes, std::size_t word)
{
    return (static_cast<std::uint32_t>(get_u16(bytes, word)) << 16) | get_u16(bytes, word + 1);
}

inline void put_u16(std::vector<std::byte>& bytes, std::size_t word, std::uint16_t value)
{
    bytes[2 * word] = static_cast<std::byte>(value >> 8);
    bytes[2 * word + 1] = static_cast<std::byte>(value & 0xFFU);
}

inline void put_u32(std::vector<std::byte>& bytes, std::size_t word, std::uint32_t value)
{
    put_u16(bytes, word, static_cast<std::uint16_t>(value >> 16));
    put_u16(bytes, word + 1, static_cast<std::uint16_t>(value & 0xFFFFU));
}

// Rewrites the CRC that ends a list or batch file (the last word).
inline void refresh_final_crc(std::vector<std::byte>& bytes)
{
    const std::size_t words = bytes.size() / 2;
    put_u16(bytes, words - 1,
            adl::arinc665::crc_of<adl::arinc665::Crc16>(std::span<const std::byte>{bytes}.first((words - 1) * 2)));
}

// Rewrites the Header File CRC of a header file (the third word from the end).
inline void refresh_header_crc(std::vector<std::byte>& bytes)
{
    const std::size_t words = bytes.size() / 2;
    put_u16(bytes, words - 3,
            adl::arinc665::crc_of<adl::arinc665::Crc16>(std::span<const std::byte>{bytes}.first((words - 3) * 2)));
}

// Inserts `count` zero words before word `at`, and moves every pointer in
// `pointers` (word offsets of 32-bit pointer fields) that points beyond it, as a
// later format version adding fields at an expansion point would. The caller
// fixes the file length and any relative pointer that spans the insertion.
inline void insert_words(std::vector<std::byte>& bytes, std::size_t at, std::size_t count)
{
    bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(2 * at), 2 * count, std::byte{0});
}

} // namespace adl::test
