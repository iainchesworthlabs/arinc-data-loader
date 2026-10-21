#pragma once

// SHA-1, the check value of type 5 in ARINC 665. A streaming calculation with
// fixed storage: bytes are fed in any number of pieces.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace adl::arinc665 {

// SHA-1, the check value ARINC 665-3 section 5.3.2 names (type 5). Feed bytes
// with update() and read digest() at any point; reset() starts again.
class Sha1 {
public:
    static constexpr std::size_t digest_size = 20;
    using Digest = std::array<std::byte, digest_size>;

    void reset() noexcept { *this = Sha1{}; }
    void update(std::span<const std::byte> data) noexcept;

    // The digest of everything fed in so far. It does not disturb the running
    // state, so more data can follow.
    [[nodiscard]] Digest digest() const noexcept;

private:
    void process_block(const std::byte* block) noexcept;

    std::array<std::uint32_t, 5> state_{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U,
                                        0xC3D2E1F0U};
    std::array<std::byte, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

}  // namespace adl::arinc665
