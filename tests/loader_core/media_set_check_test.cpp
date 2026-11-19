// SPDX-License-Identifier: GPL-3.0-or-later
// The check of a media set on a disk: the sample set written as a tree and read back, with what the
// report says of it, and the ways it can fail to read.

#include "adl/arinc665/media_set.hpp"
#include "adl/loader_core/media_set_check.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace adl::loader_core;
using adl::test::TempDir;

namespace
{

// The sample media set, with `part_number` when it is given, written to `directory` as a tree. Returns
// the findings adl::arinc665 makes of the set in memory, as the report should word them.
std::vector<std::string> write_sample(const std::filesystem::path& directory, const char* part_number = nullptr)
{
    auto spec = adl::samples::sample_media_set_spec().value();
    if (part_number != nullptr)
    {
        spec.media_set_part_number = adl::arinc665::PartNumber{part_number};
    }
    const auto members = adl::arinc665::build_media_set(spec);
    REQUIRE(members.has_value());
    REQUIRE(adl::media_tree::write_media_set_tree(directory, *members).has_value());
    const auto set = adl::arinc665::read_media_set(*members);
    REQUIRE(set.has_value());
    std::vector<std::string> findings;
    for (const auto& finding : set->findings)
    {
        findings.push_back(finding.clause + ": " + finding.message);
    }
    return findings;
}

} // namespace

TEST_CASE("a media set on disk is read and reported", "[loader_core][media_set_check]")
{
    TempDir dir;
    const auto findings = write_sample(dir.path());
    const auto spec = adl::samples::sample_media_set_spec().value();

    std::shared_ptr<const MediaSetIndex> index;
    const MediaSetReport report = check_media_set(dir.path(), &index);
    REQUIRE(report.error.empty());
    CHECK(report.part_number == adl::samples::sample_media_set_part_number);
    CHECK(report.members == 2);
    REQUIRE(report.loads.size() == 1);
    CHECK(report.loads[0].part_number == adl::samples::sample_load_part_number);
    CHECK(report.loads[0].header_file == spec.loads[0].load.header_file_name);
    CHECK(report.loads[0].data_files == spec.loads[0].load.data_files.size());
    CHECK_FALSE(report.loads[0].target_hardware_ids.empty());
    CHECK(report.findings == findings);

    // The files FILES.LUM lists, each with its CRC as four hexadecimal digits, and the index of them
    // that an upload reads them through, with the fingerprint of each as it was read.
    REQUIRE_FALSE(report.files.empty());
    REQUIRE(index != nullptr);
    REQUIRE(index->entries().size() == report.files.size());
    for (std::size_t i = 0; i < report.files.size(); ++i)
    {
        const FileReport& file = report.files[i];
        CHECK(file.name == index->entries()[i].name);
        CHECK(file.member == index->entries()[i].member);
        REQUIRE(file.crc16.size() == 4);
        CHECK(std::stoul(file.crc16, nullptr, 16) == index->entries()[i].crc16);
        for (const char c : file.crc16)
        {
            CHECK((std::isdigit(static_cast<unsigned char>(c)) != 0 || (c >= 'A' && c <= 'F')));
        }
        CHECK(index->entries()[i].fingerprint.has_value());
    }

    // Without an index to fill, the set is read all the same.
    CHECK(check_media_set(dir.path()).error.empty());
}

TEST_CASE("a departure from a recommendation is a finding, not an error", "[loader_core][media_set_check]")
{
    TempDir plain;
    const auto plain_findings = write_sample(plain.path());
    TempDir dir;
    // The standard recommends against an I in a part number.
    const auto findings = write_sample(dir.path(), "ACM-SET-000I");
    REQUIRE(findings.size() > plain_findings.size());

    const MediaSetReport report = check_media_set(dir.path());
    CHECK(report.error.empty());
    CHECK(report.findings == findings);
    CHECK(report.part_number == "ACM-SET-000I");
}

TEST_CASE("a media set that cannot be read says why and leaves no index", "[loader_core][media_set_check]")
{
    TempDir dir;
    write_sample(dir.path());
    std::shared_ptr<const MediaSetIndex> index;
    REQUIRE(check_media_set(dir.path(), &index).error.empty());
    REQUIRE(index != nullptr);

    SECTION("a folder that is not there")
    {
        const MediaSetReport report = check_media_set(dir.path() / "nowhere", &index);
        REQUIRE_FALSE(report.error.empty());
        CHECK(report.error.find("nowhere") != std::string::npos); // names the path as well
        CHECK(report.loads.empty());
        CHECK(report.files.empty());
        CHECK(index == nullptr); // the index of the earlier read is not kept
    }

    SECTION("a file that no longer matches its check value")
    {
        {
            std::fstream file(dir.path() / "member_001" / "LOADS.LUM", std::ios::in | std::ios::out | std::ios::binary);
            REQUIRE(file.is_open());
            file.seekp(12);
            file.put('\x5A');
        }
        const MediaSetReport report = check_media_set(dir.path(), &index);
        CHECK_FALSE(report.error.empty());
        CHECK(report.loads.empty());
        CHECK(index == nullptr);
    }
}
