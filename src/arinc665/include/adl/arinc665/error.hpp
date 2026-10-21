#pragma once

// The error type of the ARINC 665 library and the Result alias that its encoders,
// decoders and builders return. Something the standard only recommends is a
// Finding (finding.hpp), not an Error.

#include <expected>
#include <string_view>

namespace adl::arinc665 {

// Why an operation in this library failed. Failures are returned, never
// thrown: the library is built to be usable where exceptions are not.
enum class Error {
    unsupported_check_type,  // the check value type is not one this library can calculate
    truncated,               // the data ends before a field does
    bad_length,       // a length field disagrees with the data, or the file is not whole words
    bad_version,      // a file format version this library does not read
    bad_pointer,      // a pointer lies outside the file, out of order, or in the wrong place
    bad_crc,          // a CRC does not match the data it covers
    bad_check_value,  // a check value does not match the data it covers, or is malformed
    field_too_large,  // a value does not fit the field the format gives it
    invalid_value,    // a field holds a value the standard does not allow
    inconsistent,     // two things that must agree do not
    file_not_found,   // a file the media set lists, or a load needs, is not there
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] constexpr std::string_view to_string(Error error) noexcept {
    switch (error) {
        case Error::unsupported_check_type:
            return "unsupported check value type";
        case Error::truncated:
            return "data ends before the field does";
        case Error::bad_length:
            return "length field disagrees with the data";
        case Error::bad_version:
            return "unsupported file format version";
        case Error::bad_pointer:
            return "bad pointer";
        case Error::bad_crc:
            return "CRC mismatch";
        case Error::bad_check_value:
            return "check value mismatch or malformed check value";
        case Error::field_too_large:
            return "value too large for its field";
        case Error::invalid_value:
            return "invalid field value";
        case Error::inconsistent:
            return "inconsistent";
        case Error::file_not_found:
            return "file not found";
    }
    return "unknown error";
}

}  // namespace adl::arinc665
