#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace adl::arinc665 {

// MD5 as RFC 1321 defines it, the check value ARINC 665-3 section 5.3.1 names
// (type 4). Feed bytes with update() and read digest() at any point; reset()
// starts again.
class Md5 {
public:
    static constexpr std::size_t digest_size = 16;
    using Digest = std::array<std::byte, digest_size>;

    void reset() noexcept { *this = Md5{}; }
    void update(std::span<const std::byte> data) noexcept;

    // The digest of everything fed in so far. It does not disturb the running
    // state, so more data can follow.
    [[nodiscard]] Digest digest() const noexcept;

private:
    void process_block(const std::byte* block) noexcept;

    std::array<std::uint32_t, 4> state_{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U};
    std::array<std::byte, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

}  // namespace adl::arinc665
