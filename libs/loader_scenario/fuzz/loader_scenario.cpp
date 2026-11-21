// SPDX-License-Identifier: GPL-3.0-or-later
#include "scenario_check.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

// A scenario file, as a supplier writes it. What is refused is refused with a sentence; what is read is written
// out and read again, and has to come back as the same scenario.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view text{reinterpret_cast<const char*>(data), size};
    if (adl::loader_scenario::fuzz::check_scenario(text) == adl::loader_scenario::fuzz::Verdict::inconsistent)
    {
        std::abort();
    }
    return 0;
}
