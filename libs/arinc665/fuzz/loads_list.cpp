// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/media_files.hpp"
#include "fuzz_decoder.hpp"

// LOADS.LUM: decoded, checked and written back.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (const auto list = adl::arinc665::decode_loads_list(adl::test::fuzz_bytes(data, size)))
    {
        static_cast<void>(adl::arinc665::check_loads_list(*list));
        static_cast<void>(adl::arinc665::encode_loads_list(*list));
    }
    return 0;
}
