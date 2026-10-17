#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/load.hpp"
#include "bytes.hpp"
#include "edit.hpp"
#include "fixtures.hpp"

using namespace adl::arinc665;
using adl::test::bytes_of;
using adl::test::get_u32;
using adl::test::put_u16;

namespace {

bool mentions(const Findings& findings, std::string_view clause) {
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

}  // namespace

TEST_CASE("building a load records every file in the header", "[load]") {
    const auto load = adl::test::sample_load();
    const HeaderFile& header = load.header;

    REQUIRE(header.data_files.size() == 2);
    CHECK(header.data_files[0].file_name == "APP.LUP");
    CHECK(header.data_files[0].length == 16);
    CHECK(header.data_files[0].crc == crc_of<Crc16>(adl::test::sample_data_1()));
    REQUIRE(header.data_files[0].check_value.has_value());
    CHECK(header.data_files[0].check_value->type == CheckValueType::crc32);
    CHECK_FALSE(header.data_files[1].check_value.has_value());
    CHECK(header.data_files[1].length == 5);

    REQUIRE(header.support_files.size() == 1);
    CHECK(header.support_files[0].file_name == "CONFIG.LDR");
    REQUIRE(header.support_files[0].check_value.has_value());
    CHECK(header.support_files[0].check_value->type == CheckValueType::md5);
    CHECK(header.support_files[0].check_value->bytes.size() == 16);

    REQUIRE(header.load_check_value.has_value());
    CHECK(header.load_check_value->type == CheckValueType::crc16);
    CHECK(header.load_crc != 0);
}

TEST_CASE("a built load's header file decodes to the header it was built with", "[load]") {
    const auto load = adl::test::sample_load();

    const auto decoded = decode_header_file(load.header_bytes);

    REQUIRE(decoded.has_value());
    CHECK(*decoded == load.header);
}

TEST_CASE("the files of a built load are in the order the header lists them", "[load]") {
    const auto load = adl::test::sample_load();

    REQUIRE(load.data_files.size() == 2);
    CHECK(load.data_files[0].name == "APP.LUP");
    CHECK(load.data_files[0].bytes == adl::test::sample_data_1());
    CHECK(load.data_files[1].name == "DATA.LUP");
    REQUIRE(load.support_files.size() == 1);
    CHECK(load.support_files[0].name == "CONFIG.LDR");
    CHECK(load.header_file_name == "ACM47.LUH");
}

TEST_CASE("a built load verifies", "[load]") {
    CHECK(verify_load(adl::test::sample_load()).has_value());
}

TEST_CASE("a load verifies with each kind of load check value, or none", "[load]") {
    for (const auto type : {CheckValueType::none, CheckValueType::crc8, CheckValueType::crc16,
                            CheckValueType::crc32, CheckValueType::md5, CheckValueType::sha1}) {
        auto spec = adl::test::sample_load_spec();
        spec.load_check_type = type;

        const auto load = build_load(spec);

        REQUIRE(load.has_value());
        CHECK(verify_load(*load).has_value());
        CHECK(load->header.load_check_value.has_value() == (type != CheckValueType::none));
        if (type != CheckValueType::none) {
            CHECK(load->header.load_check_value->bytes.size() == check_value_length(type));
        }
    }
}

TEST_CASE("a load with the smallest header verifies", "[load]") {
    LoadSpec spec;
    spec.header_file_name = "ACM47.LUH";
    spec.part_number = PartNumber{"ACM47-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    spec.data_files.push_back({"A.LUP", PartNumber{}, bytes_of("DATA"), CheckValueType::none});

    const auto load = build_load(spec);

    REQUIRE(load.has_value());
    CHECK(verify_load(*load).has_value());
    CHECK_FALSE(load->header.load_check_value.has_value());
}

TEST_CASE("an empty file is a file", "[load]") {
    auto spec = adl::test::sample_load_spec();
    spec.data_files[1].content.clear();

    const auto load = build_load(spec);

    REQUIRE(load.has_value());
    CHECK(load->header.data_files[1].length == 0);
    CHECK(verify_load(*load).has_value());
}

TEST_CASE("a load is not built from what it cannot be built from", "[load]") {
    SECTION("no data file") {
        auto spec = adl::test::sample_load_spec();
        spec.data_files.clear();
        CHECK(build_load(spec).error() == Error::invalid_value);
    }
    SECTION("user data of an odd number of bytes") {
        auto spec = adl::test::sample_load_spec();
        spec.user_defined_data.push_back(std::byte{1});
        CHECK(build_load(spec).error() == Error::invalid_value);
    }
    SECTION("a check value type that cannot be calculated") {
        auto spec = adl::test::sample_load_spec();
        spec.data_files[0].check_type = static_cast<CheckValueType>(99);
        CHECK(build_load(spec).error() == Error::unsupported_check_type);
    }
    SECTION("a load check value type that cannot be calculated") {
        auto spec = adl::test::sample_load_spec();
        spec.load_check_type = static_cast<CheckValueType>(99);
        CHECK(build_load(spec).error() == Error::unsupported_check_type);
    }
}

TEST_CASE("a download part is built with the download flag set", "[load]") {
    auto spec = adl::test::sample_load_spec();
    spec.part_flags.is_download = true;

    const auto load = build_load(spec);

    REQUIRE(load.has_value());
    CHECK(load->header.part_flags.is_download);
    CHECK(verify_load(*load).has_value());
}

TEST_CASE("the load check value and the load CRC depend on the whole load", "[load]") {
    const auto original = adl::test::sample_load();

    auto spec = adl::test::sample_load_spec();
    spec.data_files[1].content = bytes_of("HELLO");
    spec.data_files[1].content[0] = std::byte{'J'};  // one byte of one data file
    const auto changed = build_load(spec).value();

    CHECK(changed.header.load_crc != original.header.load_crc);
    CHECK(changed.header.load_check_value != original.header.load_check_value);
}

TEST_CASE("a load that has been damaged does not verify", "[load]") {
    const auto good = adl::test::sample_load();

    SECTION("a byte of a data file") {
        auto load = good;
        load.data_files[0].bytes[3] ^= std::byte{0x01};
        CHECK(verify_load(load).error() == Error::bad_crc);
    }
    SECTION("a byte of a support file") {
        auto load = good;
        load.support_files[0].bytes[0] ^= std::byte{0x01};
        CHECK(verify_load(load).error() == Error::bad_crc);
    }
    SECTION("a file of another length") {
        auto load = good;
        load.data_files[1].bytes.push_back(std::byte{0});
        CHECK(verify_load(load).error() == Error::bad_length);
    }
    SECTION("a file of another name") {
        auto load = good;
        load.data_files[0].name = "OTHER.LUP";
        CHECK(verify_load(load).error() == Error::inconsistent);
    }
    SECTION("a file missing") {
        auto load = good;
        load.data_files.pop_back();
        CHECK(verify_load(load).error() == Error::inconsistent);
    }
    SECTION("an extra file") {
        auto load = good;
        load.support_files.push_back({"EXTRA.TXT", {}});
        CHECK(verify_load(load).error() == Error::inconsistent);
    }
    SECTION("the header file") {
        auto load = good;
        load.header_bytes[60] ^= std::byte{0x01};
        CHECK(verify_load(load).error() == Error::bad_crc);
    }
    SECTION("a file's check value in the header, with the header's own CRC made right") {
        auto load = good;
        // The MD5 of the support file is in the support file's entry: find it by its
        // length word (4 + 16 = 20) and its type (4).
        const std::size_t support_entry = get_u32(load.header_bytes, 10) + 1;
        std::size_t at = 0;
        for (std::size_t word = support_entry; word + 1 < load.header_bytes.size() / 2; ++word) {
            if (adl::test::get_u16(load.header_bytes, word) == 20 &&
                adl::test::get_u16(load.header_bytes, word + 1) == 4) {
                at = word + 2;
                break;
            }
        }
        REQUIRE(at != 0);
        load.header_bytes[2 * at] ^= std::byte{0x01};
        adl::test::refresh_header_crc(load.header_bytes);
        CHECK(verify_load(load).error() == Error::bad_check_value);
    }
    SECTION("the load check value, with the header's own CRC made right") {
        auto load = good;
        const std::size_t check = get_u32(load.header_bytes, 18);
        put_u16(load.header_bytes, check + 2,
                adl::test::get_u16(load.header_bytes, check + 2) ^ 0x0001U);
        adl::test::refresh_header_crc(load.header_bytes);
        CHECK(verify_load(load).error() == Error::bad_check_value);
    }
    SECTION("the load CRC") {
        auto load = good;
        load.header_bytes[load.header_bytes.size() - 1] ^= std::byte{0x01};
        CHECK(verify_load(load).error() == Error::bad_crc);
    }
}

TEST_CASE("a file with a check value of a type this library does not know is not checked",
          "[load]") {
    auto load = adl::test::sample_load();
    // Turn the 32-bit CRC of the first data file into a check value of type 77. The value
    // is not the file's, and the file still verifies, since there is nothing to check it by.
    HeaderFile header = load.header;
    header.data_files[0].check_value =
        CheckValue{static_cast<CheckValueType>(77), {std::byte{1}, std::byte{2}}};
    header.load_check_value.reset();
    load.header = header;
    load.header_bytes = encode_header_file(header).value();
    // Put the Load CRC right so only the unknown type is under test.
    Crc32 crc;
    crc.update(std::span<const std::byte>{load.header_bytes}.first(load.header_bytes.size() - 4));
    for (const auto& file : load.data_files) {
        crc.update(file.bytes);
    }
    for (const auto& file : load.support_files) {
        crc.update(file.bytes);
    }
    header.load_crc = crc.value();
    load.header_bytes = encode_header_file(header).value();

    CHECK(verify_load(load).has_value());
}

TEST_CASE("a load's findings come from its header and its header file name", "[load]") {
    auto load = adl::test::sample_load();
    load.header_file_name = "ACM47.LUP";

    const auto findings = check_load(load);

    CHECK(mentions(findings, "2.2.2.1"));  // the name
    CHECK(mentions(findings, "2.2.3.2"));  // the odd-length data file
}
