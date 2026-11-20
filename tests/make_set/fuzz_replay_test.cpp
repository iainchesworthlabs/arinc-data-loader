// SPDX-License-Identifier: GPL-3.0-or-later
// The fuzz harnesses of libs/set_maker/fuzz, run on their seeds and on the inputs that once crashed them, in the
// ordinary builds: what a harness checks is checked here too, by the same functions (fuzz/checks.hpp), so a
// build with no Clang still knows the seeds are good. The disk path of `inspect` is run on every seed as well:
// each is written out as a tree and read back with inspect_set().

#include "adl/media_tree/media_tree.hpp"
#include "adl/set_maker/inspect.hpp"
#include "checks.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

namespace fs = std::filesystem;

std::vector<fs::path> inputs(const std::string& target)
{
    std::vector<fs::path> files;
    for (const char* kind : {"corpus", "regressions"})
    {
        const fs::path directory = fs::path{ADL_SET_MAKER_FUZZ_DIR} / kind / target;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(directory, error))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
    }
    std::ranges::sort(files);
    return files;
}

std::string read_all(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Writes the members as a tree, the way adl::media_tree reads them. False if a name is one that no tree holds.
bool write_tree(const fs::path& root, const std::vector<adl::arinc665::MediaMember>& members)
{
    return adl::media_tree::write_media_set_tree(root, members).has_value();
}

} // namespace

TEST_CASE("fuzz seeds: every description seed and every prefix of it is as the harness wants", "[fuzz]")
{
    const auto files = inputs("description");
    REQUIRE(files.size() >= 8);
    for (const fs::path& file : files)
    {
        INFO(file.filename().string());
        const std::string text = read_all(file);
        CHECK(adl::set_maker::fuzz::check_description(text).empty());
        for (std::size_t length = 0; length < text.size() && length < 4096; ++length)
        {
            const std::string violation =
                adl::set_maker::fuzz::check_description(std::string_view{text}.substr(0, length));
            INFO(length);
            REQUIRE(violation.empty());
        }
    }
}

TEST_CASE("fuzz seeds: every command line seed and every prefix of it is as the harness wants", "[fuzz]")
{
    const auto files = inputs("options");
    REQUIRE(files.size() >= 6);
    for (const fs::path& file : files)
    {
        INFO(file.filename().string());
        const std::string text = read_all(file);
        CHECK(adl::set_maker::fuzz::check_options(text).empty());
        for (std::size_t length = 0; length < text.size(); ++length)
        {
            REQUIRE(adl::set_maker::fuzz::check_options(std::string_view{text}.substr(0, length)).empty());
        }
    }
}

TEST_CASE("fuzz seeds: every set seed is inspected as the harness wants, from memory and from disk", "[fuzz]")
{
    const auto files = inputs("inspect_set");
    REQUIRE(files.size() >= 5);
    std::size_t good = 0;
    for (const fs::path& file : files)
    {
        INFO(file.filename().string());
        const std::string text = read_all(file);
        const std::vector<std::uint8_t> bytes(text.begin(), text.end());
        CHECK(adl::set_maker::fuzz::check_members(bytes).empty());

        // Cut anywhere: still a refusal with a reason, or a set.
        for (std::size_t length = 0; length < bytes.size(); length += std::max<std::size_t>(1, bytes.size() / 60))
        {
            INFO(length);
            REQUIRE(adl::set_maker::fuzz::check_members({bytes.data(), length}).empty());
        }

        // The same seed through the disk: the answer is the same.
        const auto members = adl::set_maker::fuzz::members_of(bytes);
        adl::test::TempDir directory;
        const fs::path root = directory.path() / "set";
        if (!write_tree(root, members))
        {
            continue; // the members are not a tree (an odd name, or two files of one name)
        }
        const auto from_memory = adl::set_maker::inspect_members(members);
        const auto from_disk = adl::set_maker::inspect_set(root);
        REQUIRE(from_memory.has_value() == from_disk.has_value());
        if (from_memory)
        {
            ++good;
            CHECK(from_memory->part_number == from_disk->part_number);
            CHECK(from_memory->files.size() == from_disk->files.size());
            CHECK(from_memory->findings == from_disk->findings);
        }
    }
    CHECK(good >= 1); // at least one seed is a whole set
}
