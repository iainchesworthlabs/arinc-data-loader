// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/address.hpp"

#include "adl/app/text.hpp"
#include "adl/loader_core/address.hpp"

namespace adl::app
{

std::optional<std::array<std::uint8_t, 4>> parse_ipv4(const QString& text)
{
    return adl::loader_core::parse_ipv4(text.toStdString());
}

QString format_ipv4(const std::array<std::uint8_t, 4>& octets)
{
    return to_qstring(adl::loader_core::format_ipv4(octets));
}

} // namespace adl::app
