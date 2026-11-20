// SPDX-License-Identifier: GPL-3.0-or-later
// The seeds of the command line's fuzzer (libs/mock_target_core/fuzz), replayed under the ordinary compilers:
// each seed, and every prefix of it, goes through the harness's own function and through what the harness
// checks. The fuzzer itself is built only with Clang and libFuzzer (docs/fuzzing.md); this is what keeps the
// seeds, and the parser's limits, honest on every build.

#include "fuzz_mock_options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        const std::filesystem::path path = std::filesystem::path{ADL_MOCK_FUZZ_DIR} / directory / "parse_options";
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

} // namespace

TEST_CASE("the seeds of the command line's fuzzer, and every prefix of them, hold what the fuzzer checks",
          "[mock_cli][fuzz]")
{
    const std::vector<std::filesystem::path> files = seed_files();
    REQUIRE(files.size() >= 10); // the directory was found, and the seeds are there
    for (const std::filesystem::path& file : files)
    {
        INFO(file.filename().string());
        std::ifstream in(file, std::ios::binary);
        const std::vector<char> text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
        for (std::size_t length = 0; length <= text.size(); ++length)
        {
            CHECK(adl::test::fuzz_mock_options(bytes, length));
        }
        CHECK(LLVMFuzzerTestOneInput(bytes, text.size()) == 0);
    }
}

TEST_CASE("the input of the fuzzer is read as arguments, a line feed after each", "[mock_cli][fuzz]")
{
    const auto arguments = [](std::string text)
    {
        std::vector<std::string> out;
        for (const std::string_view argument :
             adl::test::fuzz_arguments(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()))
        {
            out.emplace_back(argument);
        }
        return out;
    };
    CHECK(arguments("").empty());
    CHECK(arguments("--json\n") == std::vector<std::string>{"--json"});
    CHECK(arguments("--json") == std::vector<std::string>{"--json"});
    CHECK(arguments("--seconds\n5\n") == std::vector<std::string>{"--seconds", "5"});
    CHECK(arguments("a\n\nb\n") == std::vector<std::string>{"a", "", "b"});
    CHECK(arguments("\n") == std::vector<std::string>{""});
}
