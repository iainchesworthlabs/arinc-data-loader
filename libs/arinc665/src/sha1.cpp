#include "adl/arinc665/sha1.hpp"

#include <algorithm>
#include <bit>

namespace adl::arinc665 {

void Sha1::update(std::span<const std::byte> data) noexcept {
    total_bytes_ += data.size();
    while (!data.empty()) {
        const std::size_t take = std::min(data.size(), buffer_.size() - buffer_size_);
        std::copy_n(data.begin(), take,
                    buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
        buffer_size_ += take;
        data = data.subspan(take);
        if (buffer_size_ == buffer_.size()) {
            process_block(buffer_.data());
            buffer_size_ = 0;
        }
    }
}

void Sha1::process_block(const std::byte* block) noexcept {
    std::array<std::uint32_t, 80> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (std::to_integer<std::uint32_t>(block[4 * i]) << 24) |
               (std::to_integer<std::uint32_t>(block[4 * i + 1]) << 16) |
               (std::to_integer<std::uint32_t>(block[4 * i + 2]) << 8) |
               std::to_integer<std::uint32_t>(block[4 * i + 3]);
    }
    for (std::size_t i = 16; i < 80; ++i) {
        w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];

    for (std::size_t i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999U;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        const std::uint32_t next = std::rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = next;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

Sha1::Digest Sha1::digest() const noexcept {
    // Pad a copy: a 1 bit, zeros up to 56 bytes into a block, then the length in
    // bits as 64 bits, most significant byte first.
    Sha1 copy = *this;
    const std::uint64_t bit_length = total_bytes_ * 8;
    const std::size_t pad_length = buffer_size_ < 56 ? 56 - buffer_size_ : 120 - buffer_size_;

    std::array<std::byte, 72> padding{};
    padding[0] = std::byte{0x80};
    for (std::size_t i = 0; i < 8; ++i) {
        padding[pad_length + i] = static_cast<std::byte>((bit_length >> (56 - 8 * i)) & 0xFFU);
    }
    copy.update(std::span<const std::byte>{padding}.first(pad_length + 8));

    Digest result{};
    for (std::size_t i = 0; i < copy.state_.size(); ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            result[4 * i + j] = static_cast<std::byte>((copy.state_[i] >> (24 - 8 * j)) & 0xFFU);
        }
    }
    return result;
}

}  // namespace adl::arinc665
