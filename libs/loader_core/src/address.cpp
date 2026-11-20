// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/address.hpp"

namespace adl::loader_core
{

std::optional<Ipv4Octets> parse_ipv4(std::string_view text)
{
    Ipv4Octets octets{};
    for (std::size_t i = 0; i < octets.size(); ++i)
    {
        // Every number but the last ends at a dot, and the last one at the end of the text.
        const bool last = i + 1 == octets.size();
        const std::size_t dot = text.find('.');
        if ((dot == std::string_view::npos) != last)
        {
            return std::nullopt;
        }
        const std::string_view part = text.substr(0, dot);
        text = last ? std::string_view{} : text.substr(dot + 1);
        if (part.empty() || part.size() > 3)
        {
            return std::nullopt;
        }
        unsigned value = 0;
        for (const char c : part)
        {
            if (c < '0' || c > '9')
            {
                return std::nullopt;
            }
            value = value * 10 + static_cast<unsigned>(c - '0');
        }
        if (value > 255)
        {
            return std::nullopt;
        }
        octets[i] = static_cast<std::uint8_t>(value);
    }
    return octets;
}

std::string format_ipv4(const Ipv4Octets& octets)
{
    std::string text;
    for (const std::uint8_t octet : octets)
    {
        if (!text.empty())
        {
            text += '.';
        }
        text += std::to_string(octet);
    }
    return text;
}

std::string thw_id_pos_of(std::string_view hardware_id, std::string_view position)
{
    std::string id{hardware_id};
    if (!position.empty())
    {
        id += '_';
        id += position;
    }
    return id;
}

} // namespace adl::loader_core
