// SPDX-License-Identifier: GPL-3.0-or-later
// The seeds of the shared reader's fuzzer (libs/scenario_base/fuzz), replayed under the ordinary compilers: each seed,
// and every prefix of it, goes through the harness's own function and through what the harness checks. The fuzzer
// itself is built only with Clang and libFuzzer (docs/fuzzing.md); this is what keeps the seeds, and the reader's
// limits, honest on every build.

#include "fuzz_scenario_envelope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
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
        const std::filesystem::path path = std::filesystem::path{ADL_SCENARIO_FUZZ_DIR} / directory / "envelope";
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

TEST_CASE("the seeds of the shared reader's fuzzer, and every prefix of them, hold what the fuzzer checks",
          "[scenario][fuzz]")
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
            CHECK(adl::test::fuzz_scenario_envelope(bytes, length));
        }
        CHECK(LLVMFuzzerTestOneInput(bytes, text.size()) == 0);
    }
}

TEST_CASE("each seed is what its name says: read, or refused for the reason in it", "[scenario][fuzz]")
{
    using namespace adl::scenario;
    // The part of the sentence that says why, for the mock target's reader; an empty one is a file that is read.
    const std::map<std::string, std::string> reasons{
        {"mock_full", ""},
        {"empty_steps", ""},
        {"non_ascii_name", ""},
        {"escaped_name", ""},
        {"limits", ""},
        {"loader_style", "this is a scenario for a loader, not for a mock target"},
        {"wrong_kind", "this is a scenario for a loader, not for a mock target"},
        {"unknown_kind", "\"kind\" must be \"adl-mock-scenario\""},
        {"newer_format", "made for a newer version"},
        {"unknown_key", "step 1 has the key \"chanse\""},
        {"two_triggers", "needs exactly one of"},
        {"no_trigger", "needs exactly one of"},
        {"per_mille_without_seed", "no \"seed\""},
        {"fraction", "\"seed\" must be a whole number"},
        {"repeated_key", "it is not valid JSON"},
        {"too_deep", "it is not valid JSON"},
        {"comment_in_file", "it is not valid JSON"},
        {"step_key_with_new_line", R"(step 2 has the key "wh\n", which this version does not know)"},
    };
    static constexpr std::array<std::string_view, 1> mock_when{"operation"};
    const Family family{mock_kind, mock_when, {}};
    std::size_t seen = 0;
    for (const std::filesystem::path& file : seed_files())
    {
        const std::string name = file.stem().string();
        const auto reason = reasons.find(name);
        INFO(name);
        REQUIRE(reason != reasons.end());
        ++seen;
        const auto envelope = read_envelope(read_file(file), family);
        if (reason->second.empty())
        {
            CHECK(envelope.has_value());
        }
        else
        {
            REQUIRE(!envelope);
            CHECK(envelope.error().message.find(reason->second) != std::string::npos);
        }
    }
    CHECK(seen == reasons.size());
}
