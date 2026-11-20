// SPDX-License-Identifier: GPL-3.0-or-later
#include "messages.hpp"

#include <cstddef>

namespace adl::licence_cli
{

std::string format(std::string_view text, std::initializer_list<std::string_view> values)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '%' && i + 1 < text.size() && text[i + 1] >= '1' && text[i + 1] <= '9')
        {
            const auto index = static_cast<std::size_t>(text[i + 1] - '1');
            if (index < values.size())
            {
                out += *(values.begin() + static_cast<std::ptrdiff_t>(index));
            }
            ++i;
        }
        else
        {
            out.push_back(text[i]);
        }
    }
    return out;
}

} // namespace adl::licence_cli
