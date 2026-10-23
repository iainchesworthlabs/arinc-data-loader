#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/media_set.hpp"
#include "bytes.hpp"
#include "edit.hpp"
#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace adl::arinc665;
using adl::test::bytes_of;
using adl::test::member_file;

namespace
{

std::vector<MediaMember> sample_members()
{
    return build_media_set(adl::test::sample_media_set_spec()).value();
}

MediaFile* find(MediaMember& member, const std::string& pathname, const std::string& name)
{
    for (MediaFile& file : member.files)
    {
        if (file.pathname == pathname && file.name == name)
        {
            return &file;
        }
    }
    return nullptr;
}

const std::string root = "\\ACM4712345678\\";

} // namespace

TEST_CASE("every member has the three lists in its root", "[media-set]")
{
    const auto members = sample_members();

    REQUIRE(members.size() == 2);
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        CHECK(members[i].sequence_number == i + 1);
        CHECK_FALSE(member_file(members[i], "\\", "LOADS.LUM").empty());
        CHECK_FALSE(member_file(members[i], "\\", "FILES.LUM").empty());
        CHECK_FALSE(member_file(members[i], "\\", "BATCHES.LUM").empty());
    }
}

TEST_CASE("each file is on the member it was placed on, in the directory it was placed in", "[media-set]")
{
    const auto members = sample_members();

    CHECK_FALSE(member_file(members[0], root, "ACM47.LUH").empty());
    CHECK_FALSE(member_file(members[0], root, "APP.LUP").empty());
    CHECK_FALSE(member_file(members[0], root, "CONFIG.LDR").empty());
    CHECK_FALSE(member_file(members[0], "\\", "NON_LOAD.CRC").empty());
    CHECK_FALSE(member_file(members[1], root + "disk001\\", "DATA.LUP").empty());
    CHECK_FALSE(member_file(members[1], "\\", "ACM47.LUB").empty());

    CHECK(member_file(members[1], root, "ACM47.LUH").empty());
    CHECK(member_file(members[0], root + "disk001\\", "DATA.LUP").empty());
}

TEST_CASE("a set without a batch has no BATCHES.LUM", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.batches.clear();

    const auto members = build_media_set(spec);

    REQUIRE(members.has_value());
    CHECK(member_file((*members)[0], "\\", "BATCHES.LUM").empty());
    CHECK(read_media_set(*members).has_value());
}

TEST_CASE("the list files differ between members only in the media sequence number and the CRC", "[media-set]")
{
    const auto members = sample_members();

    for (const char* name : {"LOADS.LUM", "FILES.LUM", "BATCHES.LUM"})
    {
        CAPTURE(name);
        const auto first = member_file(members[0], "\\", name);
        const auto second = member_file(members[1], "\\", name);
        REQUIRE(first.size() == second.size());
        std::size_t differing_words = 0;
        for (std::size_t word = 0; word < first.size() / 2; ++word)
        {
            if (adl::test::get_u16(first, word) != adl::test::get_u16(second, word))
            {
                ++differing_words;
            }
        }
        // The word with X and Y, the CRC, and for FILES.LUM the check value (which covers X).
        CHECK(differing_words >= 2);
        // X and Y share the word after the media set part number (Pointer to Media Set PN Length is
        // in words 4 and 5).
        const std::size_t media_set = adl::test::get_u32(first, 4);
        const std::size_t x_and_y = media_set + 1 + (adl::test::get_u16(first, media_set) + 1U) / 2U;
        CHECK(adl::test::get_u16(first, x_and_y) == 0x0102);
        CHECK(adl::test::get_u16(second, x_and_y) == 0x0202);
    }
}

TEST_CASE("FILES.LUM lists every file but itself, with the member each is on", "[media-set]")
{
    const auto members = sample_members();
    const auto files = decode_files_list(member_file(members[0], "\\", "FILES.LUM")).value();

    std::vector<std::string> names;
    for (const FilesListEntry& entry : files.files)
    {
        names.push_back(entry.file_name);
    }
    CHECK(names == std::vector<std::string>{"LOADS.LUM", "LOADS.LUM", "BATCHES.LUM", "BATCHES.LUM", "ACM47.LUH",
                                            "APP.LUP", "DATA.LUP", "CONFIG.LDR", "ACM47.LUB", "NON_LOAD.CRC"});
    CHECK(files.files[0].member_sequence_number == 1);
    CHECK(files.files[1].member_sequence_number == 2);
    CHECK(files.files[6].file_name == "DATA.LUP");
    CHECK(files.files[6].member_sequence_number == 2);
    CHECK(files.files[6].pathname == root + "disk001\\");
}

TEST_CASE("FILES.LUM records each file's CRC and check value", "[media-set]")
{
    const auto members = sample_members();
    const auto files = decode_files_list(member_file(members[0], "\\", "FILES.LUM")).value();

    for (const FilesListEntry& entry : files.files)
    {
        const auto& member = members[entry.member_sequence_number - 1U];
        const auto bytes = member_file(member, entry.pathname, entry.file_name);
        REQUIRE_FALSE(bytes.empty());
        CHECK(entry.crc == crc_of<Crc16>(bytes));
        REQUIRE(entry.check_value.has_value());
        CHECK(entry.check_value->type == CheckValueType::crc32);
        CHECK(*entry.check_value == compute_check_value(CheckValueType::crc32, bytes).value());
    }
}

TEST_CASE("the CRC FILES.LUM gives LOADS.LUM and BATCHES.LUM is zero", "[media-set]")
{
    // Section 3.2.3.2.20: each ends in its own CRC.
    const auto members = sample_members();
    const auto files = decode_files_list(member_file(members[0], "\\", "FILES.LUM")).value();

    for (const FilesListEntry& entry : files.files)
    {
        if (entry.file_name == "LOADS.LUM" || entry.file_name == "BATCHES.LUM")
        {
            CHECK(entry.crc == 0);
        }
    }
}

TEST_CASE("FILES.LUM has a check value of its own over all but the check value and CRC", "[media-set]")
{
    const auto members = sample_members();
    const auto bytes = member_file(members[0], "\\", "FILES.LUM");
    const auto files = decode_files_list(bytes).value();

    REQUIRE(files.check_value.has_value());
    CHECK(files.check_value->type == CheckValueType::sha1);
    const auto offset = files_list_check_value_offset(bytes).value();
    CHECK(*files.check_value ==
          compute_check_value(CheckValueType::sha1, std::span<const std::byte>{bytes}.first(offset)).value());
}

TEST_CASE("a media set reads back as it was built", "[media-set]")
{
    const auto members = sample_members();

    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    CHECK(set->loads_list.identity.media_set_part_number.view() == "ACM-SET-0001");
    CHECK(set->loads_list.identity.number_of_media_set_members == 2);
    REQUIRE(set->loads.size() == 1);
    const Load original = adl::test::sample_load();
    CHECK(set->loads[0].header == original.header);
    CHECK(set->loads[0].header_bytes == original.header_bytes);
    CHECK(set->loads[0].data_files == original.data_files);
    CHECK(set->loads[0].support_files == original.support_files);
    CHECK(set->loads[0].header_file_name == "ACM47.LUH");
    REQUIRE(set->batches.size() == 1);
    CHECK(set->batches[0].file_name == "ACM47.LUB");
    CHECK(set->batches[0].batch == adl::test::sample_batch());
}

TEST_CASE("members can be given in any order", "[media-set]")
{
    auto members = sample_members();
    std::swap(members[0], members[1]);

    CHECK(read_media_set(members).has_value());
}

TEST_CASE("a set with one member is a set", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.number_of_members = 1;
    spec.loads[0].data_placements.clear(); // everything with the header
    spec.batches[0].member = 1;

    const auto members = build_media_set(spec);

    REQUIRE(members.has_value());
    REQUIRE(members->size() == 1);
    const auto set = read_media_set(*members);
    REQUIRE(set.has_value());
    CHECK(set->loads[0].data_files == adl::test::sample_load().data_files);
}

TEST_CASE("a set with no loads, and one with many members", "[media-set]")
{
    MediaSetSpec spec;
    spec.media_set_part_number = PartNumber{"EMPTY-SET"};

    SECTION("nothing on it but the lists")
    {
        const auto members = build_media_set(spec);
        REQUIRE(members.has_value());
        const auto set = read_media_set(*members);
        REQUIRE(set.has_value());
        CHECK(set->loads.empty());
        CHECK(set->files_list.files.size() == 1); // LOADS.LUM
    }
    SECTION("as many members as the standard allows")
    {
        spec.number_of_members = 255;
        const auto members = build_media_set(spec);
        REQUIRE(members.has_value());
        CHECK(members->size() == 255);
        CHECK(read_media_set(*members).has_value());
    }
}

TEST_CASE("a media set that cannot be built is refused", "[media-set]")
{
    SECTION("no members")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.number_of_members = 0;
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("a load on a member the set does not have")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.loads[0].header_member = 3;
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("a data file on a member the set does not have")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.loads[0].data_placements[1].member = 3;
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("a batch or a file on a member the set does not have")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.batches[0].member = 0;
        CHECK(build_media_set(spec).error() == Error::invalid_value);

        auto other = adl::test::sample_media_set_spec();
        other.other_files[0].member = 9;
        CHECK(build_media_set(other).error() == Error::invalid_value);
    }
    SECTION("placements that do not match the load's files")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.loads[0].data_placements.pop_back();
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("two files of one name in one directory of a member")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.other_files.push_back(PlacedFile{MediaFile{"\\", "NON_LOAD.CRC", bytes_of("ABC")}, 1});
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("a file that takes a name of the lists")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.other_files.push_back(PlacedFile{MediaFile{"\\", "LOADS.LUM", bytes_of("ABC")}, 1});
        CHECK(build_media_set(spec).error() == Error::invalid_value);
    }
    SECTION("a check value type that cannot be calculated")
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.file_check_type = static_cast<CheckValueType>(99);
        CHECK(build_media_set(spec).error() == Error::unsupported_check_type);

        auto other = adl::test::sample_media_set_spec();
        other.files_list_check_type = static_cast<CheckValueType>(99);
        CHECK(build_media_set(other).error() == Error::unsupported_check_type);
    }
}

TEST_CASE("the part root directory is the part number without its delimiters unless given", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.loads[0].part_root_directory = "\\MYLOAD\\";

    const auto members = build_media_set(spec).value();

    CHECK_FALSE(member_file(members[0], "\\MYLOAD\\", "ACM47.LUH").empty());
    CHECK_FALSE(member_file(members[1], "\\MYLOAD\\disk001\\", "DATA.LUP").empty());
    CHECK(read_media_set(members).has_value());
}

TEST_CASE("a set with no check values at all reads back", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.file_check_type = CheckValueType::none;
    spec.files_list_check_type = CheckValueType::none;

    const auto members = build_media_set(spec).value();
    const auto files = decode_files_list(member_file(members[0], "\\", "FILES.LUM")).value();

    CHECK_FALSE(files.check_value.has_value());
    CHECK_FALSE(files.files[0].check_value.has_value());
    CHECK(read_media_set(members).has_value());
}

TEST_CASE("each kind of check value can be used for the files and FILES.LUM", "[media-set]")
{
    for (const auto type : {CheckValueType::crc8, CheckValueType::crc16, CheckValueType::crc32, CheckValueType::md5,
                            CheckValueType::sha1})
    {
        auto spec = adl::test::sample_media_set_spec();
        spec.file_check_type = type;
        spec.files_list_check_type = type;

        const auto members = build_media_set(spec);

        REQUIRE(members.has_value());
        CHECK(read_media_set(*members).has_value());
    }
}

// ---- Reading some of the members ----

TEST_CASE("a load that needs a member that was not given cannot be read", "[media-set]")
{
    const auto members = sample_members();

    SECTION("the data file is on the second")
    {
        const auto first_only = std::span<const MediaMember>{members}.first(1);
        CHECK(read_media_set(first_only).error() == Error::file_not_found);
    }
    SECTION("the header file is on the first")
    {
        const auto second_only = std::span<const MediaMember>{members}.subspan(1);
        CHECK(read_media_set(second_only).error() == Error::file_not_found);
    }
}

TEST_CASE("a set whose loads are all on the members given can be read from those", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.loads[0].data_placements.clear();
    spec.batches[0].member = 1;
    const auto members = build_media_set(spec).value();

    const auto first_only = std::span<const MediaMember>{members}.first(1);
    const auto set = read_media_set(first_only);

    REQUIRE(set.has_value());
    CHECK(set->loads.size() == 1);
}

// ---- Damage ----

TEST_CASE("a media set that has been damaged is not read", "[media-set]")
{
    SECTION("nothing")
    {
        CHECK(read_media_set({}).error() == Error::invalid_value);
    }
    SECTION("a data file with a byte changed")
    {
        auto members = sample_members();
        find(members[0], root, "APP.LUP")->bytes[2] ^= std::byte{0x01};
        CHECK(read_media_set(members).error() == Error::bad_crc);
    }
    SECTION("a check value in FILES.LUM that is not the file's, though its CRC is")
    {
        auto members = sample_members();
        for (auto& member : members)
        {
            auto* file = find(member, "\\", "FILES.LUM");
            auto files = decode_files_list(file->bytes).value();
            for (auto& entry : files.files)
            {
                if (entry.file_name == "DATA.LUP")
                {
                    entry.check_value =
                        CheckValue{CheckValueType::crc32, {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}}};
                }
            }
            files.check_value.reset();
            file->bytes = encode_files_list(files).value();
        }
        CHECK(read_media_set(members).error() == Error::bad_check_value);
    }
    SECTION("a file missing")
    {
        auto members = sample_members();
        auto& files = members[0].files;
        files.erase(
            std::remove_if(files.begin(), files.end(), [](const MediaFile& file) { return file.name == "APP.LUP"; }),
            files.end());
        CHECK(read_media_set(members).error() == Error::file_not_found);
    }
    SECTION("LOADS.LUM missing")
    {
        auto members = sample_members();
        auto& files = members[1].files;
        files.erase(
            std::remove_if(files.begin(), files.end(), [](const MediaFile& file) { return file.name == "LOADS.LUM"; }),
            files.end());
        CHECK(read_media_set(members).error() == Error::file_not_found);
    }
    SECTION("FILES.LUM missing")
    {
        auto members = sample_members();
        auto& files = members[0].files;
        files.erase(
            std::remove_if(files.begin(), files.end(), [](const MediaFile& file) { return file.name == "FILES.LUM"; }),
            files.end());
        CHECK(read_media_set(members).error() == Error::file_not_found);
    }
    SECTION("a list file with a byte changed")
    {
        auto members = sample_members();
        find(members[0], "\\", "LOADS.LUM")->bytes[24] ^= std::byte{0x01};
        CHECK(read_media_set(members).error() == Error::bad_crc);
    }
    SECTION("FILES.LUM with its check value wrong")
    {
        auto members = sample_members();
        auto* file = find(members[0], "\\", "FILES.LUM");
        auto list = decode_files_list(file->bytes).value();
        list.check_value->bytes[0] ^= std::byte{0x01};
        file->bytes = encode_files_list(list).value();
        CHECK(read_media_set(members).error() == Error::bad_check_value);
    }
    SECTION("a header file with a byte changed")
    {
        auto members = sample_members();
        find(members[0], root, "ACM47.LUH")->bytes[50] ^= std::byte{0x01};
        CHECK(read_media_set(members).error() == Error::bad_crc);
    }
    SECTION("two members with the same number")
    {
        auto members = sample_members();
        members[1] = members[0];
        CHECK(read_media_set(members).error() == Error::inconsistent);
    }
    SECTION("a member whose lists say it is another")
    {
        auto members = sample_members();
        members[1].sequence_number = 1;
        members[0].sequence_number = 2;
        CHECK(read_media_set(members).error() == Error::inconsistent);
    }
}

TEST_CASE("members of different sets are not read as one", "[media-set]")
{
    auto members = sample_members();
    auto other_spec = adl::test::sample_media_set_spec();
    other_spec.media_set_part_number = PartNumber{"OTHER-SET"};
    const auto other = build_media_set(other_spec).value();

    SECTION("the second member from another set")
    {
        members[1] = other[1];
        CHECK(read_media_set(members).error() == Error::inconsistent);
    }
    SECTION("the second member from the same set with a load missing")
    {
        auto fewer = adl::test::sample_media_set_spec();
        fewer.number_of_members = 2;
        fewer.loads.clear();
        fewer.other_files.clear();
        fewer.batches.clear();
        members[1] = build_media_set(fewer).value()[1];
        CHECK(read_media_set(members).error() == Error::inconsistent);
    }
}

TEST_CASE("a load whose header file disagrees with LOADS.LUM is not read", "[media-set]")
{
    // LOADS.LUM says the load has other target hardware than its header file does.
    auto spec = adl::test::sample_media_set_spec();
    auto lying = build_media_set(spec).value();
    for (auto& member : lying)
    {
        auto* loads_file = find(member, "\\", "LOADS.LUM");
        auto list = decode_loads_list(loads_file->bytes).value();
        list.loads[0].target_hardware_ids = {"ZZZZ"};
        loads_file->bytes = encode_loads_list(list).value();
    }
    // FILES.LUM has to follow the changed file for the read to get as far as the load.
    for (auto& member : lying)
    {
        auto* files_file = find(member, "\\", "FILES.LUM");
        auto files = decode_files_list(files_file->bytes).value();
        for (auto& entry : files.files)
        {
            if (entry.file_name == "LOADS.LUM")
            {
                const auto& owner = lying[entry.member_sequence_number - 1U];
                const auto bytes = member_file(owner, "\\", "LOADS.LUM");
                entry.crc = crc_of<Crc16>(bytes);
                entry.check_value = compute_check_value(CheckValueType::crc32, bytes).value();
            }
        }
        files.check_value.reset();
        files_file->bytes = encode_files_list(files).value();
    }

    CHECK(read_media_set(lying).error() == Error::inconsistent);
}

// ---- Files with the same name (section 3.2.4) ----

TEST_CASE("files of one name in different directories are told apart by directory and CRC", "[media-set]")
{
    // As for an ARINC 615 load of several disks: the same file names in disk001 and disk002.
    LoadSpec spec;
    spec.header_file_name = "ACM50.LUH";
    spec.part_number = PartNumber{"ACM50-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    spec.data_files.push_back({"DATA.BIN", PartNumber{}, bytes_of("FIRST DISK  "), CheckValueType::none});
    spec.data_files.push_back({"DATA.BIN", PartNumber{}, bytes_of("SECOND DISK "), CheckValueType::none});
    spec.support_files.push_back({"CONFIG.LDR", PartNumber{}, bytes_of("CONFIG"), CheckValueType::none});

    MediaSetSpec set_spec;
    set_spec.media_set_part_number = PartNumber{"DISKS-SET"};
    PlacedLoad placed;
    placed.load = build_load(spec).value();
    placed.data_placements = {FilePlacement{0, "disk001\\"}, FilePlacement{0, "disk002\\"}};
    set_spec.loads.push_back(std::move(placed));

    const auto members = build_media_set(set_spec);
    REQUIRE(members.has_value());
    const auto set = read_media_set(*members);

    REQUIRE(set.has_value());
    REQUIRE(set->loads.size() == 1);
    REQUIRE(set->loads[0].data_files.size() == 2);
    CHECK(set->loads[0].data_files[0].bytes == bytes_of("FIRST DISK  "));
    CHECK(set->loads[0].data_files[1].bytes == bytes_of("SECOND DISK "));
}

TEST_CASE("a file name that is in two loads' directories is found in its own load's", "[media-set]")
{
    // Two loads with a data file of the same name: each header file finds its own.
    const auto make_load = [](const char* header_name, const char* part_number, const char* content)
    {
        LoadSpec spec;
        spec.header_file_name = header_name;
        spec.part_number = PartNumber{part_number};
        spec.target_hardware_ids = {"ACME"};
        spec.data_files.push_back({"SAME.LUP", PartNumber{}, bytes_of(content), CheckValueType::none});
        return build_load(spec).value();
    };
    MediaSetSpec set_spec;
    set_spec.media_set_part_number = PartNumber{"TWO-LOADS"};
    set_spec.loads.push_back(PlacedLoad{make_load("ACM51.LUH", "ACM51-0000-0001", "ONE!"), "", 1, {}, {}});
    set_spec.loads.push_back(PlacedLoad{make_load("ACM52.LUH", "ACM52-0000-0002", "TWO!"), "", 1, {}, {}});

    const auto members = build_media_set(set_spec).value();
    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    REQUIRE(set->loads.size() == 2);
    CHECK(set->loads[0].data_files[0].bytes == bytes_of("ONE!"));
    CHECK(set->loads[1].data_files[0].bytes == bytes_of("TWO!"));
}

TEST_CASE("duplicate names with the same CRC are interchangeable, and the first is used", "[media-set]")
{
    // Section 3.2.4.2.3. Two copies in different directories of one load: any may be used.
    LoadSpec spec;
    spec.header_file_name = "ACM53.LUH";
    spec.part_number = PartNumber{"ACM53-1234-5678"};
    spec.target_hardware_ids = {"ACME"};
    spec.data_files.push_back({"COPY.BIN", PartNumber{}, bytes_of("SAME"), CheckValueType::none});

    MediaSetSpec set_spec;
    set_spec.media_set_part_number = PartNumber{"COPIES"};
    PlacedLoad placed;
    placed.load = build_load(spec).value();
    placed.data_placements = {FilePlacement{0, "a\\"}};
    set_spec.loads.push_back(std::move(placed));
    set_spec.other_files.push_back(PlacedFile{MediaFile{"\\ACM5312345678\\b\\", "COPY.BIN", bytes_of("SAME")}, 1});

    const auto members = build_media_set(set_spec).value();
    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    CHECK(set->loads[0].data_files[0].bytes == bytes_of("SAME"));
}

// ---- Findings ----

TEST_CASE("reading a media set reports what differs from the recommendations", "[media-set]")
{
    const auto members = sample_members();

    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    // The sample has the one finding its odd-length data file brings.
    REQUIRE(set->findings.size() == 1);
    CHECK(set->findings[0].clause == "2.2.3.2");
}

TEST_CASE("a file on a member that FILES.LUM does not list is reported, not refused", "[media-set]")
{
    auto members = sample_members();
    members[0].files.push_back(MediaFile{"\\", "STRAY.TXT", bytes_of("not listed")});

    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    const auto stray =
        std::find_if(set->findings.begin(), set->findings.end(), [](const Finding& finding)
                     { return finding.clause == "3.2.1" && finding.message.find("STRAY.TXT") != std::string::npos; });
    CHECK(stray != set->findings.end());
}

TEST_CASE("a set that departs from the recommendations is read and reported", "[media-set]")
{
    auto spec = adl::test::sample_media_set_spec();
    spec.media_set_part_number = PartNumber{"MEDIA SET PART NUMBER WITH BLANKS"};

    const auto members = build_media_set(spec).value();
    const auto set = read_media_set(members);

    REQUIRE(set.has_value());
    const auto about_part_number = std::count_if(set->findings.begin(), set->findings.end(),
                                                 [](const Finding& finding) { return finding.clause == "3.1"; });
    CHECK(about_part_number >= 2); // too long, and blanks (in each list that carries it)
}
