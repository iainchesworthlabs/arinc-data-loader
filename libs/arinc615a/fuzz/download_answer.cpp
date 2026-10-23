// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc615a/protocol_files.hpp"
#include "fuzz_decoder.hpp"

// The protocol file read by decode_download_answer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return adl::test::fuzz_decode_only<&adl::arinc615a::decode_download_answer>(data, size);
}
