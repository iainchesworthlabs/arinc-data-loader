// SPDX-License-Identifier: GPL-3.0-or-later
// information, upload and download: one operation on one target. What the window does for the operator, in
// order: read and check the media set of an upload, make the folder of a download, run the operation, and say
// how it ended, here as events and an exit code.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/load/program.hpp"
#include "adl/load/reason.hpp"
#include "adl/loader_core/address.hpp"
#include "adl/loader_core/download_directory.hpp"
#include "adl/loader_core/gate.hpp"
#include "adl/loader_core/media_set_check.hpp"
#include "adl/loader_core/operation_plan.hpp"
#include "adl/loader_core/record_json.hpp"
#include "adl/loader_core/run.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "commands.hpp"
#include "job.hpp"
#include "record_file.hpp"

#include <algorithm>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace adl::load
{

namespace
{

using adl::arinc615a::OperationKind;
using adl::arinc615a::OutcomeCode;
using adl::loader_core::MediaSetIndex;
using adl::loader_core::MediaSetReport;
using adl::loader_core::OperationResult;
using adl::loader_core::Severity;

// "(s)" is how the window's own sentences write a number that may be one.
std::string count_s(std::uint64_t count, std::string_view noun)
{
    return std::to_string(count) + ' ' + std::string{noun} + "(s)";
}

// The platform path (UTF-8, '/' separated) of a path as the operator wrote it, which is UTF-8 text: the arguments
// come from adl::platform::arguments, and a path is made from one by path_from_utf8, never by the code page.
std::string platform_path_of(const std::string& path)
{
    return adl::platform::to_platform_path(adl::platform::path_from_utf8(path));
}

// A platform path as the operating system writes it: its own separator, UTF-8.
std::string native_path_of(const std::string& platform_path)
{
    const std::u8string text{reinterpret_cast<const char8_t*>(platform_path.data()), platform_path.size()};
    std::filesystem::path path{text};
    path.make_preferred();
    return adl::platform::to_utf8(path.u8string());
}

OperationKind kind_of(const Options& options)
{
    switch (options.command)
    {
    case Command::upload:
        return OperationKind::upload;
    case Command::download:
        return options.choice == FileChoice::named ? OperationKind::download_media : OperationKind::download_operator;
    case Command::find:
    case Command::information:
        break;
    }
    return OperationKind::information;
}

// The loads of the media set that --load names, or all of them. Wrong usage tells why.
std::expected<std::vector<adl::loader_core::LoadChoice>, std::string> choose_loads(const Options& options,
                                                                                   const MediaSetReport& report)
{
    std::vector<adl::loader_core::LoadChoice> choices;
    if (options.all_loads)
    {
        for (const adl::loader_core::LoadReport& load : report.loads)
        {
            choices.push_back({load.header_file, load.part_number});
        }
        if (choices.empty())
        {
            return std::unexpected("The media set holds no load to send.");
        }
        return choices;
    }
    for (const std::string& part_number : options.loads)
    {
        const auto matches = std::ranges::count(report.loads, part_number, &adl::loader_core::LoadReport::part_number);
        if (matches == 0)
        {
            return std::unexpected("The media set has no load with the part number \"" + part_number + "\".");
        }
        if (matches > 1)
        {
            return std::unexpected("The part number \"" + part_number +
                                   "\" names more than one load of the media set.");
        }
        const auto load = std::ranges::find(report.loads, part_number, &adl::loader_core::LoadReport::part_number);
        choices.push_back({load->header_file, load->part_number});
    }
    return choices;
}

// The sentence for a person that tells how an operation that ran ended.
std::string detail_of(Reason reason, const OperationResult& result, const Job& job, const Options& options)
{
    const adl::arinc615a::Outcome& outcome = result.outcome;
    std::string text;
    const auto with_target_text = [&job](std::string sentence)
    {
        if (!job.target_text().empty())
        {
            sentence.insert(sentence.size() - 1, " (" + job.target_text() + ")");
        }
        return sentence;
    };
    switch (reason)
    {
    case Reason::completed:
        if (options.command == Command::upload)
        {
            return "The operation completed. The target read " + count_s(outcome.files_served, "file") + ".";
        }
        if (options.command == Command::download)
        {
            return "The operation completed. Received " + count_s(outcome.files_received, "file") + ".";
        }
        return "The operation completed.";
    case Reason::target_refused:
        return with_target_text(std::string{"The target refused the operation: "} + to_string(outcome.code) + ".");
    case Reason::target_aborted:
        return with_target_text(std::string{"The target ended the operation: "} + to_string(outcome.code) + ".");
    case Reason::load_failed:
    {
        std::string failed;
        for (const adl::loader_core::RecordedLoad& load : job.record().loads)
        {
            if (load.status == adl::arinc615a::StatusCode::item_failed)
            {
                failed += (failed.empty() ? ": " : ", ") + load.part_number;
            }
        }
        return "The upload ended and the target failed " + count_s(outcome.items_failed, "load") + failed + ".";
    }
    case Reason::missing_files:
    {
        // The target listed nothing to take, or none of what was picked.
        if (job.chose_nothing() && job.unlisted().empty())
        {
            return "The target listed no file to download.";
        }
        if (!job.unlisted().empty())
        {
            text = "Received " + std::to_string(outcome.files_received) + " of " +
                   count_s(options.names.size(), "picked file") + ". The target does not list";
            for (std::size_t i = 0; i < job.unlisted().size(); ++i)
            {
                text += (i == 0 ? " " : ", ") + job.unlisted()[i];
            }
            return text + '.';
        }
        text = "Received " + std::to_string(outcome.files_received) + " of " +
               count_s(outcome.files_requested, "requested file") + '.';
        if (outcome.items_failed != 0)
        {
            text += " The target failed " + count_s(outcome.items_failed, "file") + '.';
        }
        return text;
    }
    case Reason::protocol_error:
        return outcome.code == OutcomeCode::unsupported_protocol_version
                   ? "The target's protocol version is not supported."
                   : "What the target sent could not be understood: " + std::string{to_string(outcome.code)} + ".";
    case Reason::aborted:
        return std::string{"The operation was aborted: "} + to_string(outcome.code) + ".";
    case Reason::no_answer:
        return outcome.code == OutcomeCode::transfer_failed
                   ? "The target did not answer: a file transfer timed out."
                   : "The target did not answer: " + std::string{to_string(outcome.code)} + ".";
    case Reason::media_set_changed:
        return "The media set changed after it was checked: " + result.changed_file.value_or("a file") + ".";
    case Reason::local_error:
        if (result.not_run)
        {
            const std::string code = adl::platform::to_string(result.not_run->platform);
            switch (result.not_run->stage)
            {
            case adl::arinc615a::RunError::Stage::start:
                return std::string{"The operation could not run: the request was refused ("} +
                       std::string{adl::arinc615a::to_string(result.not_run->engine)} + ").";
            case adl::arinc615a::RunError::Stage::control_socket:
                return "The operation could not run: the loader's port could not be opened (" + code + ").";
            case adl::arinc615a::RunError::Stage::wait:
                return "The operation could not run: waiting for the network failed (" + code + ").";
            }
        }
        return std::string{"The loader could not carry out the operation: "} + to_string(outcome.code) + ".";
    case Reason::usage:
    case Reason::media_set_unreadable:
        break;
    }
    return {};
}

// What the last line of the log says: the window's "Finished: ...", with the same kind of words.
std::string phrase_of(Reason reason, const OperationResult& result, const Job& job, const Options& options)
{
    const adl::arinc615a::Outcome& outcome = result.outcome;
    switch (reason)
    {
    case Reason::completed:
        return "completed";
    case Reason::load_failed:
        return "completed, " + count_s(outcome.items_failed, "load") + " failed";
    case Reason::missing_files:
        if (job.chose_nothing() && job.unlisted().empty())
        {
            return "the target listed no file to download";
        }
        if (!job.unlisted().empty())
        {
            return "completed, " + count_s(options.names.size() - outcome.files_received, "file") + " missing";
        }
        return outcome.code == OutcomeCode::completed
                   ? "completed, " + count_s(outcome.files_requested - outcome.files_received, "file") + " missing"
                   : std::string{to_string(outcome.code)};
    case Reason::media_set_changed:
        return "the media set changed after it was checked";
    case Reason::local_error:
        return result.not_run ? "the operation could not run" : std::string{to_string(outcome.code)};
    case Reason::no_answer:
        return "the target did not answer";
    case Reason::target_refused:
    case Reason::target_aborted:
    case Reason::protocol_error:
    case Reason::aborted:
    case Reason::usage:
    case Reason::media_set_unreadable:
        break;
    }
    return to_string(outcome.code);
}

class OperationCommand
{
public:
    OperationCommand(adl::platform::Host& host, const Options& options, EventWriter& events, std::ostream& err)
        : host_(host), options_(options), events_(events), err_(err),
          address_(adl::loader_core::format_ipv4(options.target)),
          job_(events, kind_of(options), address_, options.position, adl::loader_core::wall_clock_of(host.clock()))
    {
    }

    [[nodiscard]] int run()
    {
        // What can be wrong with the choice is found before anything is written: that is wrong usage, and no
        // event is written for it.
        if (const std::optional<int> usage = prepare())
        {
            return *usage;
        }
        events_.started(to_string(options_.command), address_, options_.position);

        if (options_.command == Command::upload)
        {
            if (const std::optional<int> ended = check_media_set_events())
            {
                return *ended;
            }
        }
        if (options_.command == Command::download)
        {
            if (const std::optional<int> ended = make_folder())
            {
                return *ended;
            }
        }
        return operate();
    }

private:
    // The plan of the operation; for an upload, the media set read and the loads chosen from it.
    [[nodiscard]] std::optional<int> prepare()
    {
        switch (options_.command)
        {
        case Command::find:
        case Command::information:
            plan_ = std::make_unique<adl::loader_core::OperationPlan>(adl::loader_core::OperationPlan::information());
            return std::nullopt;
        case Command::upload:
        {
            report_ = adl::loader_core::check_media_set(adl::platform::path_from_utf8(options_.media_set), &index_);
            if (!report_.error.empty())
            {
                return std::nullopt; // told as the media set that could not be read, after `started`
            }
            auto choices = choose_loads(options_, report_);
            if (!choices)
            {
                return usage_error(choices.error());
            }
            auto plan = adl::loader_core::OperationPlan::upload(*choices);
            if (!plan)
            {
                return usage_error(plan.error().describe());
            }
            plan_ = std::make_unique<adl::loader_core::OperationPlan>(std::move(*plan));
            return std::nullopt;
        }
        case Command::download:
            if (options_.choice == FileChoice::picked)
            {
                plan_ = std::make_unique<adl::loader_core::OperationPlan>(
                    adl::loader_core::OperationPlan::download_operator());
                return std::nullopt;
            }
            auto plan = adl::loader_core::OperationPlan::download_media(options_.names);
            if (!plan)
            {
                return usage_error(plan.error().describe());
            }
            plan_ = std::make_unique<adl::loader_core::OperationPlan>(std::move(*plan));
            return std::nullopt;
        }
        return std::nullopt;
    }

    [[nodiscard]] int usage_error(const std::string& problem)
    {
        err_ << "adl_load: " << problem << '\n';
        return exit_usage;
    }

    // The media set of an upload, checked: told, or refused.
    [[nodiscard]] std::optional<int> check_media_set_events()
    {
        job_.record().media_set =
            adl::loader_core::RecordedMediaSet{options_.media_set, report_.part_number, report_.members};
        if (!report_.error.empty())
        {
            const std::string text = "The media set could not be read: " + report_.error + ".";
            job_.say(Severity::error, text);
            return finish(Reason::media_set_unreadable, text, "the media set could not be read", {});
        }
        events_.media_set_checked(
            {options_.media_set, report_.part_number, report_.members, report_.loads.size(), report_.findings});
        // What was checked is what is sent: a set edited since is refused here, before the target is contacted,
        // and again as the target opens each file.
        if (const auto changed = index_->changed_file(host_.files()))
        {
            const std::string text = "The media set changed after it was checked: " + *changed + ".";
            job_.say(Severity::error, text);
            return finish(Reason::media_set_changed, text, "the media set changed after it was checked", {});
        }
        return std::nullopt;
    }

    // The folder of a download, made inside the one the operator named.
    [[nodiscard]] std::optional<int> make_folder()
    {
        const auto made = adl::loader_core::make_download_directory(host_.files(), platform_path_of(options_.into),
                                                                    options_.position);
        if (!made)
        {
            const std::string text = "The download folder could not be used: " + made.error().describe() + ".";
            job_.say(Severity::error, text);
            return finish(Reason::local_error, text, "the download folder could not be used", {});
        }
        folder_ = *made;
        job_.set_download_folder(native_path_of(folder_));
        return std::nullopt;
    }

    [[nodiscard]] int operate()
    {
        const std::string where = address_ + " (" + options_.position + ")";
        switch (options_.command)
        {
        case Command::find:
        case Command::information:
            job_.say(Severity::info, "Asking " + where + " for its configuration...");
            break;
        case Command::upload:
            job_.say(Severity::info,
                     "Uploading " + count_s(options_.all_loads ? report_.loads.size() : options_.loads.size(), "load") +
                         " to " + where + "...");
            break;
        case Command::download:
            job_.say(Severity::info,
                     options_.choice == FileChoice::named
                         ? "Downloading " + count_s(options_.names.size(), "file") + " from " + where + "..."
                         : "Asking " + where + " for the files it can download...");
            break;
        }

        adl::loader_core::OperationGate gate;
        if (options_.command == Command::download && options_.choice == FileChoice::picked)
        {
            job_.choose_from_list(gate, options_.pick_all, options_.names);
        }
        std::optional<adl::loader_core::MediaTreeSource> source;
        if (options_.command == Command::upload)
        {
            source.emplace(host_.files(), *index_);
        }
        std::optional<adl::arinc615a::DirectoryFileSink> sink;
        if (options_.command == Command::download)
        {
            sink.emplace(host_.files(), folder_);
        }

        adl::loader_core::RunTarget target;
        target.address = adl::arinc615a::Address{options_.target};
        target.thw_id_pos = options_.position;
        target.control_port = options_.control_port;
        if (options_.local_address)
        {
            target.local_address = adl::platform::Ipv4Address{*options_.local_address};
        }
        adl::loader_core::RunServices services;
        services.listener = &job_;
        services.controller = &gate;
        services.upload_source = source ? &*source : nullptr;
        services.download_sink = sink ? &*sink : nullptr;
        services.download_folder = folder_;

        const OperationResult result = adl::loader_core::run_operation(host_, target, plan_->operation(), services);

        // A download that received nothing leaves no folder behind: it was removed if it was empty.
        std::optional<std::string> kept;
        if (options_.command == Command::download)
        {
            if (result.download_folder_removed)
            {
                job_.say(Severity::info, "Nothing was received, so the empty download folder " +
                                             native_path_of(folder_) + " was removed.");
                job_.record().download_folder.reset();
            }
            else if (!result.download_folder.empty())
            {
                kept = native_path_of(result.download_folder);
            }
        }

        Reason reason = reason_of(result.reason);
        // The files that were picked and that the target does not list, and a list with nothing to take, are
        // files that were asked for and not received: the operation, ended with "none" or completed with the
        // rest, is a failure for them.
        if (options_.command == Command::download && options_.choice == FileChoice::picked &&
            (!job_.unlisted().empty() || job_.chose_nothing()) &&
            (reason == Reason::completed || (reason == Reason::aborted && job_.chose_nothing())))
        {
            reason = Reason::missing_files;
        }
        return finish(reason, detail_of(reason, result, job_, options_), phrase_of(reason, result, job_, options_),
                      kept);
    }

    // Says how it ended: the last line of the log and the `finished` event, and the exit code.
    [[nodiscard]] int finish(Reason reason, std::string detail, const std::string& phrase,
                             const std::optional<std::string>& download_folder)
    {
        job_.say(reason == Reason::completed ? Severity::info : Severity::error, "Finished: " + phrase + ".");
        job_.finish();

        // The record says how it ended, as the event does. Its status code is the recorder's, which includes
        // the target's answer to the request that starts the operation: it is not replaced by the engine's last
        // status, which does not.
        adl::loader_core::OperationRecord& record = job_.record();
        record.program_name = std::string{program_name};
        record.program_version = std::string{program_version};
        record.result = reason == Reason::completed ? "completed" : "failed";
        record.reason = std::string{to_string(reason)};
        record.detail = detail;

        // The record is written whatever the outcome. One that cannot be written is a local error, and says
        // so ahead of what the operation came to, which it keeps in the detail.
        std::optional<std::string> written;
        if (options_.record)
        {
            if (const auto problem =
                    write_text_file(host_.files(), *options_.record, adl::loader_core::record_to_json(record)))
            {
                const std::string lost = "The operation record could not be written: " + *problem + ".";
                job_.say(Severity::error, lost);
                detail = lost + " The operation had ended with the reason " + std::string{to_string(reason)} + ".";
                reason = Reason::local_error;
            }
            else
            {
                written = *options_.record;
            }
        }

        FinishedEvent event;
        event.completed = reason == Reason::completed;
        event.reason = to_string(reason);
        if (const auto status = record.status_code)
        {
            event.status_code = static_cast<std::uint16_t>(*status);
        }
        event.detail = std::move(detail);
        event.exit_code = exit_code_of(reason);
        event.is_download = options_.command == Command::download;
        event.download_folder = download_folder;
        event.has_record = options_.record.has_value();
        event.record = std::move(written);
        events_.finished(event);
        return event.exit_code;
    }

    adl::platform::Host& host_;
    const Options& options_;
    EventWriter& events_;
    std::ostream& err_;
    std::string address_;
    Job job_;
    std::unique_ptr<adl::loader_core::OperationPlan> plan_; // what prepare() made; none for a set that cannot be read
    MediaSetReport report_;
    std::shared_ptr<const MediaSetIndex> index_;
    std::string folder_; // a download: the folder that was made, as a platform path
};

} // namespace

int run_operation_command(adl::platform::Host& host, const Options& options, EventWriter& events, std::ostream& err)
{
    return OperationCommand(host, options, events, err).run();
}

} // namespace adl::load
