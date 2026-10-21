// The library's output, byte for byte, against files written independently by
// tests/golden/make_golden.py from the tables and figures of the standard.

#include <catch2/catch_test_macros.hpp>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/header_file.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/arinc665/media_set.hpp"
#include "bytes.hpp"
#include "fixtures.hpp"
#include "golden_payloads.hpp"

using namespace adl::arinc665;
using adl::test::from_hex;
namespace golden = adl::test::golden;

TEST_CASE("the header file is the one written independently", "[golden]") {
    const auto load = adl::test::sample_load();

    CHECK(load.header_bytes == from_hex(golden::header_file_hex));
}

TEST_CASE("the independently written header file reads back as the sample", "[golden]") {
    const auto header = decode_header_file(from_hex(golden::header_file_hex));

    REQUIRE(header.has_value());
    CHECK(*header == adl::test::sample_load().header);
    CHECK(header->load_part_number.view() == "ACM47-1234-5678");
    CHECK(header->load_type == LoadType{"OPS", 1});
    CHECK(header->target_hardware_ids == std::vector<std::string>{"ACME", "0A2B"});
    CHECK(header->data_files.size() == 2);
    CHECK(header->data_files[1].length == 5);
    CHECK(header->support_files.size() == 1);
    CHECK(header->user_defined_data == from_hex("DEADBEEF"));
}

TEST_CASE("the batch file is the one written independently", "[golden]") {
    const auto bytes = encode_batch_file(adl::test::sample_batch());

    REQUIRE(bytes.has_value());
    CHECK(*bytes == from_hex(golden::batch_file_hex));

    const auto decoded = decode_batch_file(from_hex(golden::batch_file_hex));
    REQUIRE(decoded.has_value());
    CHECK(*decoded == adl::test::sample_batch());
}

TEST_CASE("the media set's list files are the ones written independently", "[golden]") {
    const auto members = build_media_set(adl::test::sample_media_set_spec());
    REQUIRE(members.has_value());
    REQUIRE(members->size() == 2);

    const auto& first = (*members)[0];
    const auto& second = (*members)[1];

    CHECK(adl::test::member_file(first, "\\", "LOADS.LUM") ==
          from_hex(golden::loads_list_member_1_hex));
    CHECK(adl::test::member_file(second, "\\", "LOADS.LUM") ==
          from_hex(golden::loads_list_member_2_hex));
    CHECK(adl::test::member_file(first, "\\", "BATCHES.LUM") ==
          from_hex(golden::batches_list_member_1_hex));
    CHECK(adl::test::member_file(second, "\\", "BATCHES.LUM") ==
          from_hex(golden::batches_list_member_2_hex));
    CHECK(adl::test::member_file(first, "\\", "FILES.LUM") ==
          from_hex(golden::files_list_member_1_hex));
    CHECK(adl::test::member_file(second, "\\", "FILES.LUM") ==
          from_hex(golden::files_list_member_2_hex));
}

TEST_CASE("the independently written list files read back", "[golden]") {
    const auto loads = decode_loads_list(from_hex(golden::loads_list_member_2_hex));
    REQUIRE(loads.has_value());
    CHECK(loads->identity.media_set_part_number.view() == "ACM-SET-0001");
    CHECK(loads->identity.media_sequence_number == 2);
    CHECK(loads->identity.number_of_media_set_members == 2);
    REQUIRE(loads->loads.size() == 1);
    CHECK(loads->loads[0].header_file_name == "ACM47.LUH");
    CHECK(loads->loads[0].target_hardware_ids == std::vector<std::string>{"ACME", "0A2B"});

    const auto files = decode_files_list(from_hex(golden::files_list_member_1_hex));
    REQUIRE(files.has_value());
    CHECK(files->files.size() == 10);  // 2 + 2 list files, header, 2 data, 1 support, batch, stray
    REQUIRE(files->check_value.has_value());
    CHECK(files->check_value->type == CheckValueType::sha1);

    const auto batches = decode_batches_list(from_hex(golden::batches_list_member_1_hex));
    REQUIRE(batches.has_value());
    REQUIRE(batches->batches.size() == 1);
    CHECK(batches->batches[0].batch_file_name == "ACM47.LUB");
    CHECK(batches->batches[0].member_sequence_number == 2);
}
