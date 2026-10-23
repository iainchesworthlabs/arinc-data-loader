#include "adl/arinc665/md5.hpp"

#include <algorithm>
#include <bit>

namespace adl::arinc665
{

namespace
{

// floor(2^32 * abs(sin(i + 1))), RFC 1321 section 3.4.
constexpr std::array<std::uint32_t, 64> sine_table{
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// Left rotation per step: four values repeated for each of the four rounds.
constexpr std::array<int, 16> rotations{7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};

} // namespace

void Md5::update(std::span<const std::byte> data) noexcept
{
    total_bytes_ += data.size();
    while (!data.empty())
    {
        const std::size_t take = std::min(data.size(), buffer_.size() - buffer_size_);
        std::copy_n(data.begin(), take, buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
        buffer_size_ += take;
        data = data.subspan(take);
        if (buffer_size_ == buffer_.size())
        {
            process_block(buffer_.data());
            buffer_size_ = 0;
        }
    }
}

void Md5::process_block(const std::byte* block) noexcept
{
    // The words of a block are little-endian.
    std::array<std::uint32_t, 16> m{};
    for (std::size_t i = 0; i < 16; ++i)
    {
        m[i] = std::to_integer<std::uint32_t>(block[4 * i]) | (std::to_integer<std::uint32_t>(block[4 * i + 1]) << 8) |
               (std::to_integer<std::uint32_t>(block[4 * i + 2]) << 16) |
               (std::to_integer<std::uint32_t>(block[4 * i + 3]) << 24);
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];

    for (std::size_t i = 0; i < 64; ++i)
    {
        std::uint32_t f = 0;
        std::size_t g = 0;
        if (i < 16)
        {
            f = (b & c) | (~b & d);
            g = i;
        }
        else if (i < 32)
        {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        }
        else if (i < 48)
        {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        }
        else
        {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        const std::uint32_t rotated = std::rotl(a + f + sine_table[i] + m[g], rotations[(i / 16) * 4 + i % 4]);
        a = d;
        d = c;
        c = b;
        b += rotated;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

Md5::Digest Md5::digest() const noexcept
{
    // Pad a copy: a 1 bit, zeros up to 56 bytes into a block, then the length in
    // bits as 64 bits, least significant byte first.
    Md5 copy = *this;
    const std::uint64_t bit_length = total_bytes_ * 8;
    const std::size_t pad_length = buffer_size_ < 56 ? 56 - buffer_size_ : 120 - buffer_size_;

    std::array<std::byte, 72> padding{};
    padding[0] = std::byte{0x80};
    for (std::size_t i = 0; i < 8; ++i)
    {
        padding[pad_length + i] = static_cast<std::byte>((bit_length >> (8 * i)) & 0xFFU);
    }
    copy.update(std::span<const std::byte>{padding}.first(pad_length + 8));

    Digest result{};
    for (std::size_t i = 0; i < copy.state_.size(); ++i)
    {
        for (std::size_t j = 0; j < 4; ++j)
        {
            result[4 * i + j] = static_cast<std::byte>((copy.state_[i] >> (8 * j)) & 0xFFU);
        }
    }
    return result;
}

} // namespace adl::arinc665
