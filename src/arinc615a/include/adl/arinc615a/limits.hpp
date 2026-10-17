#pragma once

// The figures of ARINC 615A-3 that the codecs check against, and that the
// protocol engine above them will use. Section numbers are 615A-3's.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adl::arinc615a {

// Section 6.4: the Protocol Version of Supplement 3, two ASCII characters, always the
// second field of a protocol file.
inline constexpr std::string_view protocol_version = "A4";

// Section 5.3.2.3.2: the TFTP control port of the Target Hardware, and section 5.3.3: the
// port of FIND.
inline constexpr std::uint16_t tftp_control_port = 59;
inline constexpr std::uint16_t find_port = 1001;

// Section 5.3.2.3.8.6: a "port" option should name a port from here up.
inline constexpr std::uint16_t first_private_port = 49152;

// RFC 1350's block size and RFC 2348's limits on the "blksize" option.
inline constexpr std::size_t tftp_default_block_size = 512;
inline constexpr std::size_t tftp_min_block_size = 8;
inline constexpr std::size_t tftp_max_block_size = 65464;

// Section 5.3.2.3.4: the longest wait, in seconds.
inline constexpr std::uint16_t max_wait_seconds = 65535;

// Section 6.4: a text field has an 8-bit length, so it holds at most 255 bytes. The
// terminator is one of them (see docs/arinc615a-codec.md), leaving 254 characters.
inline constexpr std::size_t max_field_bytes = 255;
inline constexpr std::size_t max_text_chars = 254;

// Section 6.4.6: the most bytes of user defined data in a .LNR entry.
inline constexpr std::size_t max_user_data_bytes = 255;

// Attachment 3, 3-6: the most characters of each FIND answer field.
inline constexpr std::size_t find_max_hardware_id = 15;
inline constexpr std::size_t find_max_type_name = 8;
inline constexpr std::size_t find_max_position = 8;
inline constexpr std::size_t find_max_literal_name = 20;
inline constexpr std::size_t find_max_manufacturer_code = 3;

// The estimated time field when the target cannot say (sections 6.4.3 to 6.4.7).
inline constexpr std::uint16_t estimated_time_unknown = 0xFFFF;

}  // namespace adl::arinc615a
