// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/cli/command.hpp"

#include "adl/mock/cli/events.hpp"
#include "adl/mock/cli/options.hpp"
#include "adl/mock/cli/version.hpp"
#include "adl/mock/engine.hpp"
#include "adl/mock/link.hpp"
#include "adl/mock/settings.hpp"
#include "adl/mock/storage.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/paths.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <utility>

namespace adl::mock::cli
{

namespace
{

constexpr std::string_view program_name = "adl_mock";

// A path as UTF-8 text for a message. (path::string() is the text in the system's code page on Windows,
// and throws for a name that has a character outside it.)
[[nodiscard]] std::string utf8(const std::filesystem::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

// What the engine reports, as events. It runs on the thread that runs the engine, inside its loop, and
// writes each event as it happens. It also counts the operations and stops the run when it is asked to.
class Reporter final : public EngineListener
{
public:
    Reporter(std::ostream& out, std::ostream& err, bool json, TargetLink& link,
             std::optional<std::filesystem::path> storage, std::optional<std::uint64_t> operation_limit)
        : out_(out), err_(err), json_(json), link_(link), origin_(link.now()), storage_(std::move(storage)),
          operation_limit_(operation_limit)
    {
    }

    // The engine's own log lines are not events: what the target did is told by the events, and a
    // line for every status file would bury them.
    void log(Severity /*severity*/, std::string_view /*text*/) override {}

    void operation_changed(std::string_view name) override
    {
        current_operation_ = operation_id(name);
        if (!current_operation_.empty())
        {
            emit(OperationBegan{current_operation_});
        }
    }

    void progress_changed(int percent) override
    {
        if (percent >= 0 && !current_operation_.empty())
        {
            emit(Progress{current_operation_, percent});
        }
    }

    void upload_received(const UploadRecord& record) override
    {
        std::optional<std::string> stored_in;
        if (storage_)
        {
            const StoreOutcome outcome = store_upload(*storage_, record);
            for (const StoreIssue& issue : outcome.issues)
            {
                err_ << program_name << ": " << describe(issue) << '\n';
            }
            if (!outcome.stored.empty())
            {
                stored_in = utf8(*storage_ / storage_directory_name(record.load_part_number));
            }
        }
        std::vector<std::string> names;
        names.reserve(record.files.size());
        for (const StoredFile& file : record.files)
        {
            names.push_back(file.name);
        }
        emit(UploadReceived{record.load_part_number, std::move(names), std::move(stored_in)});
    }

    void operation_ended(std::string_view name, std::optional<adl::arinc615a::StatusCode> status, bool refused) override
    {
        emit(OperationEnded{operation_id(name),
                            status ? std::optional<std::uint16_t>{static_cast<std::uint16_t>(*status)} : std::nullopt,
                            std::string{status ? status_name(*status) : invalid_initialization_file}, refused});
        ++operations_;
        if (operation_limit_ && operations_ >= *operation_limit_)
        {
            stop("operations");
        }
    }

    // Asks the run to end, for `reason` (operations or seconds). The first reason is the one kept.
    void stop(std::string_view reason)
    {
        if (stop_reason_.empty())
        {
            stop_reason_ = reason;
        }
        link_.request_stop();
    }

    void emit(EventData data)
    {
        const Event event{std::chrono::duration_cast<std::chrono::milliseconds>(link_.now() - origin_),
                          std::move(data)};
        out_ << (json_ ? to_json_line(event) : to_plain_line(event)) << '\n';
        out_.flush(); // a reader sees each event when it happens
    }

    [[nodiscard]] const std::string& stop_reason() const noexcept
    {
        return stop_reason_;
    }
    [[nodiscard]] std::uint64_t operations() const noexcept
    {
        return operations_;
    }

private:
    [[nodiscard]] static std::string describe(const StoreIssue& issue)
    {
        switch (issue.problem)
        {
        case StoreProblem::no_storage_folder:
            return "there is no storage folder";
        case StoreProblem::storage_folder_failed:
            return "the storage folder could not be made: " + issue.detail;
        case StoreProblem::load_folder_failed:
            return "the load's folder could not be made: " + issue.detail;
        case StoreProblem::unsafe_file_name:
            return issue.subject + " was not stored: not a safe file name";
        case StoreProblem::file_write_failed:
            return issue.subject + " could not be written to the storage folder";
        }
        return "an upload could not be stored";
    }

    std::ostream& out_;
    std::ostream& err_;
    bool json_;
    TargetLink& link_;
    adl::arinc615a::Instant origin_;
    std::optional<std::filesystem::path> storage_;
    std::optional<std::uint64_t> operation_limit_;
    std::string current_operation_;
    std::string stop_reason_;
    std::uint64_t operations_ = 0;
};

} // namespace

int run_command(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err)
{
    const auto parsed = parse_options(arguments);
    if (!parsed)
    {
        err << program_name << ": " << parsed.error().message << "\ntry 'adl_mock --help'\n";
        return exit_usage;
    }
    const Options& options = *parsed;
    if (options.action == Action::help)
    {
        out << usage_text();
        return exit_ok;
    }
    if (options.action == Action::version)
    {
        out << program_name << ' ' << version << " events " << events_format << '\n';
        return exit_ok;
    }

    Config config = make_config(options.settings);
    config.bind_address = options.address;
    if (!options.parts.empty())
    {
        if (!encode_configuration(options.parts))
        {
            err << program_name << ": the parts given with --part do not fit in a configuration list\n";
            return exit_usage;
        }
        config.information = options.parts;
    }

    // The folders, before anything is opened.
    if (options.holds)
    {
        auto held = read_held_files(*options.holds);
        if (!held)
        {
            err << program_name << ": --holds " << utf8(*options.holds) << ": " << held.error() << '\n';
            return exit_folder;
        }
        config.holds = std::move(*held);
    }
    if (config.holds.empty())
    {
        config.holds.push_back(demonstration_file("the folder given with --holds"));
    }
    if (options.storage)
    {
        std::error_code error;
        std::filesystem::create_directories(*options.storage, error);
        if (!error && !std::filesystem::is_directory(*options.storage, error))
        {
            error = std::make_error_code(std::errc::not_a_directory);
        }
        if (error)
        {
            err << program_name << ": --storage " << utf8(*options.storage) << ": " << error.message() << '\n';
            return exit_folder;
        }
    }

    auto host = adl::platform::make_host();
    if (!host)
    {
        err << program_name << ": the platform could not start (" << adl::platform::to_string(host.error().code)
            << ")\n";
        return exit_failed;
    }
    RealTargetLink link(std::move(*host));
    Reporter reporter(out, err, options.json, link, options.storage, options.operations);
    MockEngine engine(&reporter);
    if (const auto error = engine.start(link, config))
    {
        err << program_name << ": " << *error << '\n';
        return exit_port;
    }

    reporter.emit(Started{std::string{program_name}, std::string{version}, config.thw, format_ipv4(options.address),
                          config.find_port, config.tftp_port, fault_options(options)});
    if (options.seconds)
    {
        link.after(std::chrono::seconds(static_cast<std::chrono::seconds::rep>(*options.seconds)),
                   [&reporter] { reporter.stop("seconds"); });
    }
    engine.run();

    std::string reason = reporter.stop_reason();
    int code = exit_ok;
    if (reason.empty())
    {
        // The loop ended without being asked to: waiting on the network failed.
        reason = "error";
        code = exit_failed;
        err << program_name << ": " << engine.last_error() << '\n';
    }
    reporter.emit(Stopped{reason, reporter.operations()});
    return code;
}

} // namespace adl::mock::cli
