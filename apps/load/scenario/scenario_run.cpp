// SPDX-License-Identifier: GPL-3.0-or-later
#include "scenario_run.hpp"

#include "adl/loader_core/address.hpp"
#include "adl/loader_scenario/reader.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "adl/scenario/envelope.hpp"

#include <optional>
#include <span>
#include <utility>

namespace adl::load
{

namespace
{

using adl::loader_scenario::Action;
using adl::loader_scenario::FaultEvent;

std::string cannot_read(adl::platform::ErrorCode code)
{
    return "the file cannot be read (" + std::string{adl::platform::to_string(code)} + ')';
}

// "address:port".
std::string endpoint_text(const adl::arinc615a::Endpoint& endpoint)
{
    return adl::loader_core::format_ipv4(endpoint.address.octets) + ':' + std::to_string(endpoint.port);
}

} // namespace

std::expected<adl::loader_scenario::Scenario, std::string> read_scenario_file(adl::platform::FileSystem& files,
                                                                              const std::string& path)
{
    const std::string platform_path = adl::platform::to_platform_path(adl::platform::path_from_utf8(path));
    auto opened = files.open_read(platform_path);
    if (!opened)
    {
        return std::unexpected(cannot_read(opened.error().code));
    }
    adl::platform::ReadFile& file = **opened;
    const auto size = file.size();
    if (!size)
    {
        return std::unexpected(cannot_read(size.error().code));
    }
    if (*size > adl::scenario::max_file_size)
    {
        return std::unexpected("the file is larger than " + std::to_string(adl::scenario::max_file_size) + " bytes");
    }
    std::string text(static_cast<std::size_t>(*size), '\0');
    std::size_t have = 0;
    while (have < text.size())
    {
        const auto got = file.read_at(have, std::as_writable_bytes(std::span{text}.subspan(have)));
        if (!got)
        {
            return std::unexpected(cannot_read(got.error().code));
        }
        if (*got == 0)
        {
            break; // the file is shorter than it was a moment ago
        }
        have += *got;
    }
    text.resize(have);
    auto scenario = adl::loader_scenario::read_scenario(text);
    if (!scenario)
    {
        return std::unexpected(std::move(scenario.error().message));
    }
    return std::move(*scenario);
}

ScenarioRun::ScenarioRun(adl::loader_scenario::Scenario scenario, std::string path,
                         adl::loader_scenario::OperationName operation, EventWriter& events)
    : path_(std::move(path)), steps_(scenario.steps.size()), seed_(scenario.seed), events_(events),
      player_(std::move(scenario), operation, this)
{
}

ScenarioInfo ScenarioRun::info() const
{
    return ScenarioInfo{path_, steps_, seed_};
}

std::vector<std::size_t> ScenarioRun::unused() const
{
    return player_.unapplied_steps();
}

void ScenarioRun::on_fault(const FaultEvent& fault) noexcept
{
    ScenarioStepEvent event;
    event.step = fault.step;
    event.trigger = to_string(fault.trigger);
    event.at = fault.at;
    event.operation = to_string(fault.operation);
    event.action = to_string(fault.action);
    if (fault.datagram)
    {
        event.datagram = to_string(*fault.datagram);
        event.file = fault.file;
        event.block = fault.block;
        if (fault.to)
        {
            event.to = endpoint_text(*fault.to);
        }
    }
    switch (fault.action)
    {
    case Action::duplicate:
        event.parameter = "copies";
        event.value = fault.amount;
        break;
    case Action::delay:
        event.parameter = "ms";
        event.value = fault.amount;
        break;
    case Action::reorder:
        event.parameter = "within_ms";
        event.value = fault.amount;
        break;
    case Action::silent:
        // A silence of no length lasts the rest of the operation.
        event.parameter = "for_ms";
        if (fault.amount > 0)
        {
            event.value = fault.amount;
        }
        break;
    case Action::length:
        event.parameter = "delta";
        event.value = fault.amount;
        break;
    case Action::error:
        event.parameter = "code";
        event.value = fault.amount;
        break;
    case Action::wait:
        event.parameter = "seconds";
        event.value = fault.amount;
        break;
    case Action::abort_message:
    case Action::abort:
        event.parameter = "status";
        event.value = fault.amount;
        break;
    case Action::drop:
    case Action::corrupt:
    case Action::check_value:
        break;
    }
    events_.scenario_step(event);
}

} // namespace adl::load
