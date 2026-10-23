// The widths of the fields in the formats are limits: 16-bit counts and string
// lengths, a 16-bit pointer to the next entry. These put a payload on each side of
// each limit.

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/header_file.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/arinc665/media_set.hpp"
#include "bytes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace adl::arinc665;
using adl::test::bytes_of;

namespace
{

HeaderFile small_header()
{
    HeaderFile header;
    header.load_part_number = PartNumber{"ACM47-1234-5678"};
    header.target_hardware_ids = {"ACME"};
    header.data_files.push_back(DataFile{});
    return header;
}

} // namespace

TEST_CASE("a text field can be 65535 characters and not one more", "[limits]")
{
    auto header = small_header();

    header.load_type = LoadType{std::string(65'535, 'D'), 1};
    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    const auto decoded = decode_header_file(*bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->load_type->description.size() == 65'535);

    header.load_type = LoadType{std::string(65'536, 'D'), 1};
    CHECK(encode_header_file(header).error() == Error::field_too_large);
}

TEST_CASE("a count can be 65535 and not one more", "[limits]")
{
    auto header = small_header();

    header.target_hardware_ids.assign(65'535, "ACME");
    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    const auto decoded = decode_header_file(*bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->target_hardware_ids.size() == 65'535);

    header.target_hardware_ids.push_back("ACME");
    CHECK(encode_header_file(header).error() == Error::field_too_large);
}

TEST_CASE("a pointer to the next entry reaches 65535 words and no further", "[limits]")
{
    // An entry is its pointer, its text and its numbers. With names and part numbers of the
    // greatest length an entry is too long for the 16-bit pointer that opens it.
    auto header = small_header();
    DataFile huge;
    huge.file_name = std::string(65'535, 'N');
    huge.part_number = PartNumber{std::string(65'535, 'P')};
    header.data_files = {huge, DataFile{}};

    CHECK(encode_header_file(header).error() == Error::field_too_large);

    // The last entry has no pointer, so the same entry can end the list.
    header.data_files = {DataFile{}, huge};
    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    const auto decoded = decode_header_file(*bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->data_files[1].file_name.size() == 65'535);
}

TEST_CASE("a check value fills at most 65531 bytes of its 16-bit length", "[limits]")
{
    auto header = small_header();

    // The length counts itself and the type, so the value can be 65535 - 4, rounded down
    // to whole words.
    header.load_check_value = CheckValue{static_cast<CheckValueType>(200), std::vector<std::byte>(65'530)};
    CHECK(encode_header_file(header).has_value());

    header.load_check_value = CheckValue{static_cast<CheckValueType>(200), std::vector<std::byte>(65'532)};
    CHECK(encode_header_file(header).error() == Error::field_too_large);
}

TEST_CASE("a load of many files builds, reads and verifies", "[limits]")
{
    LoadSpec spec;
    spec.header_file_name = "ACM80.LUH";
    spec.part_number = PartNumber{"ACM80-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    for (int i = 0; i < 1000; ++i)
    {
        const std::string name = "FILE" + std::to_string(i) + ".LUP";
        spec.data_files.push_back({name, PartNumber{}, bytes_of(name + name), CheckValueType::crc16});
    }
    for (int i = 0; i < 300; ++i)
    {
        spec.support_files.push_back(
            {"S" + std::to_string(i) + ".TXT", PartNumber{}, bytes_of("support"), CheckValueType::none});
    }
    spec.load_check_type = CheckValueType::sha1;

    const auto load = build_load(spec);

    REQUIRE(load.has_value());
    CHECK(load->header.data_files.size() == 1000);
    CHECK(load->header.support_files.size() == 300);
    CHECK(verify_load(*load).has_value());
    const auto decoded = decode_header_file(load->header_bytes);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == load->header);
}

TEST_CASE("a large file is covered by its CRC and check value", "[limits]")
{
    LoadSpec spec;
    spec.header_file_name = "ACM81.LUH";
    spec.part_number = PartNumber{"ACM81-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    std::vector<std::byte> big(3 * 1024 * 1024);
    for (std::size_t i = 0; i < big.size(); ++i)
    {
        big[i] = static_cast<std::byte>((i * 31U + i / 251U) & 0xFFU);
    }
    spec.data_files.push_back({"BIG.LUP", PartNumber{}, big, CheckValueType::sha1});
    spec.load_check_type = CheckValueType::crc32;

    const auto load = build_load(spec);

    REQUIRE(load.has_value());
    CHECK(load->header.data_files[0].length == big.size());
    CHECK(verify_load(*load).has_value());

    auto damaged = *load;
    damaged.data_files[0].bytes[big.size() - 1] ^= std::byte{0x80};
    CHECK(verify_load(damaged).error() == Error::bad_crc);
}

TEST_CASE("a media set of many loads and files builds and reads", "[limits]")
{
    MediaSetSpec set_spec;
    set_spec.media_set_part_number = PartNumber{"BIG-SET"};
    set_spec.number_of_members = 3;
    for (int i = 0; i < 40; ++i)
    {
        LoadSpec spec;
        spec.header_file_name = "ACM" + std::to_string(100 + i) + ".LUH";
        spec.part_number = PartNumber{"ACM47-" + std::to_string(1000 + i) + "-0001"};
        spec.target_hardware_ids = {"ACME", "0A2B"};
        for (int j = 0; j < 5; ++j)
        {
            const std::string name = "D" + std::to_string(j) + ".LUP";
            spec.data_files.push_back(
                {name, PartNumber{}, bytes_of(spec.header_file_name + name), CheckValueType::crc32});
        }
        PlacedLoad placed;
        placed.load = build_load(spec).value();
        placed.header_member = static_cast<std::uint16_t>(1 + i % 3);
        set_spec.loads.push_back(std::move(placed));
    }
    set_spec.file_check_type = CheckValueType::md5;

    const auto members = build_media_set(set_spec);
    REQUIRE(members.has_value());
    const auto set = read_media_set(*members);

    REQUIRE(set.has_value());
    CHECK(set->loads.size() == 40);
    CHECK(set->files_list.files.size() == 3 + 40 * 6);
}

TEST_CASE("a batch of many targets and loads reads back", "[limits]")
{
    BatchFile batch;
    batch.batch_part_number = PartNumber{"ACM4C-0100-0200"};
    batch.comment = std::string(5000, 'c');
    for (int i = 0; i < 200; ++i)
    {
        BatchTarget target;
        target.target_hardware_id_position = "ACME-" + std::to_string(i);
        for (int j = 0; j < 10; ++j)
        {
            target.loads.push_back({"ACM" + std::to_string(j) + ".LUH", PartNumber{"ACM47-1234-5678"}});
        }
        batch.targets.push_back(std::move(target));
    }

    const auto bytes = encode_batch_file(batch);
    REQUIRE(bytes.has_value());
    const auto decoded = decode_batch_file(*bytes);

    REQUIRE(decoded.has_value());
    CHECK(*decoded == batch);
}

TEST_CASE("a file list with a long path and name reads back", "[limits]")
{
    FilesList list;
    list.identity = MediaSetIdentity{PartNumber{"SET"}, 1, 1};
    FilesListEntry entry;
    entry.file_name = std::string(255, 'F'); // the longest file name section 2.2.2 allows
    entry.pathname = "\\" + std::string(200, 'A') + "\\" + std::string(200, 'B') + "\\";
    list.files.assign(50, entry);

    const auto decoded = decode_files_list(encode_files_list(list).value());

    REQUIRE(decoded.has_value());
    CHECK(*decoded == list);
}
