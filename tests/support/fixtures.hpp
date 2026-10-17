#pragma once

// The sample load, batch and media set the tests share. They are the scenario of
// tests/golden/make_golden.py, input for input: change one and the other has to
// change with it.

#include <string>
#include <utility>
#include <vector>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "bytes.hpp"

namespace adl::test {

using namespace adl::arinc665;

inline constexpr const char* sample_load_part_number = "ACM47-1234-5678";  // Appendix E's example
inline constexpr const char* sample_batch_part_number = "ACM4C-0100-0200";
inline constexpr const char* sample_media_set_part_number = "ACM-SET-0001";

inline std::vector<std::byte> sample_data_1() {
    std::vector<std::byte> bytes(16);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::byte>(i);
    }
    return bytes;
}

// Five bytes: three words, the last one half full.
inline std::vector<std::byte> sample_data_2() {
    return bytes_of("HELLO");
}

inline std::vector<std::byte> sample_support() {
    return bytes_of("ABCD");
}

// Every part of the header file is present: a load type, target hardware with
// positions, two data files (one with a check value, one of odd length), a
// support file with an MD5, user defined data and a Load Check Value.
inline LoadSpec sample_load_spec() {
    LoadSpec spec;
    spec.header_file_name = "ACM47.LUH";
    spec.part_number = PartNumber{sample_load_part_number};
    spec.load_type = LoadType{"OPS", 0x0001};
    spec.target_hardware_ids = {"ACME", "0A2B"};
    spec.target_hardware_ids_with_positions = {TargetHardware{"ACME", {"L", "R"}}};
    spec.data_files.push_back(
        {"APP.LUP", PartNumber{"ACM4F-0001-0001"}, sample_data_1(), CheckValueType::crc32});
    spec.data_files.push_back(
        {"DATA.LUP", PartNumber{"ACM4F-0002-0002"}, sample_data_2(), CheckValueType::none});
    spec.support_files.push_back(
        {"CONFIG.LDR", PartNumber{}, sample_support(), CheckValueType::md5});
    spec.user_defined_data = from_hex("DE AD BE EF");
    spec.load_check_type = CheckValueType::crc16;
    return spec;
}

inline Load sample_load() {
    return build_load(sample_load_spec()).value();
}

inline BatchFile sample_batch() {
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

// Two members: the header file and application on the first, the data file on the
// second (in a subdirectory), a batch on the second and a stray file on the first.
inline MediaSetSpec sample_media_set_spec() {
    MediaSetSpec spec;
    spec.media_set_part_number = PartNumber{sample_media_set_part_number};
    spec.number_of_members = 2;

    PlacedLoad placed;
    placed.load = sample_load();
    placed.header_member = 1;
    placed.data_placements = {FilePlacement{0, ""}, FilePlacement{2, "disk001\\"}};
    placed.support_placements = {FilePlacement{0, ""}};
    spec.loads.push_back(std::move(placed));

    spec.batches.push_back(PlacedBatch{"ACM47.LUB", sample_batch(), 2, "\\"});
    spec.other_files.push_back(PlacedFile{MediaFile{"\\", "NON_LOAD.CRC", bytes_of("XYZ")}, 1});

    spec.file_check_type = CheckValueType::crc32;
    spec.files_list_check_type = CheckValueType::sha1;
    return spec;
}

inline std::vector<std::byte> member_file(const MediaMember& member, const std::string& pathname,
                                          const std::string& name) {
    for (const MediaFile& file : member.files) {
        if (file.pathname == pathname && file.name == name) {
            return file.bytes;
        }
    }
    return {};
}

}  // namespace adl::test
