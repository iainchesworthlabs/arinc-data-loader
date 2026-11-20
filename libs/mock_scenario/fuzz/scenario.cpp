// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuzz_mock_scenario.hpp"

#include <cstdint>
#include <cstdlib>

// The mock target's scenario file: the input is the file, as a person chose to give it to adl_mock or to the window.
// The reader returns an error with one line of words for a file it cannot use and never crashes; what it reads is
// checked against the limits it states, written and read again as the same scenario, and played for a few moments
// (tests/support/fuzz_mock_scenario.hpp).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::test::fuzz_mock_scenario(data, size))
    {
        std::abort();
    }
    return 0;
}
