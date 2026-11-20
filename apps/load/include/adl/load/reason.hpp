// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Why the program ended as it did: the `reason` of the `finished` event and of the operation record, one
// word for a program to read, and the exit code that goes with it (docs/load-command.md has the table).
// The reasons of adl::loader_core::run_operation are the same words; the program adds the ones that are its
// own: wrong usage, and a media set that could not be read.

#include "adl/load/exit_codes.hpp"
#include "adl/loader_core/run.hpp"

#include <cstdint>
#include <string_view>

namespace adl::load
{

enum class Reason : std::uint8_t
{
    completed,            // the operation completed, and for an upload every load did
    target_refused,       // the target refused the operation
    target_aborted,       // the target ended the operation
    load_failed,          // the upload ended and the target failed at least one load
    missing_files,        // the download ended with fewer files than were asked for, or a file failed
    protocol_error,       // what the target sent could not be understood, or its protocol version is not supported
    aborted,              // the operation was aborted from this side
    usage,                // wrong options
    media_set_unreadable, // the media set could not be read or checked
    media_set_changed,    // a file changed after the set was checked
    no_answer,            // the target did not answer
    local_error           // a port could not be opened, a folder could not be made, or the record could not be written
};

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
    case Reason::protocol_error:
        return "protocol_error";
    case Reason::aborted:
        return "aborted";
    case Reason::usage:
        return "usage";
    case Reason::media_set_unreadable:
        return "media_set_unreadable";
    case Reason::media_set_changed:
        return "media_set_changed";
    case Reason::no_answer:
        return "no_answer";
    case Reason::local_error:
        return "local_error";
    }
    return "local_error";
}

[[nodiscard]] constexpr int exit_code_of(Reason reason) noexcept
{
    switch (reason)
    {
    case Reason::completed:
        return exit_completed;
    case Reason::target_refused:
    case Reason::target_aborted:
    case Reason::load_failed:
    case Reason::missing_files:
    case Reason::protocol_error:
    case Reason::aborted:
        return exit_failed;
    case Reason::usage:
        return exit_usage;
    case Reason::media_set_unreadable:
    case Reason::media_set_changed:
        return exit_media_set;
    case Reason::no_answer:
        return exit_no_answer;
    case Reason::local_error:
        return exit_local_error;
    }
    return exit_local_error;
}

// The reason of an operation that ran.
[[nodiscard]] constexpr Reason reason_of(adl::loader_core::Reason reason) noexcept
{
    switch (reason)
    {
    case adl::loader_core::Reason::completed:
        return Reason::completed;
    case adl::loader_core::Reason::target_refused:
        return Reason::target_refused;
    case adl::loader_core::Reason::target_aborted:
        return Reason::target_aborted;
    case adl::loader_core::Reason::load_failed:
        return Reason::load_failed;
    case adl::loader_core::Reason::missing_files:
        return Reason::missing_files;
    case adl::loader_core::Reason::protocol_error:
        return Reason::protocol_error;
    case adl::loader_core::Reason::aborted:
        return Reason::aborted;
    case adl::loader_core::Reason::media_set_changed:
        return Reason::media_set_changed;
    case adl::loader_core::Reason::no_answer:
        return Reason::no_answer;
    case adl::loader_core::Reason::local_error:
        return Reason::local_error;
    }
    return Reason::local_error;
}

} // namespace adl::load
