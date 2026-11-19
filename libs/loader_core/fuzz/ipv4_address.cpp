// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/address.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

// The text of an IPv4 address, as an operator types it or a command line gives it. What is not an
// address is refused; what is one is written out and read again, and has to come back as the same four
// numbers.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view text{reinterpret_cast<const char*>(data), size};
    if (const auto octets = adl::loader_core::parse_ipv4(text))
    {
        if (adl::loader_core::parse_ipv4(adl::loader_core::format_ipv4(*octets)) != octets)
        {
            std::abort();
        }
    }
    return 0;
}
