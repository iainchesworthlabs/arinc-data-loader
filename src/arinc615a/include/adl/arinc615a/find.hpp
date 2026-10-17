#pragma once

// The FIND packets of Attachment 3: the data loader broadcasts a request on UDP port 1001
// and each target answers with who it is. Both are a 16-bit opcode, a list of
// zero-terminated strings, and a final 0x10 byte. There are no error replies: a request
// or an answer that is not valid is ignored, which is what the decoders returning an
// error is for.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

#include "adl/arinc615a/error.hpp"

namespace adl::arinc615a {

enum class FindOpcode : std::uint16_t {
    request = 1,  // Information ReQuest, IRQ
    answer = 2,   // Information ANswer, IAN
};

// The byte that ends every FIND packet (Attachment 3, 3-4 and 3-5).
inline constexpr std::uint8_t find_packet_terminator = 0x10;

struct FindRequest {
    friend bool operator==(const FindRequest&, const FindRequest&) = default;
};

// One answer: who a target is, in the order of Attachment 3, 3-6, within the size limits
// listed in limits.hpp. An empty field is allowed (it is one NUL on the wire).
struct FindAnswer {
    std::string_view target_hardware_id;  // "HNPFMS", 15 characters at most
    std::string_view target_type_name;    // "FMS", 8
    std::string_view target_position;     // "L", 8
    std::string_view literal_name;        // "FMS LEFT", 20
    std::string_view manufacturer_code;   // "HNP", 3

    friend bool operator==(const FindAnswer&, const FindAnswer&) = default;
};

using FindPacket = std::variant<FindRequest, FindAnswer>;

// The most bytes an answer takes: the opcode, the five fields at their limits, five NULs
// and the terminator.
inline constexpr std::size_t find_max_answer_size = 2 + 15 + 8 + 8 + 20 + 3 + 5 + 1;

[[nodiscard]] Result<std::size_t> encode_find_request(std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_find_answer(const FindAnswer& answer,
                                                     std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const FindAnswer& answer) noexcept;

// Decodes a datagram, which must be exactly a request (opcode 1, one NUL, 0x10) or an
// answer (opcode 2, five strings within their limits, 0x10) with nothing after it.
[[nodiscard]] Result<FindPacket> decode_find(std::span<const std::byte> bytes) noexcept;

}  // namespace adl::arinc615a
