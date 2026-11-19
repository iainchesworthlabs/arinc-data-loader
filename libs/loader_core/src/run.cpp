// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/run.hpp"

#include "adl/loader_core/download_directory.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <variant>
#include <vector>

namespace adl::loader_core
{

namespace
{

using adl::arinc615a::OutcomeCode;

[[nodiscard]] Reason reason_of(const adl::arinc615a::Outcome& outcome, bool is_download, bool missing_files)
{
    switch (outcome.code)
    {
    case OutcomeCode::completed:
        if (is_download)
        {
            return missing_files || outcome.items_failed != 0 ? Reason::missing_files : Reason::completed;
        }
        return outcome.items_failed != 0 ? Reason::load_failed : Reason::completed;
    case OutcomeCode::denied:
    case OutcomeCode::not_supported:
        return Reason::target_refused;
    case OutcomeCode::aborted_by_target:
        return Reason::target_aborted;
    case OutcomeCode::aborted_by_operator:
    case OutcomeCode::aborted_by_data_loader:
        return Reason::aborted;
    case OutcomeCode::protocol_error:
    case OutcomeCode::unsupported_protocol_version:
        return Reason::protocol_error;
    case OutcomeCode::dlp_timed_out:
    case OutcomeCode::exception_timer_expired:
        return Reason::no_answer;
    case OutcomeCode::transfer_failed:
        if (outcome.failed_transfer == adl::arinc615a::TransferOutcome::timed_out)
        {
            return Reason::no_answer;
        }
        return outcome.failed_transfer == adl::arinc615a::TransferOutcome::local_error ? Reason::local_error
                                                                                       : Reason::target_aborted;
    case OutcomeCode::local_error:
    case OutcomeCode::running:
        return Reason::local_error;
    }
    return Reason::local_error;
}

// Sets the engine up and runs the operation over the host's sockets: how it ended, or why it did not run.
[[nodiscard]] std::expected<adl::arinc615a::Outcome, adl::arinc615a::RunError>
run_engine(adl::platform::Host& host, const RunTarget& target, const adl::arinc615a::Operation& operation,
           const RunServices& services)
{
    if (target.port == 0)
    {
        // The engine takes the target's port as it is. No datagram can go to port 0, so the request is
        // refused here as the engine refuses a control port of 0: it did not run, at the start.
        return std::unexpected(adl::arinc615a::RunError{});
    }
    adl::arinc615a::LoaderConfig config;
    config.target = adl::arinc615a::Endpoint{target.address, target.port};
    config.thw_id_pos = target.thw_id_pos;
    config.control_port = target.control_port;

    // The engine allocates nothing: it works in a block of memory we give it, and the loader object
    // itself is too big for a thread's stack to be a good place for it.
    std::vector<std::byte> workspace(adl::arinc615a::DataLoader::workspace_bytes(config));
    adl::arinc615a::LoaderServices loader_services;
    loader_services.listener = services.listener;
    loader_services.source = services.upload_source;
    loader_services.sink = services.download_sink;
    auto loader = std::make_unique<adl::arinc615a::DataLoader>(config, workspace, loader_services);

    adl::arinc615a::RunOptions options;
    options.controller = services.controller;
    if (target.local_address)
    {
        options.local_address = *target.local_address;
    }
    else if (target.address.is_loopback())
    {
        // A target on this machine is reached over loopback alone, so the sockets need not listen on
        // the network, and then no firewall asks whether to allow the program.
        options.local_address = adl::platform::Ipv4Address::loopback();
    }
    adl::arinc615a::Runner runner(host);
    return runner.run(*loader, operation, options);
}

} // namespace

OperationResult run_operation(adl::platform::Host& host, const RunTarget& target,
                              const adl::arinc615a::Operation& operation, const RunServices& services)
{
    const auto outcome = run_engine(host, target, operation, services);

    OperationResult result;
    // A download that received nothing (the target said no, never answered, was cancelled, or the loader
    // could not run at all) leaves no folder behind: the one made for it is removed if it is empty, and
    // left alone if anything is in it.
    if (!services.download_folder.empty())
    {
        result.download_folder_removed = remove_if_empty(host.files(), services.download_folder);
        if (!result.download_folder_removed)
        {
            result.download_folder = std::string{services.download_folder};
        }
    }
    if (!outcome)
    {
        result.not_run = outcome.error();
        return result;
    }

    result.outcome = *outcome;
    const bool is_download = std::holds_alternative<adl::arinc615a::MediaDownloadOperation>(operation) ||
                             std::holds_alternative<adl::arinc615a::OperatorDownloadOperation>(operation);
    // A download that completed without every asked-for file is as much a failure as one the target
    // reported as failed: the operator asked for files and has not got them all.
    result.missing_files =
        is_download && outcome->code == OutcomeCode::completed && outcome->files_received < outcome->files_requested;
    result.reason = reason_of(*outcome, is_download, result.missing_files);
    if (services.upload_source != nullptr && services.upload_source->changed_file())
    {
        // The target was refused a file that is not what was checked; that, and not what the target made
        // of the refusal, is why the upload did not complete.
        result.changed_file = *services.upload_source->changed_file();
        result.reason = Reason::media_set_changed;
    }
    return result;
}

} // namespace adl::loader_core
