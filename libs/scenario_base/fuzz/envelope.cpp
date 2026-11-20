// SPDX-License-Identifier: GPL-3.0-or-later
#include "fuzz_scenario_envelope.hpp"

#include <cstdint>
#include <cstdlib>

// The shared reader of the scenario files: the input is a scenario file, as the mock target's or the loader's
// program is given one. The reader returns an error with one line of words for a file it cannot use and never
// crashes; what it accepts is checked against the limits it states (tests/support/fuzz_scenario_envelope.hpp).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::test::fuzz_scenario_envelope(data, size))
    {
        std::abort();
    }
    return 0;
}
