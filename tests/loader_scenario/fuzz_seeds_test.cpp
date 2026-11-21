// SPDX-License-Identifier: GPL-3.0-or-later
// The seeds of the scenario harness (libs/loader_scenario/fuzz), replayed under the ordinary compilers: what the
// harness does to each of them is what the libFuzzer build does, so a seed that breaks it fails here on every preset.

#include "scenario_check.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_CASE("the seeds of the scenario fuzzer are read, written and read again, or refused", "[loader_scenario][fuzz]")
{
    using adl::loader_scenario::fuzz::check_scenario;
    using adl::loader_scenario::fuzz::Verdict;
    const std::filesystem::path fuzz{ADL_SCENARIO_FUZZ_DIR};
    std::size_t read = 0;
    std::size_t refused = 0;
    for (const char* directory : {"corpus", "regressions"})
    {
        for (const auto& entry : std::filesystem::directory_iterator(fuzz / directory / "scenario"))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".bin")
            {
                continue;
            }
            std::ifstream in(entry.path(), std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            const Verdict verdict = check_scenario(text);
            INFO(entry.path().string());
            REQUIRE(verdict != Verdict::inconsistent);
            // The seeds that are meant to be refused are, and the others are read.
            CHECK((verdict == Verdict::refused) == entry.path().filename().string().starts_with("refused_"));
            (verdict == Verdict::refused ? refused : read) += 1;
        }
    }
    CHECK(read >= 15);
    CHECK(refused >= 10);
}
