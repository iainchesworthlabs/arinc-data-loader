// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/make_set/command.hpp"

#include "adl/loader_core/media_set_check.hpp"
#include "adl/make_set/description.hpp"
#include "adl/make_set/inspection.hpp"
#include "adl/make_set/json.hpp"
#include "adl/make_set/options.hpp"
#include "adl/make_set/version.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "adl/set_maker/inspect.hpp"
#include "adl/set_maker/make.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace adl::make_set
{

namespace
{

namespace fs = std::filesystem;
using adl::set_maker::FailureKind;

constexpr int events_number = 1;
constexpr std::string_view program_name = "adl_make_set";

std::string text_of(const fs::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

// "1 member", "2 members".
std::string counted(std::size_t count, std::string_view singular, std::string_view plural)
{
    return std::to_string(count) + " " + std::string{count == 1 ? singular : plural};
}

// Every file on every member: the files FILES.LUM lists and the FILES.LUM of each member, which it does
// not list.
std::size_t files_on_members(const adl::loader_core::MediaSetReport& report)
{
    return report.files.size() + report.members;
}

// What a check or a make found, in the form of `adl_write_payloads`.
std::string summary_line(const adl::loader_core::MediaSetReport& report, std::string_view where)
{
    return report.part_number + ": " + counted(report.members, "member", "members") + ", " +
           counted(files_on_members(report), "file", "files") + ", " + counted(report.loads.size(), "load", "loads") +
           ", " + counted(report.batches, "batch", "batches") + ", " +
           counted(report.findings.size(), "finding", "findings") + " -> " + std::string{where};
}

std::string completion_sentence(std::string_view verb, const adl::loader_core::MediaSetReport& report)
{
    return "Media set " + report.part_number + " " + std::string{verb} + ": " +
           counted(report.members, "member", "members") + ", " + counted(files_on_members(report), "file", "files") +
           ", " + counted(report.findings.size(), "finding", "findings") + ".";
}

// Where the result goes: JSON events on standard output, or plain lines. Standard output that cannot be
// written is remembered and ends the run with exit_local.
class Sink
{
public:
    Sink(std::ostream& out, std::ostream& err, bool json, std::string_view command)
        : out_(out), err_(err), json_(json), command_(command), start_(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] bool json() const noexcept
    {
        return json_;
    }
    [[nodiscard]] bool output_failed() const noexcept
    {
        return output_failed_;
    }

    // An event: the object that has been begun with event() and its members, ended and written as a line.
    JsonWriter event(std::string_view name)
    {
        JsonWriter writer;
        writer.begin_object().member("event", name).key("t").seconds(elapsed());
        return writer;
    }
    // Ends the event and writes it as a line, in JSON mode; in plain mode there are no events.
    void emit(JsonWriter& writer)
    {
        if (!json_)
        {
            return;
        }
        writer.end_object();
        write_line(writer.text());
    }

    void log(std::string_view severity, std::string_view text)
    {
        if (json_)
        {
            JsonWriter writer = event("log");
            writer.member("severity", severity).member("text", text);
            emit(writer);
        }
        else if (severity == "error")
        {
            err_ << program_name << ": " << text << '\n';
        }
    }

    void write_line(const std::string& text)
    {
        out_ << text << '\n';
        out_.flush();
        if (!out_)
        {
            output_failed_ = true;
        }
    }

    void plain(const std::string& text)
    {
        if (!json_)
        {
            write_line(text);
        }
    }

    [[nodiscard]] std::chrono::milliseconds elapsed() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_);
    }

    [[nodiscard]] std::string_view command() const noexcept
    {
        return command_;
    }

private:
    std::ostream& out_;
    std::ostream& err_;
    bool json_;
    std::string_view command_;
    std::chrono::steady_clock::time_point start_;
    bool output_failed_ = false;
};

JsonWriter started_event(Sink& sink)
{
    JsonWriter writer = sink.event("started");
    writer.member("program", program_name).member("version", version).member_int("events", events_number);
    writer.member("command", sink.command());
    return writer;
}

void checked_event(Sink& sink, std::string_view path, const adl::loader_core::MediaSetReport& report)
{
    if (!sink.json())
    {
        return;
    }
    JsonWriter writer = sink.event("media_set_checked");
    writer.member("path", path).member("part_number", report.part_number);
    writer.member_int("members", report.members)
        .member_int("loads", static_cast<std::int64_t>(report.loads.size()))
        .member_int("batches", static_cast<std::int64_t>(report.batches))
        .member_int("files", static_cast<std::int64_t>(files_on_members(report)));
    writer.member("findings", report.findings);
    sink.emit(writer);
}

// The `finished` event, or in plain mode the sentence of a failure.
int finish(Sink& sink, const Verdict& verdict, const std::string& detail_for_plain_errors,
           const adl::loader_core::MediaSetReport* report, std::string_view output)
{
    Verdict final_verdict = verdict;
    if (sink.output_failed())
    {
        final_verdict = Verdict{false, "local_error", "Standard output could not be written.", exit_local};
    }
    if (sink.json())
    {
        JsonWriter writer = sink.event("finished");
        writer.member("result", final_verdict.completed ? "completed" : "failed");
        writer.member("reason", final_verdict.reason).member("detail", final_verdict.detail);
        writer.member_int("exit_code", final_verdict.exit_code);
        if (sink.command() == "make")
        {
            if (final_verdict.completed)
            {
                writer.member("output", output);
            }
            else
            {
                writer.member_null("output");
            }
        }
        if (report != nullptr && final_verdict.completed)
        {
            writer.member_int("members", report->members)
                .member_int("files", static_cast<std::int64_t>(files_on_members(*report)))
                .member_int("finding_count", static_cast<std::int64_t>(report->findings.size()));
        }
        else
        {
            writer.member_null("members").member_null("files").member_null("finding_count");
        }
        sink.emit(writer);
    }
    else if (!final_verdict.completed && !detail_for_plain_errors.empty())
    {
        sink.log("error", detail_for_plain_errors);
    }
    return final_verdict.exit_code;
}

// ---------------------------------------------------------------------------------------------------------
// make
// ---------------------------------------------------------------------------------------------------------

// What the library tells the command while it makes the set, written as events.
class MakeHooks final : public adl::set_maker::Hooks
{
public:
    MakeHooks(Sink& sink, std::string_view output) : sink_(sink), output_(output) {}

    void progress(int percent) override
    {
        if (percent <= last_percent_)
        {
            return;
        }
        last_percent_ = percent;
        if (sink_.json())
        {
            JsonWriter writer = sink_.event("progress");
            writer.member_int("percent", percent);
            sink_.emit(writer);
        }
    }

    void written(std::size_t members, std::size_t files, std::uint64_t bytes) override
    {
        if (sink_.json())
        {
            JsonWriter writer = sink_.event("set_written");
            writer.member_int("members", static_cast<std::int64_t>(members))
                .member_int("files", static_cast<std::int64_t>(files))
                .member_int("bytes", static_cast<std::int64_t>(bytes));
            sink_.emit(writer);
        }
    }

    void checked(const adl::loader_core::MediaSetReport& report) override
    {
        checked_event(sink_, output_, report);
    }

    void took_over_empty_folder(const fs::path& folder) override
    {
        sink_.log("note", "The empty folder " + text_of(folder) + " was taken over.");
    }

private:
    Sink& sink_;
    std::string output_;
    int last_percent_ = -1;
};

// "<what> (<system words>)" or "<what>".
std::string with_system_words(std::string what, const std::string& system)
{
    if (!system.empty())
    {
        what += " (" + system + ")";
    }
    return what;
}

// What became of what had been written, in a sentence to follow the failure.
std::string discard_sentence(const adl::set_maker::Discard& discard)
{
    if (!discard.attempted)
    {
        return {};
    }
    return discard.removed
               ? " Nothing was kept: what had been written was removed."
               : " What had been written could not be removed, and is still in " + text_of(discard.where) + ".";
}

Verdict verdict_of(const adl::set_maker::MakeFailure& failure, const fs::path& output)
{
    const std::string below = text_of(failure.path);
    const std::string discard = discard_sentence(failure.discard);
    switch (failure.kind)
    {
    case FailureKind::source_unreadable:
        return {false, "source_unreadable", "Cannot read " + text_of(failure.path) + ".", exit_input};
    case FailureKind::source_too_large:
    case FailureKind::source_does_not_fit:
        return {false, "source_unreadable", text_of(failure.path) + " is too large to be read into memory.",
                exit_input};
    case FailureKind::no_data_files:
        return {false, "description_invalid", "The load has no data file.", exit_input};
    case FailureKind::load_not_built:
        return {false, "description_invalid", "The load could not be built: " + failure.detail + ".", exit_input};
    case FailureKind::set_not_built:
        return {false, "description_invalid", "The media set could not be built: " + failure.detail + ".", exit_input};
    case FailureKind::output_exists:
        return {false, "output_exists", text_of(failure.path) + " is there already and is not an empty folder.",
                exit_output_exists};
    case FailureKind::not_written_below:
        return {false, "local_error",
                with_system_words("The media set could not be written below " + below,
                                  failure.system.empty() ? failure.detail : failure.system) +
                    ".",
                exit_local};
    case FailureKind::too_many_unfinished:
        return {false, "local_error",
                "Too many unfinished sets (" + text_of(failure.path) + ".writing...) are beside the output.",
                exit_local};
    case FailureKind::write_failed:
        return {false, "local_error",
                "The media set could not be written below " + below + ": " + failure.detail + " (" +
                    text_of(failure.at) + ")." + discard,
                exit_local};
    case FailureKind::read_back_failed:
        return {false, "set_invalid",
                "The written media set could not be read back: " + failure.detail + " (" + text_of(failure.at) + ")." +
                    discard,
                exit_set_invalid};
    case FailureKind::not_verified:
        return {false, "set_invalid", "The written media set does not verify: " + failure.detail + "." + discard,
                exit_set_invalid};
    case FailureKind::not_the_set_described:
        return {false, "set_invalid",
                "The written media set is not the one described: " + failure.detail + "." + discard, exit_set_invalid};
    case FailureKind::not_given_its_name:
        return {false, "local_error",
                with_system_words("The media set could not be put in place at " + text_of(output), failure.system) +
                    "." + discard,
                exit_local};
    case FailureKind::cancelled:
        return {false, "local_error", "The job was stopped." + discard, exit_local};
    case FailureKind::out_of_memory:
        return {false, "local_error", "The computer ran out of memory while the media set was being made.", exit_local};
    case FailureKind::error:
        return {false, "local_error", "The media set could not be made: " + failure.detail + ".", exit_local};
    }
    return {false, "local_error", "The media set could not be made.", exit_local};
}

int run_make(const Options& options, Sink& sink)
{
    JsonWriter started = started_event(sink);
    started.member("description", options.subject).member("output", *options.output);
    sink.emit(started);

    const fs::path description_file = adl::platform::path_from_utf8(options.subject);
    std::optional<fs::path> root;
    if (options.source_root)
    {
        root = adl::platform::path_from_utf8(*options.source_root);
    }
    auto description = read_description_file(description_file, root);
    if (!description)
    {
        for (const std::string& sentence : description.error().sentences)
        {
            sink.log("error", sentence);
        }
        // In plain mode every sentence has been printed; in JSON the first is the detail.
        return finish(sink, Verdict{false, "description_invalid", description.error().sentences.front(), exit_input},
                      {}, nullptr, {});
    }
    const adl::set_maker::SetPlan& plan = description->plan;
    if (sink.json())
    {
        JsonWriter writer = sink.event("description_read");
        std::size_t files = 0;
        for (const adl::set_maker::PlannedLoad& load : plan.loads)
        {
            files += load.files.size();
        }
        writer.member("media_set_part_number", plan.part_number).member_int("members", plan.members);
        writer.member_int("loads", static_cast<std::int64_t>(plan.loads.size()))
            .member_int("files", static_cast<std::int64_t>(files))
            .member_bool("batch", plan.batch.has_value());
        sink.emit(writer);
    }

    const fs::path output = adl::platform::path_from_utf8(*options.output);
    MakeHooks hooks{sink, *options.output};
    auto made = adl::set_maker::make_set(plan, adl::set_maker::Destination::exact(output), hooks);
    if (!made)
    {
        const Verdict verdict = verdict_of(made.error(), output);
        return finish(sink, verdict, verdict.detail, nullptr, {});
    }

    const adl::loader_core::MediaSetReport& report = made->report;
    if (!sink.json())
    {
        sink.plain(summary_line(report, *options.output));
        for (const std::string& finding : report.findings)
        {
            sink.plain("    " + finding);
        }
    }
    return finish(sink, Verdict{true, "completed", completion_sentence("written and verified", report), exit_ok}, {},
                  &report, *options.output);
}

// ---------------------------------------------------------------------------------------------------------
// verify and inspect
// ---------------------------------------------------------------------------------------------------------

Verdict folder_unreadable(const fs::path& folder)
{
    return {false, "folder_unreadable", text_of(folder) + " is not a folder, or cannot be read.", exit_input};
}

int run_verify(const Options& options, Sink& sink)
{
    JsonWriter started = started_event(sink);
    started.member("folder", options.subject);
    sink.emit(started);

    const fs::path folder = adl::platform::path_from_utf8(options.subject);
    std::error_code error;
    if (!fs::is_directory(folder, error))
    {
        const Verdict verdict = folder_unreadable(folder);
        return finish(sink, verdict, verdict.detail, nullptr, {});
    }
    const adl::loader_core::MediaSetReport report = adl::loader_core::check_media_set(folder);
    if (!report.error.empty())
    {
        const Verdict verdict{false, "set_invalid", "The media set could not be read: " + report.error + ".",
                              exit_set_invalid};
        return finish(sink, verdict, verdict.detail, nullptr, {});
    }
    checked_event(sink, options.subject, report);
    if (!sink.json())
    {
        sink.plain(summary_line(report, options.subject));
        for (const std::string& finding : report.findings)
        {
            sink.plain("    " + finding);
        }
    }
    return finish(sink, Verdict{true, "completed", completion_sentence("verified", report), exit_ok}, {}, &report, {});
}

// The plain lines of `inspect`: the summary, each load, each finding.
void print_inspection(Sink& sink, const adl::set_maker::Inspection& inspection, std::string_view folder)
{
    const std::size_t files = inspection.files.size() + inspection.members_present.size();
    sink.plain(inspection.part_number + ": " + counted(inspection.members, "member", "members") + ", " +
               counted(files, "file", "files") + ", " + counted(inspection.loads.size(), "load", "loads") + ", " +
               counted(inspection.batches.size(), "batch", "batches") + ", " +
               counted(inspection.findings.size(), "finding", "findings") + " -> " + std::string{folder});
    for (const adl::set_maker::InspectedLoad& load : inspection.loads)
    {
        sink.plain("    load " + load.part_number + " (" + load.header_file + ", member " +
                   std::to_string(load.header_member) +
                   "): " + counted(load.data_files.size(), "data file", "data files") + ", " +
                   counted(load.support_files.size(), "support file", "support files"));
    }
    for (const std::string& finding : inspection.findings)
    {
        sink.plain("    " + finding);
    }
}

int run_inspect(const Options& options, Sink& sink)
{
    const fs::path folder = adl::platform::path_from_utf8(options.subject);
    auto inspected = adl::set_maker::inspect_set(folder);

    Verdict verdict;
    if (inspected)
    {
        const adl::set_maker::Inspection& inspection = *inspected;
        const std::size_t files = inspection.files.size() + inspection.members_present.size();
        verdict = Verdict{true, "completed",
                          "Media set " + inspection.part_number +
                              " read: " + counted(inspection.members, "member", "members") + ", " +
                              counted(files, "file", "files") + ", " +
                              counted(inspection.findings.size(), "finding", "findings") + ".",
                          exit_ok};
    }
    else if (inspected.error().kind == adl::set_maker::InspectFailureKind::folder_unreadable)
    {
        verdict = folder_unreadable(folder);
    }
    else
    {
        verdict = Verdict{
            false, "set_invalid",
            "The media set could not be read: " + inspected.error().detail +
                (inspected.error().path.empty() ? std::string{} : " (" + text_of(inspected.error().path) + ")") + ".",
            exit_set_invalid};
    }

    if (sink.json())
    {
        // One object, with its own outcome. It is the whole of standard output.
        sink.write_line(inspection_document(verdict, options.subject, inspected ? &*inspected : nullptr));
        return sink.output_failed() ? exit_local : verdict.exit_code;
    }
    if (inspected)
    {
        print_inspection(sink, *inspected, options.subject);
    }
    else
    {
        sink.log("error", verdict.detail);
    }
    return sink.output_failed() ? exit_local : verdict.exit_code;
}

} // namespace

int run_command(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err)
{
    auto options = parse_options(arguments);
    if (!options)
    {
        err << program_name << ": " << options.error().sentence << '\n' << usage_text();
        return exit_usage;
    }
    if (options->help)
    {
        out << usage_text();
        return exit_ok;
    }
    if (options->version)
    {
        out << program_name << ' ' << version << " events " << events_number << '\n';
        return exit_ok;
    }

    Sink sink{out, err, options->json, to_string(options->command)};
    switch (options->command)
    {
    case Command::make:
        return run_make(*options, sink);
    case Command::verify:
        return run_verify(*options, sink);
    case Command::inspect:
        return run_inspect(*options, sink);
    }
    return exit_usage;
}

} // namespace adl::make_set
