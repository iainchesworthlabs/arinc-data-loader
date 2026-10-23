// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/address.hpp"

#include <QStringList>

namespace adl::app
{

std::optional<std::array<std::uint8_t, 4>> parse_ipv4(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() != 4)
    {
        return std::nullopt;
    }
    std::array<std::uint8_t, 4> octets{};
    for (qsizetype i = 0; i < 4; ++i)
    {
        const QString& part = parts[i];
        if (part.isEmpty() || part.size() > 3)
        {
            return std::nullopt;
        }
        for (const QChar c : part)
        {
            if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            {
                return std::nullopt;
            }
        }
        const int value = part.toInt();
        if (value > 255)
        {
            return std::nullopt;
        }
        octets[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value);
    }
    return octets;
}

QString format_ipv4(const std::array<std::uint8_t, 4>& octets)
{
    return QStringLiteral("%1.%2.%3.%4").arg(octets[0]).arg(octets[1]).arg(octets[2]).arg(octets[3]);
}

} // namespace adl::app
