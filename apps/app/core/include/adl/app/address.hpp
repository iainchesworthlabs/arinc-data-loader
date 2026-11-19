// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A target's IPv4 address as the operator types it and sees it, as Qt strings. The parsing and the
// writing are adl::loader_core's (adl/loader_core/address.hpp); parsing is strict, because the result
// is where datagrams are sent.

#include <QString>
#include <array>
#include <cstdint>
#include <optional>

namespace adl::app
{

// "10.0.0.2" as four octets (adl::loader_core::parse_ipv4).
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> parse_ipv4(const QString& text);

[[nodiscard]] QString format_ipv4(const std::array<std::uint8_t, 4>& octets);

} // namespace adl::app
