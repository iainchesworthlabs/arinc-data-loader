// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What an operator sets before the mock target starts, in plain values, and how it becomes the
// engine's Config. The window's model fills it from its properties; a program with no window fills
// it from its command line. Both go through make_config, so the two cannot drift apart.

#include "adl/mock/engine.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace adl::mock
{

struct TargetSettings
{
    // ---- identity ----
    std::string hardware_id = "HNPFMS";
    std::string type_name = "FMS";
    std::string position = "L";
    std::string literal_name = "FMS LEFT";
    std::string manufacturer_code = "HNP";

    // ---- the network: ports from 1 to 65535, checked by whoever fills this in ----
    std::uint16_t tftp_port = 59;
    std::uint16_t find_port = 1001;
    bool loopback_only = false; // listen on 127.0.0.1 only, not on every interface

    // ---- how it works ----
    int think_time_ms = 1000;
    int status_period_sec = 4;

    // ---- the injections (all default to "behave") ----
    int wait_first_seconds = 0;
    bool wait_every_request = false;
    std::optional<OperationKind> refuse_operation;
    // With which status the refused operation is refused: 0x1000 (denied) or 0x1002 (not
    // supported). The window has no control for it, and always refuses with 0x1000.
    adl::arinc615a::StatusCode refuse_code = adl::arinc615a::StatusCode::operation_denied;
    // Serve an initialization file of an invalid structure. The window has no control for it.
    bool invalid_init_file = false;
    int drop_status_every = 0;
    bool corrupt_served_file = false;
    bool wrong_checksum = false;
    int abort_after_statuses = 0;
    bool silent = false;
};

// The engine's configuration for `settings`. The identity gives THW_ID_POS (the hardware ID, then
// '_' and the position if there is one) and the FIND answer; a count below zero is no count; the
// WAIT time is held to what a WAIT message can carry.
// The data (the parts reported, the files held) is not here: the caller sets Config::holds.
[[nodiscard]] Config make_config(const TargetSettings& settings);

} // namespace adl::mock
