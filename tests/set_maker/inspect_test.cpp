// SPDX-License-Identifier: GPL-3.0-or-later
// inspect_set() and inspect_members(): what a media set holds, as facts.

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "adl/set_maker/inspect.hpp"
#include "adl/set_maker/make.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

namespace fs = std::filesystem;
using adl::arinc665::CheckValueType;
using namespace adl::set_maker;
using adl::test::set_maker::one_load_plan;
using adl::test::set_maker::tree_bytes;
using adl::test::set_maker::write_file;

class NoHooks final : public Hooks
{
};

// Two loads, two members, a batch, check values of every kind: a set with something to say.
SetPlan big_plan(const fs::path& sources)
{
    SetPlan plan = one_load_plan(sources, 2);
    plan.files_check = CheckValueType::sha1;
    plan.files_list_check = CheckValueType::crc32;
    plan.loads[0].check = CheckValueType::sha1;
    plan.loads[0].type = adl::arinc665::LoadType{"Operational software", 1};
    plan.loads[0].files[1].check = CheckValueType::crc8;
    PlannedLoad second;
    second.part_number = "ACM4C-0100-0200";
    second.header_file_name = "ACM4C.LUH";
    second.target_hardware_ids = {"ACME", "ACMEFMS"};
    // The same names as the first load's files, with other contents: told apart by their folders.
    second.files.push_back(PlannedFile{write_file(sources / "two", "app.bin", "SECOND LOAD APP!!"), "APP.LUP",
                                       FileKind::data, CheckValueType::crc16});
    second.files.push_back(PlannedFile{write_file(sources / "two", "config.ldr", "WXYZ"), "CONFIG.LDR",
                                       FileKind::support, CheckValueType::none});
    plan.loads.push_back(std::move(second));
    plan.batch = PlannedBatch{"ACM4D-0100-0300", "ACM.LUB", "for the unit", derive_batch_targets(plan.loads)};
    return plan;
}

// The bytes of the file that `entry` says, in the members read.
const adl::arinc665::MediaFile* file_at(const std::vector<adl::arinc665::MediaMember>& members, unsigned member,
                                        const std::string& pathname, const std::string& name)
{
    for (const auto& candidate : members)
    {
        if (candidate.sequence_number != member)
        {
            continue;
        }
        for (const auto& file : candidate.files)
        {
            if (file.pathname == pathname && file.name == name)
            {
                return &file;
            }
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("inspect: the facts of a set that was made are the facts of its plan", "[inspect]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = big_plan(sources.path());
    NoHooks hooks;
    REQUIRE(make_set(plan, Destination::exact(out.path() / "set"), hooks).has_value());

    const auto inspected = inspect_set(out.path() / "set");
    REQUIRE(inspected.has_value());
    const Inspection& set = *inspected;

    CHECK(set.part_number == "ACM-SET-0001");
    CHECK(set.members == 2);
    CHECK(set.members_present == std::vector<unsigned>{1, 2});
    CHECK(set.loads_list_version == "0xA004");
    CHECK(set.files_list_version == "0xA004");
    CHECK(set.batches_list_version == std::optional<std::string>{"0xA004"});
    CHECK(set.files_list_check_type == "crc32");
    REQUIRE(set.files_list_check_value.has_value());
    CHECK(set.files_list_check_value->size() == 8);

    REQUIRE(set.loads.size() == 2);
    const InspectedLoad& first = set.loads[0];
    CHECK(first.part_number == "ACM47-1234-5678");
    CHECK(first.header_file == "ACM.LUH");
    CHECK(first.header_member == 1);
    CHECK(first.header_pathname == "\\ACM4712345678\\");
    CHECK(first.format_version == "0x8004");
    REQUIRE(first.type.has_value());
    CHECK(first.type->description == "Operational software");
    CHECK(first.type->id == 1);
    CHECK_FALSE(first.is_download);
    CHECK(first.target_hardware_ids == std::vector<std::string>{"ACME"});
    CHECK(first.check_type == "sha1");
    REQUIRE(first.check_value.has_value());
    CHECK(first.check_value->size() == 40);
    CHECK(first.load_crc.size() == 8);
    REQUIRE(first.data_files.size() == 2);
    CHECK(first.data_files[0].name == "APP.LUP");
    CHECK(first.data_files[0].size == 17);
    CHECK(first.data_files[0].check_type == "crc32");
    CHECK(first.data_files[0].member == 1);
    CHECK(first.data_files[0].pathname == "\\ACM4712345678\\ACM4712345678\\");
    CHECK(first.data_files[1].name == "DATA.LUP");
    CHECK(first.data_files[1].size == 5);
    CHECK(first.data_files[1].check_type == "crc8");
    CHECK(first.data_files[1].check_value->size() == 4); // an 8-bit CRC takes a whole word
    CHECK(first.data_files[1].member == 2);
    REQUIRE(first.support_files.size() == 1);
    CHECK(first.support_files[0].name == "CONFIG.LDR");
    CHECK(first.support_files[0].check_type == "md5");
    CHECK(first.support_files[0].check_value->size() == 32);
    CHECK(first.support_files[0].pathname == "\\ACM4712345678\\");

    // The second load has files of the same names, in folders of its own, with their own contents.
    const InspectedLoad& second = set.loads[1];
    CHECK_FALSE(second.type.has_value());
    CHECK(second.check_type == "none");
    CHECK_FALSE(second.check_value.has_value());
    REQUIRE(second.data_files.size() == 1);
    CHECK(second.data_files[0].name == "APP.LUP");
    CHECK(second.data_files[0].size == 17);
    CHECK(second.data_files[0].pathname == "\\ACM4C01000200\\ACM4C01000200\\");
    CHECK(second.data_files[0].crc16 != first.data_files[0].crc16);
    CHECK(second.support_files[0].pathname == "\\ACM4C01000200\\");

    REQUIRE(set.batches.size() == 1);
    CHECK(set.batches[0].part_number == "ACM4D-0100-0300");
    CHECK(set.batches[0].file_name == "ACM.LUB");
    CHECK(set.batches[0].member == 1);
    CHECK(set.batches[0].pathname == "\\");
    CHECK(set.batches[0].comment == "for the unit");
    CHECK(set.batches[0].format_version == "0x9004");
    REQUIRE(set.batches[0].targets.size() == 2);
    CHECK(set.batches[0].targets[0].target_hardware_id_position == "ACME");
    REQUIRE(set.batches[0].targets[0].loads.size() == 2);
    CHECK(set.batches[0].targets[0].loads[1].load_part_number == "ACM4C-0100-0200");
    CHECK(set.batches[0].targets[0].loads[1].header_file_name == "ACM4C.LUH");
    CHECK(set.batches[0].targets[1].target_hardware_id_position == "ACMEFMS");

    // FILES.LUM: every file but itself, each with the size it has on its member, and a check value of the type
    // the plan asked for.
    const auto tree = tree_bytes(out.path() / "set");
    CHECK(set.files.size() + 2 == tree.size()); // the two FILES.LUM are not listed
    for (const InspectedFile& file : set.files)
    {
        std::string relative = "member_00" + std::to_string(file.member);
        for (const char c : file.pathname)
        {
            relative += c == '\\' ? '/' : c;
        }
        relative += file.name;
        INFO(relative);
        REQUIRE(tree.contains(relative));
        CHECK(file.size == tree.at(relative).size());
        CHECK(file.check_type == "sha1");
        CHECK(file.crc16.size() == 4);
    }
    CHECK(set.unlisted_files.empty());

    // The size of each member is every file on it, FILES.LUM included, so the members add up to the tree.
    REQUIRE(set.member_sizes.size() == 2);
    std::uint64_t all_bytes = 0;
    for (const auto& [name, bytes] : tree)
    {
        all_bytes += bytes.size();
    }
    std::uint64_t sum = 0;
    std::size_t files = 0;
    for (const MemberSize& size : set.member_sizes)
    {
        std::uint64_t on_member = 0;
        std::size_t count = 0;
        const std::string prefix = "member_00" + std::to_string(size.member) + "/";
        for (const auto& [name, bytes] : tree)
        {
            if (name.starts_with(prefix))
            {
                on_member += bytes.size();
                ++count;
            }
        }
        INFO(size.member);
        CHECK(size.size == on_member);
        CHECK(size.files == count);
        sum += size.size;
        files += size.files;
    }
    CHECK(set.member_sizes[0].member == 1);
    CHECK(set.member_sizes[1].member == 2);
    CHECK(sum == all_bytes);
    CHECK(files == tree.size());
    CHECK(set.findings.size() == 3); // the three data files that are not whole words
}

TEST_CASE("inspect: inspecting the tree and inspecting its members say the same", "[inspect]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    NoHooks hooks;
    REQUIRE(make_set(big_plan(sources.path()), Destination::exact(out.path() / "set"), hooks).has_value());

    auto members = adl::media_tree::read_media_set_tree(out.path() / "set");
    REQUIRE(members.has_value());
    const auto from_tree = inspect_set(out.path() / "set");
    const auto from_members = inspect_members(*members);
    REQUIRE(from_tree.has_value());
    REQUIRE(from_members.has_value());
    CHECK(from_tree->part_number == from_members->part_number);
    CHECK(from_tree->files.size() == from_members->files.size());
    CHECK(from_tree->findings == from_members->findings);
    CHECK(from_tree->loads.size() == from_members->loads.size());
}

TEST_CASE("inspect: the file a header file means is found as read_media_set finds it", "[inspect]")
{
    // The same rule is in libs/arinc665/src/media_set.cpp (section 3.2.4.2); the two have to give one answer.
    adl::test::TempDir sources;
    adl::test::TempDir out;
    NoHooks hooks;
    REQUIRE(make_set(big_plan(sources.path()), Destination::exact(out.path() / "set"), hooks).has_value());

    std::vector<std::vector<adl::arinc665::MediaMember>> sets;
    sets.push_back(*adl::media_tree::read_media_set_tree(out.path() / "set"));
    for (const adl::samples::Scenario& scenario : adl::samples::scenarios())
    {
        INFO(scenario.name);
        auto spec = scenario.make();
        REQUIRE(spec.has_value());
        auto members = adl::arinc665::build_media_set(*spec);
        REQUIRE(members.has_value());
        sets.push_back(std::move(*members));
    }

    std::size_t files_checked = 0;
    for (const auto& members : sets)
    {
        const auto read = adl::arinc665::read_media_set(members);
        REQUIRE(read.has_value());
        for (std::size_t i = 0; i < read->loads.size(); ++i)
        {
            const auto& load = read->loads[i];
            const auto& listed = read->loads_list.loads[i];
            const auto header =
                std::ranges::find_if(read->files_list.files,
                                     [&](const auto& entry)
                                     {
                                         return entry.file_name == listed.header_file_name &&
                                                entry.member_sequence_number == listed.member_sequence_number;
                                     });
            REQUIRE(header != read->files_list.files.end());
            for (std::size_t k = 0; k < load.header.data_files.size(); ++k)
            {
                const auto* entry = locate_listed_file(read->files_list, header->pathname, load.header.data_files[k]);
                REQUIRE(entry != nullptr);
                const auto* file = file_at(members, entry->member_sequence_number, entry->pathname, entry->file_name);
                REQUIRE(file != nullptr);
                CHECK(file->bytes == load.data_files[k].bytes);
                ++files_checked;
            }
            for (std::size_t k = 0; k < load.header.support_files.size(); ++k)
            {
                const auto* entry =
                    locate_listed_file(read->files_list, header->pathname, load.header.support_files[k]);
                REQUIRE(entry != nullptr);
                const auto* file = file_at(members, entry->member_sequence_number, entry->pathname, entry->file_name);
                REQUIRE(file != nullptr);
                CHECK(file->bytes == load.support_files[k].bytes);
                ++files_checked;
            }
        }
    }
    CHECK(files_checked >= 12);
}

TEST_CASE("inspect: the sample scenarios are read", "[inspect]")
{
    for (const adl::samples::Scenario& scenario : adl::samples::scenarios())
    {
        INFO(scenario.name);
        auto spec = scenario.make();
        REQUIRE(spec.has_value());
        auto members = adl::arinc665::build_media_set(*spec);
        REQUIRE(members.has_value());
        const auto read = adl::arinc665::read_media_set(*members);
        REQUIRE(read.has_value());
        const auto inspected = inspect_members(*members);
        REQUIRE(inspected.has_value());
        CHECK(inspected->loads.size() == read->loads.size());
        CHECK(inspected->batches.size() == read->batches.size());
        CHECK(inspected->findings.size() == read->findings.size());
        CHECK(inspected->members == read->loads_list.identity.number_of_media_set_members);
        for (const InspectedLoad& load : inspected->loads)
        {
            for (const InspectedLoadFile& file : load.data_files)
            {
                CHECK(file.member != 0); // every file of a load is somewhere
                CHECK_FALSE(file.pathname.empty());
            }
        }
    }
}

TEST_CASE("inspect: a file that is on a member and not in FILES.LUM is said", "[inspect]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    NoHooks hooks;
    REQUIRE(make_set(one_load_plan(sources.path()), Destination::exact(out.path() / "set"), hooks).has_value());
    auto members = adl::media_tree::read_media_set_tree(out.path() / "set");
    REQUIRE(members.has_value());
    const auto before = inspect_members(*members);
    REQUIRE(before.has_value());
    REQUIRE(before->member_sizes.size() == 1);
    members->at(0).files.push_back(
        adl::arinc665::MediaFile{"\\", "STRAY.TXT", {std::byte{1}, std::byte{2}, std::byte{3}}});

    const auto inspected = inspect_members(*members);
    REQUIRE(inspected.has_value());
    // The member's size counts the stray file, as it counts FILES.LUM, which is in neither list.
    REQUIRE(inspected->member_sizes.size() == 1);
    CHECK(inspected->member_sizes[0].files == before->member_sizes[0].files + 1);
    CHECK(inspected->member_sizes[0].size == before->member_sizes[0].size + 3);
    REQUIRE(inspected->unlisted_files.size() == 1);
    CHECK(inspected->unlisted_files[0].name == "STRAY.TXT");
    CHECK(inspected->unlisted_files[0].member == 1);
    CHECK(inspected->unlisted_files[0].size == 3);
    CHECK(inspected->findings.size() == 3); // the stray file, and the two data files that are not whole words
    CHECK(std::ranges::any_of(inspected->findings, [](const std::string& f) { return f.starts_with("3.2.1: "); }));
}

TEST_CASE("inspect: what is not a media set says why", "[inspect]")
{
    adl::test::TempDir out;
    adl::test::TempDir sources;
    NoHooks hooks;

    // A folder that is not there, a file, a folder with nothing in it.
    auto missing = inspect_set(out.path() / "missing");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().kind == InspectFailureKind::folder_unreadable);
    const fs::path file = write_file(out.path(), "file.txt", "x");
    CHECK(inspect_set(file).error().kind == InspectFailureKind::folder_unreadable);
    fs::create_directories(out.path() / "empty");
    auto empty = inspect_set(out.path() / "empty");
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error().kind == InspectFailureKind::set_invalid);
    CHECK_FALSE(empty.error().detail.empty());

    // A set with one byte changed does not verify.
    REQUIRE(make_set(one_load_plan(sources.path()), Destination::exact(out.path() / "set"), hooks).has_value());
    const fs::path data = out.path() / "set" / "member_001" / "ACM4712345678" / "DATA.LUP";
    {
        std::fstream stream(data, std::ios::in | std::ios::out | std::ios::binary);
        stream.put('J');
    }
    auto changed = inspect_set(out.path() / "set");
    REQUIRE_FALSE(changed.has_value());
    CHECK(changed.error().kind == InspectFailureKind::set_invalid);
    CHECK_FALSE(changed.error().detail.empty());
}
