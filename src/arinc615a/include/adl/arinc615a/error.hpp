#pragma once

// The error type of the ARINC 615A codecs and the Result alias their encoders and
// decoders return. How an operation as a whole ended is reported differently, as
// an OutcomeCode (data_loader.hpp).

#include <expected>
#include <string_view>

namespace adl::arinc615a {

// Why an encode or a decode in this library failed. Failures are returned, never
// thrown: the library is built to be usable where exceptions are not.
enum class Error {
    buffer_too_small,  // the caller's output buffer cannot hold the encoding
    truncated,         // the data ends before a field does
    bad_length,        // a length disagrees with the data, bytes follow the end, or a size is
                       // above its limit
    unknown_opcode,    // a TFTP or FIND opcode this library does not know
    unsupported_mode,  // a TFTP transfer mode other than octet
    bad_version,       // a protocol file whose Protocol Version is not "A4"
    malformed_text,    // text that is not terminated, or has a NUL or a control character in it
    invalid_value,     // a field holds a value the standard does not allow
    field_too_large,   // a value or a text does not fit the field the format gives it
    bad_option,        // a TFTP option that is malformed, out of range or repeated
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] constexpr std::string_view to_string(Error error) noexcept {
    switch (error) {
        case Error::buffer_too_small:
            return "output buffer too small";
        case Error::truncated:
            return "data ends before the field does";
        case Error::bad_length:
            return "length disagrees with the data, or is above its limit";
        case Error::unknown_opcode:
            return "unknown opcode";
        case Error::unsupported_mode:
            return "unsupported TFTP mode";
        case Error::bad_version:
            return "unsupported protocol version";
        case Error::malformed_text:
            return "malformed text";
        case Error::invalid_value:
            return "invalid field value";
        case Error::field_too_large:
            return "value too large for its field";
        case Error::bad_option:
            return "bad TFTP option";
    }
    return "unknown error";
}

}  // namespace adl::arinc615a
