// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A target's IPv4 address as the operator types it and sees it, and the name the target's protocol
// files go by. Parsing is strict, because the result is where datagrams are sent.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace adl::loader_core
{

using Ipv4Octets = std::array<std::uint8_t, 4>;

// "10.0.0.2" as four octets. Four decimal numbers of 0 to 255 separated by dots, and nothing else (no
// spaces, signs, leading or trailing text, hexadecimal or short forms), because it is the address a
// datagram will be sent to. A number may have leading zeros, up to three digits in all.
[[nodiscard]] std::optional<Ipv4Octets> parse_ipv4(std::string_view text);

// The four octets in dotted decimal, with no leading zeros.
[[nodiscard]] std::string format_ipv4(const Ipv4Octets& octets);

// The <THW_ID_POS> a target's protocol files are named by: its hardware identifier, an underscore and
// its position ("HNPFMS" at "L" is "HNPFMS_L"), or the identifier alone for a target that gave no
// position. Whether the result is one the loader takes is adl::arinc615a::is_valid_thw_id_pos's to say.
[[nodiscard]] std::string thw_id_pos_of(std::string_view hardware_id, std::string_view position);

} // namespace adl::loader_core
