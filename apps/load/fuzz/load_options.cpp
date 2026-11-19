// SPDX-License-Identifier: GPL-3.0-or-later
#include "load_options_fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// The arguments of a command line, cut from the fuzzer's bytes at each NUL. What is not a command is
// refused; what is one is written out again and read again, and has to give the same options.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::test::fuzz_options(data, size).round_trips)
    {
        std::abort();
    }
    return 0;
}
