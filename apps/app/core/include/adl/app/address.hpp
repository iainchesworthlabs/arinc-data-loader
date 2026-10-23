#pragma once

// A target's IPv4 address as the operator types it and sees it. Parsing is strict,
// because the result is where datagrams are sent.

#include <QString>
#include <array>
#include <cstdint>
#include <optional>

namespace adl::app
{

// "10.0.0.2" as four octets. Four decimal numbers of 0 to 255 separated by dots, and nothing
// else (no spaces, signs, leading or trailing text, hexadecimal or short forms), because it is
// the address a datagram will be sent to.
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> parse_ipv4(const QString& text);

[[nodiscard]] QString format_ipv4(const std::array<std::uint8_t, 4>& octets);

} // namespace adl::app
