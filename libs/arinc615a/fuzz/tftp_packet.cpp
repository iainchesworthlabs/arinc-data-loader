// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc615a/tftp.hpp"
#include "fuzz_decoder.hpp"

#include <variant>

// A datagram of the TFTP dialogue: decoded, and the transfer options of a request or an option
// acknowledgement read. This is the parser that faces the network.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const auto packet = adl::arinc615a::decode_packet(adl::test::fuzz_bytes(data, size));
    if (!packet)
    {
        return 0;
    }
    if (const auto* request = std::get_if<adl::arinc615a::TftpRequest>(&*packet))
    {
        static_cast<void>(adl::arinc615a::read_transfer_options(request->options));
    }
    else if (const auto* acknowledgement = std::get_if<adl::arinc615a::TftpOptionAck>(&*packet))
    {
        static_cast<void>(adl::arinc615a::read_transfer_options(acknowledgement->options));
    }
    return 0;
}
