#include "adl/samples/scenarios.hpp"

#include "adl/arinc665/naming.hpp"

#include <array>
#include <string>
#include <utility>

namespace adl::samples
{

using namespace adl::arinc665;

namespace
{

std::vector<std::byte> bytes_of(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text)
    {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    return bytes;
}

// A media set of one load, built from a spec whose pieces are all constants here.
Result<MediaSetSpec> single_load_set(const char* set_part_number, const LoadSpec& load_spec, PlacedLoad placed)
{
    auto load = build_load(load_spec);
    if (!load)
    {
        return std::unexpected(load.error());
    }
    placed.load = std::move(*load);
    MediaSetSpec spec;
    spec.media_set_part_number = PartNumber{set_part_number};
    spec.loads.push_back(std::move(placed));
    return spec;
}

} // namespace

std::vector<std::byte> sample_data_1()
{
    std::vector<std::byte> bytes(16);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(i);
    }
    return bytes;
}

std::vector<std::byte> sample_data_2()
{
    return bytes_of("HELLO");
}

std::vector<std::byte> sample_support()
{
    return bytes_of("ABCD");
}

LoadSpec sample_load_spec()
{
    LoadSpec spec;
    spec.header_file_name = "ACM47.LUH";
    spec.part_number = PartNumber{sample_load_part_number};
    spec.load_type = LoadType{"OPS", 0x0001};
    spec.target_hardware_ids = {"ACME", "0A2B"};
    spec.target_hardware_ids_with_positions = {TargetHardware{"ACME", {"L", "R"}}};
    spec.data_files.push_back({"APP.LUP", PartNumber{"ACM4F-0001-0001"}, sample_data_1(), CheckValueType::crc32});
    spec.data_files.push_back({"DATA.LUP", PartNumber{"ACM4F-0002-0002"}, sample_data_2(), CheckValueType::none});
    spec.support_files.push_back({"CONFIG.LDR", PartNumber{}, sample_support(), CheckValueType::md5});
    spec.user_defined_data = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
    spec.load_check_type = CheckValueType::crc16;
    return spec;
}

Result<Load> sample_load()
{
    return build_load(sample_load_spec());
}

BatchFile sample_batch()
{
    BatchFile batch;
    batch.batch_part_number = PartNumber{sample_batch_part_number};
    batch.comment = "Test batch";
    batch.targets = {
        BatchTarget{"ACME-L", {BatchLoad{"ACM47.LUH", PartNumber{sample_load_part_number}}}},
        BatchTarget{"0A2B-1",
                    {BatchLoad{"ACM47.LUH", PartNumber{sample_load_part_number}},
                     BatchLoad{"ACM48.LUH", PartNumber{"ACM4F-5555-6666"}}}},
    };
    return batch;
}

Result<MediaSetSpec> sample_media_set_spec()
{
    PlacedLoad placed;
    placed.header_member = 1;
    placed.data_placements = {FilePlacement{0, ""}, FilePlacement{2, "disk001\\"}};
    placed.support_placements = {FilePlacement{0, ""}};
    auto spec = single_load_set(sample_media_set_part_number, sample_load_spec(), std::move(placed));
    if (!spec)
    {
        return spec;
    }
    spec->number_of_members = 2;
    spec->batches.push_back(PlacedBatch{"ACM47.LUB", sample_batch(), 2, "\\"});
    spec->other_files.push_back(PlacedFile{MediaFile{"\\", "NON_LOAD.CRC", bytes_of("XYZ")}, 1});
    spec->file_check_type = CheckValueType::crc32;
    spec->files_list_check_type = CheckValueType::sha1;
    return spec;
}

Result<MediaSetSpec> arinc615_media_set_spec()
{
    LoadSpec spec;
    spec.header_file_name = "ACM60.LUH";
    spec.part_number = PartNumber{"ACM60-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    // The two disks of a 615 part have files of the same names.
    spec.data_files.push_back({"LOAD.BIN", PartNumber{}, bytes_of("DISK ONE DATA!"), CheckValueType::none});
    spec.data_files.push_back({"LOAD.BIN", PartNumber{}, bytes_of("DISK TWO DATA!"), CheckValueType::none});
    spec.support_files.push_back(
        {std::string{config_ldr_file_name}, PartNumber{}, bytes_of("CONFIG ONE"), CheckValueType::none});
    spec.support_files.push_back(
        {std::string{exconfig_ldr_file_name}, PartNumber{}, bytes_of("EXCONFIG!!"), CheckValueType::none});

    PlacedLoad placed;
    placed.data_placements = {FilePlacement{0, "disk001\\"}, FilePlacement{0, "disk002\\"}};
    placed.support_placements = {FilePlacement{0, "disk001\\"}, FilePlacement{0, "disk001\\"}};
    return single_load_set("ACM-615-SET", spec, std::move(placed));
}

Result<MediaSetSpec> boeing777_media_set_spec()
{
    LoadSpec spec;
    spec.header_file_name = "ACM61.LUH";
    spec.part_number = PartNumber{"ACM61-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    spec.data_files.push_back({"ACM12345.001", PartNumber{}, bytes_of("LEGACY DATA 01"), CheckValueType::none});
    spec.data_files.push_back({"ACM12345.002", PartNumber{}, bytes_of("LEGACY DATA 02"), CheckValueType::none});
    spec.support_files.push_back({"ACM12345.HDR", PartNumber{}, bytes_of("D6-55562-5 HEADER"), CheckValueType::none});
    spec.support_files.push_back({"CONFIG.LDR", PartNumber{}, bytes_of("CONFIG"), CheckValueType::none});

    auto set = single_load_set("ACM-777-SET", spec, PlacedLoad{});
    if (!set)
    {
        return set;
    }
    set->other_files.push_back(PlacedFile{MediaFile{"\\", "DISK.DIR", bytes_of("DIRECTORY")}, 1});
    set->other_files.push_back(PlacedFile{MediaFile{"\\", "NON_LOAD.CRC", bytes_of("CRCS")}, 1});
    return set;
}

Result<MediaSetSpec> combined_media_set_spec()
{
    LoadSpec spec;
    spec.header_file_name = "ACM62.LUH";
    spec.part_number = PartNumber{"ACM62-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    spec.data_files.push_back({"ACM12345.001", PartNumber{}, bytes_of("DATA"), CheckValueType::none});
    spec.support_files.push_back({"ACM12345.HDR", PartNumber{}, bytes_of("HEADER"), CheckValueType::none});
    spec.support_files.push_back({"CONFIG.LDR", PartNumber{}, bytes_of("CONFIG"), CheckValueType::none});
    spec.support_files.push_back({"EXCONFIG.LDR", PartNumber{}, bytes_of("EXCONFIG"), CheckValueType::none});
    return single_load_set("ACM-ALL-SET", spec, PlacedLoad{});
}

std::span<const Scenario> scenarios()
{
    static constexpr std::array<Scenario, 4> all{{
        {"two-member", "two members, a load across both, a batch and a stray file; CRC-32 and SHA-1 check values",
         &sample_media_set_spec},
        {"arinc615", "an ARINC 615 part with a directory for each disk (Appendix F-1)", &arinc615_media_set_spec},
        {"boeing777", "a Boeing 777 load with DISK.DIR and NON_LOAD.CRC (Appendix F-2)", &boeing777_media_set_spec},
        {"arinc615-boeing777", "a load both kinds of loader can use (Appendix F-3)", &combined_media_set_spec},
    }};
    return all;
}

} // namespace adl::samples
