// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

// The members of a media set as a list of files (checks.hpp says how the input is read): what does not read
// as a media set is refused with a reason, and what does is inspected and written as the document of
// `adl_make_set inspect --json`, which the strict reader has to read.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::set_maker::fuzz::check_members({data, size}).empty())
    {
        std::abort();
    }
    return 0;
}
