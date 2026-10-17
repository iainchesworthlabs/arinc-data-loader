#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/error.hpp"

namespace adl::arinc665 {

// A check value as ARINC 665-3 section 5 stores it: the type, and the value
// bytes exactly as they are written to the file, most significant byte first
// and in whole 16-bit words. An 8-bit CRC is therefore two bytes, a zero and
// the CRC. A type this library does not know is held as it was read.
struct CheckValue {
    CheckValueType type = CheckValueType::none;
    std::vector<std::byte> bytes;

    friend bool operator==(const CheckValue&, const CheckValue&) = default;
};

// Calculates the check value of `type` over `data`. Fails with
// Error::unsupported_check_type for a type that is not a calculation (none, or
// one this library does not know).
[[nodiscard]] Result<CheckValue> compute_check_value(CheckValueType type,
                                                     std::span<const std::byte> data);

// A file field that may carry a check value (data, support and listed files):
// absent means the length field is 0x0000 and nothing follows it.
using OptionalCheckValue = std::optional<CheckValue>;

}  // namespace adl::arinc665
