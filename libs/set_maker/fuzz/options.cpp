// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

// The command line of adl_make_set, one argument to a line: what cannot be used is refused with a sentence, and what
// can is a command with its subject and only the options that belong to it (checks.hpp says what is checked).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::set_maker::fuzz::check_options({reinterpret_cast<const char*>(data), size}).empty())
    {
        std::abort();
    }
    return 0;
}
