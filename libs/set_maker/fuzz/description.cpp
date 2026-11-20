// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

// The bytes of a description file: what is not a description is refused, and what is one is inside the limits,
// accepted by the generator's rules and written back as a description that reads as the same one
// (checks.hpp says what is checked).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::set_maker::fuzz::check_description({reinterpret_cast<const char*>(data), size}).empty())
    {
        std::abort();
    }
    return 0;
}
