// The media sets of Appendix F: loads that other loaders can use too. They are
// built here the way the appendix says, and read back through the whole of the
// rules in sections 3.2.4.1 and 3.2.4.2.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "adl/arinc665/media_set.hpp"
#include "adl/arinc665/naming.hpp"
#include "adl/samples/scenarios.hpp"
#include "bytes.hpp"

using namespace adl::arinc665;
using adl::test::bytes_of;

namespace {

bool lists_name(const FilesList& list, std::string_view name) {
    return std::any_of(list.files.begin(), list.files.end(),
                       [&](const FilesListEntry& entry) { return entry.file_name == name; });
}

}  // namespace

// F-1.2: an ARINC 615 part is built first and the ARINC 665 files are added to it.
// CONFIG.LDR and EXCONFIG.LDR go in the header file's support files; every other file
// is a data file. The 615 media has a directory for each disk (section 3.2.4.1.1).
TEST_CASE("a load that ARINC 615 loaders can load too (Appendix F-1)", "[compatibility]") {
    const auto set_spec = adl::samples::arinc615_media_set_spec();
    REQUIRE(set_spec.has_value());

    const auto members = build_media_set(*set_spec);
    REQUIRE(members.has_value());
    const auto set = read_media_set(*members);

    REQUIRE(set.has_value());
    REQUIRE(set->loads.size() == 1);
    const Load& load = set->loads[0];
    CHECK(load.header.data_files.size() == 2);
    REQUIRE(load.header.support_files.size() == 2);
    CHECK(load.header.support_files[0].file_name == "CONFIG.LDR");
    CHECK(load.header.support_files[1].file_name == "EXCONFIG.LDR");
    CHECK(load.data_files[0].bytes == bytes_of("DISK ONE DATA!"));
    CHECK(load.data_files[1].bytes == bytes_of("DISK TWO DATA!"));

    // The 615 file names break none of the naming recommendations.
    for (const Finding& finding : set->findings) {
        CHECK(finding.clause != "3.2.2");
        CHECK(finding.clause != "2.2.2");
    }
    // But the two data files do have one name, which the standard allows for 615 media
    // and still reports (2.2.1 asks for unique names).
    CHECK(std::any_of(set->findings.begin(), set->findings.end(),
                      [](const Finding& finding) { return finding.clause == "2.2.1"; }));
}

// F-2: a Boeing 777 (ARINC 629) load. The legacy header file is a support file of the
// 665 part, its data files are data files, and DISK.DIR and NON_LOAD.CRC are in
// FILES.LUM but in no header file.
TEST_CASE("a load that Boeing 777 loaders can load too (Appendix F-2)", "[compatibility]") {
    const auto set_spec = adl::samples::boeing777_media_set_spec();
    REQUIRE(set_spec.has_value());

    const auto members = build_media_set(*set_spec);
    REQUIRE(members.has_value());
    const auto set = read_media_set(*members);

    REQUIRE(set.has_value());
    CHECK(lists_name(set->files_list, "DISK.DIR"));
    CHECK(lists_name(set->files_list, "NON_LOAD.CRC"));
    const HeaderFile& header = set->loads[0].header;
    const auto names_in_header = [&header](std::string_view name) {
        return std::any_of(header.data_files.begin(), header.data_files.end(),
                           [&](const DataFile& file) { return file.file_name == name; }) ||
               std::any_of(header.support_files.begin(), header.support_files.end(),
                           [&](const SupportFile& file) { return file.file_name == name; });
    };
    CHECK_FALSE(names_in_header("DISK.DIR"));
    CHECK_FALSE(names_in_header("NON_LOAD.CRC"));
    CHECK(names_in_header("ACM12345.HDR"));

    // The legacy names (.HDR, .001) draw no finding about reserved extensions.
    for (const Finding& finding : set->findings) {
        CHECK(finding.clause != "3.2.2");
    }
}

// F-3: both at once.
TEST_CASE("a load for 615 loaders and Boeing 777 together (Appendix F-3)", "[compatibility]") {
    const auto set_spec = adl::samples::combined_media_set_spec();
    REQUIRE(set_spec.has_value());

    const auto members = build_media_set(*set_spec);
    REQUIRE(members.has_value());
    CHECK(read_media_set(*members).has_value());
}

TEST_CASE("each load of a media set has its own part root directory", "[compatibility]") {
    const auto make = [](const char* header, const char* part_number) {
        LoadSpec spec;
        spec.header_file_name = header;
        spec.part_number = PartNumber{part_number};
        spec.target_hardware_ids = {"ACME"};
        spec.data_files.push_back(
            {"DATA.LUP", PartNumber{}, bytes_of(part_number), CheckValueType::none});
        return build_load(spec).value();
    };
    MediaSetSpec set_spec;
    set_spec.media_set_part_number = PartNumber{"MANY-LOADS"};
    set_spec.loads.push_back(PlacedLoad{make("ACM70.LUH", "ACM70-0000-0001"), "", 1, {}, {}});
    set_spec.loads.push_back(PlacedLoad{make("ACM71.LUH", "ACM71-0000-0002"), "", 1, {}, {}});
    set_spec.loads.push_back(PlacedLoad{make("ACM72.LUH", "ACM72-0000-0003"), "", 1, {}, {}});

    const auto members = build_media_set(set_spec).value();
    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    REQUIRE(set->loads.size() == 3);
    for (const Load& load : set->loads) {
        // The one data file is the one whose content is this load's part number.
        CHECK(load.data_files[0].bytes ==
              bytes_of(std::string{load.header.load_part_number.view()}));
    }
}
