// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/batch_file.hpp"

#include "fuzz_decoder.hpp"

// A batch file (.LUB): decoded, checked and written back.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (const auto batch = adl::arinc665::decode_batch_file(adl::test::fuzz_bytes(data, size)))
    {
        static_cast<void>(adl::arinc665::check_batch_file(*batch));
        static_cast<void>(adl::arinc665::encode_batch_file(*batch));
    }
    return 0;
}
