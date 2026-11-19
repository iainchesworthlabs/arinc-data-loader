// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Running one operation against one target, and what became of it. run_operation sets the engine up the
// way the loader's window does (the workspace it works in, the loopback rule for the address the sockets
// are bound to), runs it over the host's sockets until it has an outcome, clears away an empty download
// folder, and reduces the outcome to a result a program can act on: succeeded or failed, one reason out of
// a short list, the last status code the target gave, and the folder a download kept.
//
// It blocks until the operation has ended, so it belongs on a thread of its own when something else has
// to go on; the operator's abort and file choice reach it through the controller (gate.hpp). It does the
// part of an operation that has no wording of its own. What goes before it (the check that the media set
// is as it was, the making of the download folder) and what is said about the result are the caller's.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/loader_core/media_source.hpp"
#include "adl/platform/host.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace adl::loader_core
{

// Why an operation ended as it did. The words are the ones a command prints and a record keeps.
enum class Reason : std::uint8_t
{
    completed,         // it completed, and for an upload every load did
    target_refused,    // the target refused the operation, or does not do it or does not speak its protocol
    target_aborted,    // the target ended the operation, or ended it at the loader's request (an operator's
                       // abort: the outcome's code says which), or sent what the protocol does not allow
    load_failed,       // an upload ended and the target failed at least one load
    missing_files,     // a download ended with fewer files than were asked for, or a file the target failed
    media_set_changed, // a file of the media set changed after the set was checked, and was not sent
    no_answer,         // the target did not answer in time
    local_error,       // the loader could not run it: a port, the files, the request itself
};

// "completed", "target_refused", "target_aborted", "load_failed", "missing_files", "media_set_changed",
// "no_answer", "local_error".
[[nodiscard]] constexpr std::string_view to_string(Reason reason) noexcept
{
    switch (reason)
    {
    case Reason::completed:
        return "completed";
    case Reason::target_refused:
        return "target_refused";
    case Reason::target_aborted:
        return "target_aborted";
    case Reason::load_failed:
        return "load_failed";
    case Reason::missing_files:
        return "missing_files";
    case Reason::media_set_changed:
        return "media_set_changed";
    case Reason::no_answer:
        return "no_answer";
    case Reason::local_error:
        return "local_error";
    }
    return "local_error";
}

// Where the operation goes.
struct RunTarget
{
    adl::arinc615a::Address address;
    std::string_view thw_id_pos; // as adl::arinc615a::is_valid_thw_id_pos takes it; must outlive the call
    // The port the loader listens on. Anything but 59 is offered to the target in the port option of its
    // request, which matters when the target runs on the same machine.
    std::uint16_t control_port = adl::arinc615a::tftp_control_port;
    // The interface the sockets are bound to. Empty: the window's rule, the loopback address for a target
    // on this machine (so no firewall asks about the program) and every interface for any other.
    std::optional<adl::platform::Ipv4Address> local_address;
};

// What the operation reads, writes and is told by. All of it is optional and must outlive the call.
struct RunServices
{
    adl::arinc615a::OperationListener* listener = nullptr;
    // Polled between waits: the operator's abort and file choice (gate.hpp).
    adl::arinc615a::RunController* controller = nullptr;
    // An upload: where the load's files come from. Asked afterwards whether it refused a file because
    // it was not what the set was when it was checked.
    MediaTreeSource* upload_source = nullptr;
    // A download: where the target's files are written, and the folder that is (a platform path, UTF-8,
    // '/' separated). A download that received nothing leaves no folder behind: it is removed if it is
    // empty, and left alone if anything is in it.
    adl::arinc615a::FileSink* download_sink = nullptr;
    std::string_view download_folder;
};

struct OperationResult
{
    Reason reason = Reason::local_error;
    // Set when the operation could not run at all (reason local_error): the request was refused, the port
    // could not be opened, or waiting for the network failed. `outcome` is then empty.
    std::optional<adl::arinc615a::RunError> not_run;
    // How the engine says it ended, with the counts of status files, files and failed items.
    adl::arinc615a::Outcome outcome;
    // A download that completed with fewer files than were asked for.
    bool missing_files = false;
    // An upload that was stopped because a file of the media set is no longer what was checked: how
    // MediaSetIndex::Entry::describe words that file.
    std::optional<std::string> changed_file;
    // A download: the folder that was kept, or empty when there is none (nothing was made, or nothing was
    // received and it was removed). `download_folder_removed` says it was removed.
    std::string download_folder;
    bool download_folder_removed = false;

    // The result of the operation: it completed (reason completed) or failed (any other).
    [[nodiscard]] bool success() const noexcept
    {
        return reason == Reason::completed;
    }
    // The last status code the target gave, if any came.
    [[nodiscard]] std::optional<adl::arinc615a::StatusCode> status_code() const noexcept
    {
        return not_run ? std::nullopt : outcome.last_status;
    }
};

// Runs `operation` against `target`. How its outcome becomes a reason:
//
//   completed                                           completed, or for an upload with failed loads
//                                                       load_failed, for a download that is short of files
//                                                       or has a failed one missing_files
//   denied, not_supported, unsupported_protocol_version target_refused
//   aborted_by_target, aborted_by_data_loader,
//   aborted_by_operator, protocol_error                 target_aborted
//   dlp_timed_out, exception_timer_expired              no_answer
//   transfer_failed                                     no_answer if the last attempt timed out,
//                                                       local_error if the files failed here,
//                                                       target_aborted otherwise
//   local_error, and an operation that could not run    local_error
//
// A media set that changed (the upload source refused a file) is media_set_changed whatever the outcome.
[[nodiscard]] OperationResult run_operation(adl::platform::Host& host, const RunTarget& target,
                                            const adl::arinc615a::Operation& operation, const RunServices& services);

} // namespace adl::loader_core
