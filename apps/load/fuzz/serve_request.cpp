// SPDX-License-Identifier: GPL-3.0-or-later
#include "serve_request_fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// The bytes of standard input of `adl_load serve`: cut into lines, each read as a request. Whatever the bytes are,
// every line is a request or the sentence that refuses it, and a request that was read has the id it names.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (!adl::test::fuzz_requests(data, size).consistent)
    {
        std::abort();
    }
    return 0;
}
