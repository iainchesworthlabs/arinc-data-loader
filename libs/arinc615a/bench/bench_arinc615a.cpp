// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "bytes.hpp"
#include "golden_615a.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

TEST_CASE("TFTP data packets", "[perf]")
{
    // A full block at the default size (512 bytes) and at the largest RFC 2348 size (65464 would
    // not fit a datagram on every path; 8192 is the size a loader asks for in practice).
    for (const std::size_t size : {std::size_t{512}, std::size_t{8192}})
    {
        std::vector<std::byte> payload(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            payload[i] = static_cast<std::byte>(i * 31U);
        }
        std::vector<std::byte> wire(size + 4);
        const adl::arinc615a::TftpData packet{7, payload};

        BENCHMARK("encode a " + std::to_string(size) + " byte data packet")
        {
            return adl::arinc615a::encode_data(packet, wire);
        };
        const auto encoded = adl::arinc615a::encode_data(packet, wire);
        REQUIRE(encoded.has_value());
        const std::span<const std::byte> datagram{wire.data(), *encoded};
        BENCHMARK("decode a " + std::to_string(size) + " byte data packet")
        {
            return adl::arinc615a::decode_packet(datagram);
        };
    }
}

TEST_CASE("a read request with options", "[perf]")
{
    const auto bytes = adl::test::from_hex(adl::test::golden::rrq_checksum_hex);
    BENCHMARK("decode a read request with the checksum option")
    {
        return adl::arinc615a::decode_packet(bytes);
    };
}

TEST_CASE("a FIND answer", "[perf]")
{
    const auto bytes = adl::test::from_hex(adl::test::golden::find_answer_hex);
    BENCHMARK("decode a FIND answer")
    {
        return adl::arinc615a::decode_find(bytes);
    };
}
