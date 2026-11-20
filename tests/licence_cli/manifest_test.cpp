// SPDX-License-Identifier: GPL-3.0-or-later
// Every row of the manifest of test licences (tests/licence/data/licences/manifest.txt) judged by `show` and by
// `verify`: which licence, when and where it is judged, and the state and reason it is in. The same rows are
// judged by the library (tests/licence/store_test.cpp) and by the owner's tool (test_owner_tool.py); here it is the
// words of adl_licence that are held to them. The files are made by tests/licence/make_test_licences.py and signed
// with the throwaway test key.

#include "adl/licence/licence.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence/words.hpp"
#include "adl/licence_cli/licence_cli.hpp"
#include "memory_files.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lic = adl::licence;
namespace st = adl::licence::store;
namespace cli = adl::licence_cli;
namespace fs = std::filesystem;

namespace
{

struct Row
{
    std::string file;
    std::string now;
    std::string latest_seen;
    std::string computer;
    std::string revocations;
    std::string state;
    std::string reason;
};

[[nodiscard]] std::string read_bytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

[[nodiscard]] std::vector<Row> read_manifest(const fs::path& data)
{
    std::vector<Row> rows;
    std::istringstream lines(read_bytes(data / "licences" / "manifest.txt"));
    for (std::string line; std::getline(lines, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream words(line);
        std::vector<std::string> columns;
        for (std::string word; words >> word;)
        {
            columns.push_back(word);
        }
        if (columns.empty() || columns.front().starts_with('#'))
        {
            continue;
        }
        REQUIRE(columns.size() >= 7);
        rows.push_back({columns[0], columns[1], columns[2], columns[3], columns[4], columns[5], columns[6]});
    }
    return rows;
}

[[nodiscard]] lic::TimePoint time_of(const std::string& text)
{
    const auto parsed = lic::parse_time(text);
    REQUIRE(parsed);
    return *parsed;
}

[[nodiscard]] lic::Reason reason_of(const std::string& name)
{
    static const std::map<std::string, lic::Reason, std::less<>> reasons{
        {"ok", lic::Reason::ok},
        {"no_licence", lic::Reason::no_licence},
        {"expired", lic::Reason::expired},
        {"not_yet_valid", lic::Reason::not_yet_valid},
        {"clock_suspect", lic::Reason::clock_suspect},
        {"malformed", lic::Reason::malformed},
        {"newer_version", lic::Reason::newer_version},
        {"bad_signature", lic::Reason::bad_signature},
        {"revoked", lic::Reason::revoked},
        {"wrong_computer", lic::Reason::wrong_computer},
        {"wrong_product", lic::Reason::wrong_product},
        {"not_in_licence", lic::Reason::not_in_licence},
    };
    const auto found = reasons.find(name);
    REQUIRE(found != reasons.end());
    return found->second;
}

// The `Name: value` lines of an output; the first of each name wins.
[[nodiscard]] std::map<std::string, std::string> lines_of(const std::string& output)
{
    std::map<std::string, std::string> found;
    std::istringstream lines(output);
    for (std::string line; std::getline(lines, line);)
    {
        const auto colon = line.find(": ");
        if (colon != std::string::npos && !line.starts_with(' '))
        {
            found.try_emplace(line.substr(0, colon), line.substr(colon + 2));
        }
    }
    return found;
}

} // namespace

TEST_CASE("every row of the manifest of test licences is shown and checked in the state it names",
          "[unit][licence_cli][manifest]")
{
    const fs::path data{ADL_LICENCE_TEST_DATA_DIR};
    const std::vector<Row> rows = read_manifest(data);
    REQUIRE(rows.size() >= 25);

    for (const Row& row : rows)
    {
        CAPTURE(row.file, row.now, row.latest_seen, row.computer, row.revocations);
        const lic::TimePoint now = time_of(row.now);
        const std::string licence = row.file == "-" ? std::string{} : read_bytes(data / "licences" / row.file);
        const std::string list = row.revocations == "-" ? std::string{} : read_bytes(data / "licences" / row.revocations);

        const auto make = [&](const std::string& folder_file)
        {
            auto files = std::make_unique<adl::test::MemoryFiles>();
            cli::Environment env;
            env.files = files.get();
            env.folders = {"home/licences", "machine/licences", "home/licence-clock.json"};
            env.computer_name = row.computer == "-" ? "LAB-RIG-07" : row.computer;
            const auto test_key = lic::test_keys::trusted();
            env.keys.assign(test_key.begin(), test_key.end());
            env.now = [now]() -> std::optional<lic::TimePoint> { return now; };
            if (row.latest_seen != "-")
            {
                files->put(env.folders.clock_file,
                           st::write_clock({.latest_seen = time_of(row.latest_seen), .applied_resets = {}}));
            }
            if (!list.empty())
            {
                files->put("home/licences/" + row.revocations, list);
            }
            if (!licence.empty())
            {
                files->put(folder_file, licence);
            }
            return std::make_pair(std::move(files), std::move(env));
        };
        const auto run = [](const cli::Environment& env, std::vector<std::string> arguments)
        {
            std::ostringstream out;
            std::ostringstream err;
            const int code = cli::run(arguments, env, {out, err});
            return std::tuple{code, out.str(), err.str()};
        };

        // `show`, with the file in the user's folder.
        {
            const auto [files, env] = make("home/licences/" + row.file);
            const auto [code, out, err] = run(env, {"show"});
            CHECK(code == cli::exit_ok);
            CHECK(err.empty());
            const auto lines = lines_of(out);
            CHECK(lines.at("State") == row.state);
            const lic::Reason reason = reason_of(row.reason);
            if (reason == lic::Reason::ok)
            {
                CHECK_FALSE(lines.contains("Reason"));
            }
            else
            {
                CHECK(lines.at("Reason") == lic::reason_sentence(reason));
            }
            const bool in_force = row.state == "valid" || row.state == "expiring" || row.state == "grace";
            const bool is_paid_edition = lines.at("Edition") != "community";
            CHECK(is_paid_edition == in_force);
        }

        // `verify`, with the file where a customer would have it. It takes everything but what a program refuses.
        if (!licence.empty())
        {
            const auto [files, env] = make("inbox/" + row.file);
            const auto before = files->files;
            const auto [code, out, err] = run(env, {"verify", "inbox/" + row.file});
            CHECK(code == (row.state == "invalid" ? cli::exit_failed : cli::exit_ok));
            CHECK(files->files == before);
            if (row.state != "invalid")
            {
                CHECK(lines_of(out).at("State") == row.state);
                CHECK(out.ends_with("A program would accept this licence file.\n"));
            }
        }
    }
}
