#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace adl::arinc665 {

// The parameters of one of the CRCs in ARINC 665 section 4.3 (Tables 4.3-1 to
// 4.3-3). All of them are processed most-significant-bit first, with neither
// the input nor the result reflected, so those two switches are not here.
template <std::unsigned_integral T>
struct CrcParameters {
    T polynomial;  // the generator, most significant bit suppressed
    T initial;
    T xor_out;
};

namespace detail {

// The 256-entry lookup table for a most-significant-bit-first CRC, built at
// compile time: no start-up work, no heap, and nothing whose initialisation
// order matters.
template <std::unsigned_integral T>
[[nodiscard]] constexpr std::array<T, 256> make_crc_table(T polynomial) noexcept {
    constexpr int width = std::numeric_limits<T>::digits;
    constexpr T top_bit = static_cast<T>(T{1} << (width - 1));

    std::array<T, 256> table{};
    for (unsigned index = 0; index < table.size(); ++index) {
        T remainder = static_cast<T>(static_cast<T>(index) << (width - 8));
        for (int bit = 0; bit < 8; ++bit) {
            const bool carry = (remainder & top_bit) != 0;
            remainder = static_cast<T>(remainder << 1);
            if (carry) {
                remainder = static_cast<T>(remainder ^ polynomial);
            }
        }
        table[index] = remainder;
    }
    return table;
}

}  // namespace detail

// A running CRC. Feed it bytes with update() in as many pieces as you like and
// read value() at any point; reset() starts again.
template <std::unsigned_integral T, CrcParameters<T> Parameters>
class Crc {
    static constexpr int width = std::numeric_limits<T>::digits;
    static_assert(width == 8 || width == 16 || width == 32,
                  "ARINC 665 defines 8, 16 and 32-bit CRCs");

public:
    using value_type = T;

    constexpr void reset() noexcept { state_ = Parameters.initial; }

    constexpr void update(std::span<const std::byte> data) noexcept {
        for (const std::byte byte : data) {
            const unsigned index =
                (static_cast<unsigned>(state_ >> (width - 8)) ^ std::to_integer<unsigned>(byte)) &
                0xFFU;
            state_ = static_cast<T>(static_cast<T>(state_ << 8) ^ table_[index]);
        }
    }

    // The CRC of everything fed in since the last reset().
    [[nodiscard]] constexpr T value() const noexcept {
        return static_cast<T>(state_ ^ Parameters.xor_out);
    }

private:
    static constexpr std::array<T, 256> table_ = detail::make_crc_table<T>(Parameters.polynomial);

    T state_ = Parameters.initial;
};

// The CRC of `data` in one call.
template <typename CrcType>
[[nodiscard]] constexpr typename CrcType::value_type crc_of(
    std::span<const std::byte> data) noexcept {
    CrcType crc;
    crc.update(data);
    return crc.value();
}

// Table 4.3-1: polynomial X^8 + 1.
inline constexpr CrcParameters<std::uint8_t> crc8_parameters{0x01, 0x00, 0x00};
// Table 4.3-2: polynomial X^16 + X^12 + X^5 + 1.
inline constexpr CrcParameters<std::uint16_t> crc16_parameters{0x1021, 0xFFFF, 0x0000};
// Table 4.3-3: polynomial 0x04C11DB7.
inline constexpr CrcParameters<std::uint32_t> crc32_parameters{0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF};

using Crc8 = Crc<std::uint8_t, crc8_parameters>;
using Crc16 = Crc<std::uint16_t, crc16_parameters>;
using Crc32 = Crc<std::uint32_t, crc32_parameters>;

}  // namespace adl::arinc665
