// SPDX-License-Identifier: GPL-3.0-or-later
// The seed corpus and the past crashes of the fuzz harness of adl_load's options (apps/load/fuzz), replayed
// under the ordinary compilers: the harness itself needs Clang and libFuzzer, and this is how every build
// finds out that a seed no longer decodes the way it was written to, or that an input that once crashed
// the parser still does not.

#include "load_options_fuzz.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

// The bytes of every file in `directory`, which has to be there.
std::vector<std::vector<std::uint8_t>> files_in(const std::filesystem::path& directory)
{
    REQUIRE(std::filesystem::is_directory(directory));
    std::vector<std::vector<std::uint8_t>> contents;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().filename() == ".gitkeep")
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        contents.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return contents;
}

} // namespace

TEST_CASE("the seeds of the options fuzzer are read, and every command line they make reads back the same",
          "[load][options][fuzz]")
{
    const std::filesystem::path fuzz{ADL_LOAD_FUZZ_DIR};
    std::size_t commands = 0;
    std::size_t refused = 0;
    for (const char* directory : {"corpus", "regressions"})
    {
        for (const std::vector<std::uint8_t>& bytes : files_in(fuzz / directory / "load_options"))
        {
            const adl::test::OptionsFuzzResult result = adl::test::fuzz_options(bytes.data(), bytes.size());
            CHECK(result.round_trips);
            (result.made_a_command ? commands : refused) += 1;
        }
    }
    // The seeds hold a command of each kind, and what is refused as well as what is accepted.
    CHECK(commands >= 9);
    CHECK(refused >= 20);
}

TEST_CASE("the fuzzer's bytes are cut into arguments at each NUL", "[load][options][fuzz]")
{
    const char nul = static_cast<char>(0);
    const std::string bytes = std::string{"find"} + nul + "--seconds" + nul + "5";
    const auto arguments = adl::test::fuzz_arguments(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    REQUIRE(arguments.size() == 3);
    CHECK(arguments[0] == "find");
    CHECK(arguments[1] == "--seconds");
    CHECK(arguments[2] == "5");
    CHECK(adl::test::fuzz_arguments(nullptr, 0) == std::vector<std::string>{""});

    const adl::test::OptionsFuzzResult result =
        adl::test::fuzz_options(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    CHECK(result.made_a_command);
    CHECK(result.round_trips);
}
