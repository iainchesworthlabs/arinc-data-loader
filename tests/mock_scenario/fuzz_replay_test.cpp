// SPDX-License-Identifier: GPL-3.0-or-later
// The seeds of the scenario file's fuzzer (libs/mock_scenario/fuzz), replayed under the ordinary compilers: each seed,
// and every prefix of it, goes through the harness's own function and through what the harness checks. The fuzzer
// itself is built only with Clang and libFuzzer (docs/fuzzing.md); this is what keeps the seeds, and the reader's
// limits, honest on every build.

#include "fuzz_mock_scenario.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace
{

// The files of the seed corpus and of the past crashes, but for the .gitkeep that keeps a directory.
[[nodiscard]] std::vector<std::filesystem::path> seed_files()
{
    std::vector<std::filesystem::path> files;
    for (const char* directory : {"corpus", "regressions"})
    {
        const std::filesystem::path path = std::filesystem::path{ADL_MOCK_SCENARIO_FUZZ_DIR} / directory / "scenario";
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_regular_file() && entry.path().filename() != ".gitkeep")
            {
                files.push_back(entry.path());
            }
        }
    }
    std::ranges::sort(files);
    return files;
}

[[nodiscard]] std::string read_file(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("the seeds of the scenario file's fuzzer, and every prefix of them, hold what the fuzzer checks",
          "[mock_scenario][fuzz]")
{
    const std::vector<std::filesystem::path> files = seed_files();
    REQUIRE(files.size() >= 15); // the directory was found, and the seeds are there
    for (const std::filesystem::path& file : files)
    {
        INFO(file.filename().string());
        const std::string text = read_file(file);
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
        for (std::size_t length = 0; length <= text.size(); ++length)
        {
            CHECK(adl::test::fuzz_mock_scenario(bytes, length));
        }
        CHECK(LLVMFuzzerTestOneInput(bytes, text.size()) == 0);
    }
}

TEST_CASE("each seed is what its name says: read, or refused for the reason in it", "[mock_scenario][fuzz]")
{
    // The part of the sentence that says why; an empty one is a file that is read.
    const std::map<std::string, std::string> reasons{
        {"every_action", ""},
        {"guide_example", ""},
        {"minimum", ""},
        {"limits", ""},
        {"wrong_trigger_for_wait", "\"wait\" does not go with a \"status_file\" trigger"},
        {"wrong_trigger_for_abort", "\"abort\" does not go with a \"request\" trigger"},
        {"unknown_action", "is not one this version knows"},
        {"unknown_key_in_do", "\"do\" has the key \"for_ms\""},
        {"delay_without_seed", "uses \"min_ms\" but the scenario has no \"seed\""},
        {"delay_both_forms", "not both"},
        {"refuse_code", "\"code\" must be \"0x1000\" or \"0x1002\""},
        {"operation_word", "\"operation\" must be one of"},
        {"wait_zero", "\"seconds\" must be a whole number from 1 to 65535"},
        {"loader_kind", "this is a scenario for a loader, not for a mock target"},
        {"newer_format", "made for a newer version"},
        {"not_json", "it is not valid JSON"},
    };
    std::size_t seen = 0;
    for (const std::filesystem::path& file : seed_files())
    {
        const std::string name = file.stem().string();
        const auto reason = reasons.find(name);
        INFO(name);
        REQUIRE(reason != reasons.end());
        ++seen;
        const auto scenario = adl::mock::read_scenario(read_file(file));
        if (reason->second.empty())
        {
            CHECK(scenario.has_value());
        }
        else
        {
            REQUIRE(!scenario);
            CHECK(scenario.error().message.find(reason->second) != std::string::npos);
        }
    }
    CHECK(seen == reasons.size());
}
