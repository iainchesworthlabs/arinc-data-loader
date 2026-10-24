// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc615a/find.hpp"

#include "fuzz_decoder.hpp"

// A FIND packet (a target's answer to the loader's broadcast).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return adl::test::fuzz_decode_only<&adl::arinc615a::decode_find>(data, size);
}
