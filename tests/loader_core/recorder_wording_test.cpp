// SPDX-License-Identifier: GPL-3.0-or-later
// The recorder's sentences are the window's. The window's English is the source strings of its translation
// catalogue (apps/app/translations, read here as a file, as tests/unit/translations_test.cpp reads it, so that
// this needs no Qt), and the recorder's log has to read as the window's log does. Two things are held:
//
//  * every sentence the window's log listener (context `JobListener`) and its wording (context `Wording`) can
//    say is accounted for below, so a sentence added to the window without the recorder knowing it fails here;
//  * what the recorder writes for each of them is that source string with its %1, %2, %3 and %n filled in, and
//    the words of the statuses, outcomes, transfers and notices are the ones the engine's to_string gives.

#include "adl/loader_core/recorder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using adl::arinc615a::Notice;
using adl::arinc615a::OperationKind;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::StatusCode;
using adl::arinc615a::TransferOutcome;
using adl::loader_core::OperationRecorder;

namespace
{

[[nodiscard]] std::string read_catalogue()
{
    // Every language's catalogue holds every source string; German is as good as any.
    const std::filesystem::path path = std::filesystem::path{ADL_APP_TRANSLATIONS_DIR} / "adl_app_de.ts";
    std::ifstream in(path, std::ios::binary);
    INFO("catalogue " << path.string());
    REQUIRE(in.is_open());
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] std::string unescape(std::string_view text)
{
    static constexpr std::array<std::pair<std::string_view, char>, 5> entities{
        {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}}};
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '&')
        {
            for (const auto& [entity, character] : entities)
            {
                if (text.substr(i).starts_with(entity))
                {
                    result += character;
                    i += entity.size() - 1;
                    break;
                }
            }
            continue;
        }
        result += text[i];
    }
    return result;
}

// The source strings of one context of the catalogue.
[[nodiscard]] std::set<std::string> sources_of(const std::string& catalogue, std::string_view context)
{
    const std::string name = "<name>" + std::string{context} + "</name>";
    const std::size_t name_at = catalogue.find(name);
    INFO("context " << context);
    REQUIRE(name_at != std::string::npos);
    const std::size_t end = catalogue.find("</context>", name_at);
    REQUIRE(end != std::string::npos);

    std::set<std::string> sources;
    for (std::size_t at = catalogue.find("<source>", name_at); at != std::string::npos && at < end;
         at = catalogue.find("<source>", at))
    {
        const std::size_t from = at + std::string_view{"<source>"}.size();
        const std::size_t to = catalogue.find("</source>", from);
        REQUIRE(to != std::string::npos);
        sources.insert(unescape(std::string_view{catalogue}.substr(from, to - from)));
        at = to;
    }
    return sources;
}

// What Qt makes of a source string and its arguments: %1 to %3 and %n are replaced, and any other % stays.
[[nodiscard]] std::string fill(std::string_view source, std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> values{arguments};
    std::string result;
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        if (source[i] == '%' && i + 1 < source.size() && source[i + 1] >= '1' && source[i + 1] <= '3')
        {
            const std::size_t index = static_cast<std::size_t>(source[i + 1] - '1');
            REQUIRE(index < values.size());
            result += values[index];
            ++i;
        }
        else if (source[i] == '%' && i + 1 < source.size() && source[i + 1] == 'n')
        {
            REQUIRE_FALSE(values.empty());
            result += values[0];
            ++i;
        }
        else
        {
            result += source[i];
        }
    }
    return result;
}

OperationRecorder fresh()
{
    return OperationRecorder{OperationKind::upload, "10.0.0.2", "HNPFMS_L",
                             [] { return adl::loader_core::RecordTime{}; }};
}

// What a call makes the recorder log, as the one line it is expected to be.
template <typename Call>
[[nodiscard]] std::vector<std::string> logged_by(Call call)
{
    OperationRecorder recorder = fresh();
    call(recorder);
    std::vector<std::string> lines;
    for (const auto& line : recorder.record().log)
    {
        lines.push_back(line.text);
    }
    return lines;
}

using Lines = std::vector<std::string>;

} // namespace

TEST_CASE("every sentence the window's log listener says is one the recorder says", "[loader_core][recorder][wording]")
{
    const std::string catalogue = read_catalogue();

    // The source string, and what the recorder logs for a call that makes the window say it.
    struct Sentence
    {
        std::string source;
        Lines logged;
    };
    adl::arinc615a::UploadStatus upload;
    upload.counter = 4;
    upload.status = StatusCode::in_progress_with_text;
    upload.description = "checking";
    upload.list_ratio = 40;
    upload.estimated_time = 90;
    const std::array<adl::arinc615a::LoadProgress, 1> failed_load{
        adl::arinc615a::LoadProgress{"A.LUH", "PN-A", 30, StatusCode::item_failed, "no space"}};
    upload.loads = failed_load;
    const std::array<adl::arinc615a::FileProgress, 1> failed_file{
        adl::arinc615a::FileProgress{"X.BIN", StatusCode::item_failed, "unreadable"}};
    adl::arinc615a::DownloadStatus download;
    download.status = StatusCode::in_progress;
    download.files = failed_file;
    const std::array<adl::arinc615a::FileListing, 2> listed{adl::arinc615a::FileListing{"A.BIN", ""},
                                                            adl::arinc615a::FileListing{"B.BIN", ""}};
    adl::arinc615a::DownloadList list;
    list.files = listed;

    const std::string answered = "The target answered: %1";
    const std::string status = "Status %1: %2";
    const std::string status_of_list = "Status %1: %2, %3% of the list";
    const std::string time_left = ", about %1 s left";
    const std::vector<std::pair<std::string, Lines>> table{
        {answered, logged_by([](OperationRecorder& r) { r.on_initialization({StatusCode::accepted, ""}); })},
        {status, logged_by(
                     [](OperationRecorder& r)
                     {
                         adl::arinc615a::ConfigurationStatus s;
                         s.counter = 3;
                         s.status = StatusCode::in_progress;
                         r.on_configuration_status(s);
                     })},
        {status_of_list, logged_by([&](OperationRecorder& r) { r.on_upload_status(upload); })},
        {"The target failed load %1 (%2)%3", logged_by([&](OperationRecorder& r) { r.on_upload_status(upload); })},
        {"The target failed file %1%2", logged_by([&](OperationRecorder& r) { r.on_download_status(download); })},
        {"The target read %1 (%2 bytes)", logged_by([](OperationRecorder& r) { r.on_file_sent("A.BIN", 12); })},
        {"Received %1 (%2 bytes)",
         logged_by([](OperationRecorder& r)
                   { r.on_file_received("A.BIN", {12, adl::arinc615a::ChecksumVerdict::matched}); })},
        {"The target listed its configuration.",
         logged_by([](OperationRecorder& r) { r.on_configuration_list(adl::arinc615a::ConfigurationList{}); })},
        {"The target listed %n file(s) it can download.", logged_by(
                                                              [&](OperationRecorder& r)
                                                              {
                                                                  adl::arinc615a::FileSelection selection;
                                                                  static_cast<void>(
                                                                      r.on_download_list(list, selection));
                                                              })},
        {time_left, logged_by([&](OperationRecorder& r) { r.on_upload_status(upload); })}};

    // The sentences the window's listener has, which the table must hold all of. The window's rows say
    // "serial number %1" for a piece of hardware: the record has the serial number as a field of its own.
    std::set<std::string> in_table{"serial number %1"};
    for (const auto& [source, logged] : table)
    {
        in_table.insert(source);
    }
    CHECK(sources_of(catalogue, "JobListener") == in_table);

    // What the recorder says for each, built from the source string.
    CHECK(table[0].second == Lines{fill(answered, {"accepted"})});
    CHECK(table[1].second == Lines{fill(status, {"3", "in progress"})});
    CHECK(table[2].second == Lines{fill(status_of_list, {"4", "in progress, with details", "40"}) +
                                       fill(time_left, {"90"}) + " (checking)",
                                   fill("The target failed load %1 (%2)%3", {"PN-A", "A.LUH", ": no space"})});
    CHECK(table[3].second == table[2].second);
    CHECK(table[4].second == Lines{fill(status_of_list, {"0", "in progress", "0"}),
                                   fill("The target failed file %1%2", {"X.BIN", ": unreadable"})});
    CHECK(table[5].second == Lines{fill("The target read %1 (%2 bytes)", {"A.BIN", "12"})});
    CHECK(table[6].second == Lines{fill("Received %1 (%2 bytes)", {"A.BIN", "12"})});
    CHECK(table[7].second == Lines{"The target listed its configuration."});
    CHECK(table[8].second == Lines{fill("The target listed %n file(s) it can download.", {"2"})});
    CHECK(table[9].second == table[2].second);
}

TEST_CASE("the words of the statuses, outcomes, transfers and notices are the window's",
          "[loader_core][recorder][wording]")
{
    const std::string catalogue = read_catalogue();

    std::set<std::string> words;
    for (unsigned value = 0; value <= 0x1007; ++value)
    {
        if (adl::arinc615a::is_known(static_cast<StatusCode>(value)))
        {
            words.insert(std::string{adl::arinc615a::to_string(static_cast<StatusCode>(value))});
        }
    }
    for (int value = 0; value <= static_cast<int>(OutcomeCode::local_error); ++value)
    {
        words.insert(adl::arinc615a::to_string(static_cast<OutcomeCode>(value)));
    }
    for (int value = 0; value <= static_cast<int>(TransferOutcome::aborted); ++value)
    {
        words.insert(adl::arinc615a::to_string(static_cast<TransferOutcome>(value)));
    }
    for (int value = 0; value <= static_cast<int>(Notice::files_missing); ++value)
    {
        words.insert(adl::arinc615a::to_string(static_cast<Notice>(value)));
    }
    // What is said of a number that is none of them, and the two units the window words as sentences.
    CHECK(std::string{adl::arinc615a::to_string(static_cast<StatusCode>(0x7FFF))} == "unknown status code");
    CHECK(std::string{adl::arinc615a::to_string(static_cast<OutcomeCode>(200))} == "unknown");
    words.insert("unknown status code");
    words.insert("unknown");
    words.insert(" (%1 s)");
    words.insert(" (port %1)");
    CHECK(sources_of(catalogue, "Wording") == words);

    // And the recorder says the words of the status codes the target can send.
    for (unsigned value = 0; value <= 0x1007; ++value)
    {
        if (adl::arinc615a::is_known(static_cast<StatusCode>(value)))
        {
            const std::string word{adl::arinc615a::to_string(static_cast<StatusCode>(value))};
            CAPTURE(word);
            CHECK(logged_by([value](OperationRecorder& r)
                            { r.on_initialization({static_cast<StatusCode>(value), ""}); }) ==
                  Lines{fill("The target answered: %1", {word})});
        }
    }

    // A notice with a number in a unit is the notice's phrase, the file and the unit's own sentence.
    CHECK(logged_by([](OperationRecorder& r) { r.on_notice(Notice::waiting, "A.LUI", 5); }) ==
          Lines{"target asked to wait: A.LUI" + fill(" (%1 s)", {"5"})});
    CHECK(logged_by([](OperationRecorder& r) { r.on_notice(Notice::port_option_ignored, "A.LCI", 5900); }) ==
          Lines{"port option not taken by the target: A.LCI" + fill(" (port %1)", {"5900"})});
}
