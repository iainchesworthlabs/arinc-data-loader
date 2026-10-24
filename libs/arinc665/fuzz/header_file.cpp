// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/header_file.hpp"

#include "fuzz_decoder.hpp"

// A load header file (.LUH): decoded, checked against the standard's recommendations and written back.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (const auto header = adl::arinc665::decode_header_file(adl::test::fuzz_bytes(data, size)))
    {
        static_cast<void>(adl::arinc665::check_header_file(*header));
        static_cast<void>(adl::arinc665::encode_header_file(*header));
    }
    return 0;
}
