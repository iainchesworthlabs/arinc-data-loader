// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuzz_mock_options.hpp"

#include <cstdint>
#include <cstdlib>

// The command line of adl_mock: the input is the arguments, a line feed after each one. The parser
// returns an error for a command line it cannot use and never crashes; what it accepts is checked
// against the limits it states (tests/support/fuzz_mock_options.hpp), and an address it reads is
// written back as it was.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::test::fuzz_mock_options(data, size))
    {
        std::abort();
    }
    return 0;
}
