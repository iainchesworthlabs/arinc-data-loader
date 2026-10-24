// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/header_file.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/samples/scenarios.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace
{

// 1 MiB of a pattern that is not all zeros, so a table-driven CRC and a block hash do their usual work.
std::vector<std::byte> megabyte()
{
    std::vector<std::byte> bytes(1U << 20);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>((i * 2654435761U) >> 24);
    }
    return bytes;
}

} // namespace

TEST_CASE("check values over 1 MiB", "[perf]")
{
    using adl::arinc665::CheckValueType;
    const auto data = megabyte();
    const std::span<const std::byte> bytes{data};

    BENCHMARK("crc16 1 MiB")
    {
        return adl::arinc665::compute_check_value(CheckValueType::crc16, bytes);
    };
    BENCHMARK("crc32 1 MiB")
    {
        return adl::arinc665::compute_check_value(CheckValueType::crc32, bytes);
    };
    BENCHMARK("md5 1 MiB")
    {
        return adl::arinc665::compute_check_value(CheckValueType::md5, bytes);
    };
    BENCHMARK("sha1 1 MiB")
    {
        return adl::arinc665::compute_check_value(CheckValueType::sha1, bytes);
    };
}

TEST_CASE("a load header file", "[perf]")
{
    const auto load = adl::samples::sample_load();
    REQUIRE(load.has_value());
    const std::span<const std::byte> bytes{load->header_bytes};

    BENCHMARK("decode the sample header file")
    {
        return adl::arinc665::decode_header_file(bytes);
    };
}

TEST_CASE("a media set", "[perf]")
{
    const auto spec = adl::samples::sample_media_set_spec();
    REQUIRE(spec.has_value());
    const auto members = adl::arinc665::build_media_set(*spec);
    REQUIRE(members.has_value());

    BENCHMARK("build the sample media set")
    {
        return adl::arinc665::build_media_set(*spec);
    };
    BENCHMARK("read and check the sample media set")
    {
        return adl::arinc665::read_media_set(*members);
    };
}
