// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/settings.hpp"

#include "adl/arinc615a/limits.hpp"

#include <algorithm>
#include <chrono>

namespace adl::mock
{

Config make_config(const TargetSettings& settings)
{
    Config config;
    config.thw = settings.hardware_id + (settings.position.empty() ? std::string{} : "_" + settings.position);
    config.find_identity = FindIdentity{settings.hardware_id, settings.type_name, settings.position,
                                        settings.literal_name, settings.manufacturer_code};
    config.tftp_port = settings.tftp_port;
    config.find_port = settings.find_port;
    if (settings.loopback_only)
    {
        config.bind_address = adl::arinc615a::Address::loopback();
    }
    config.think_time = std::chrono::milliseconds(settings.think_time_ms);
    config.status_period = std::chrono::seconds(settings.status_period_sec);
    config.wait_first_seconds = static_cast<std::uint16_t>(
        std::clamp(settings.wait_first_seconds, 0, static_cast<int>(adl::arinc615a::max_wait_seconds)));
    config.wait_always = settings.wait_every_request;
    config.drop_status_every = static_cast<unsigned>(std::max(settings.drop_status_every, 0));
    config.corrupt_served = settings.corrupt_served_file;
    config.wrong_checksum = settings.wrong_checksum;
    if (settings.abort_after_statuses > 0)
    {
        config.abort_after_statuses = static_cast<std::size_t>(settings.abort_after_statuses);
    }
    config.silent = settings.silent;
    if (settings.refuse_operation)
    {
        Refusal refusal;
        refusal.kind = *settings.refuse_operation;
        refusal.code = adl::arinc615a::StatusCode::operation_denied;
        refusal.text = "refused by the mock target";
        config.refusal = std::move(refusal);
    }
    return config;
}

} // namespace adl::mock
