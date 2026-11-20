// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the fuzzer of adl_mock's command line and the test that replays its seeds share: the fuzzer's
// bytes as the arguments of a command line, and what must hold of whatever the parser makes of them.
// The input is the arguments with a line feed after each one (a blank line is an empty argument), so
// that the seeds are text a person can read.

#include "adl/mock/cli/options.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace adl::test
{

// The text between the line feeds of the input; what follows the last one is an argument only if it is
// not empty.
[[nodiscard]] inline std::vector<std::string_view> fuzz_arguments(const std::uint8_t* data, std::size_t size)
{
    std::vector<std::string_view> arguments;
    const std::string_view text{size == 0 ? "" : reinterpret_cast<const char*>(data), size};
    for (std::size_t start = 0; start < text.size();)
    {
        const std::size_t end = text.find('\n', start);
        arguments.push_back(text.substr(start, end == std::string_view::npos ? end : end - start));
        start = end == std::string_view::npos ? text.size() : end + 1;
    }
    return arguments;
}

// Parses the input as a command line and checks what must hold: an error says why; options that were
// accepted are inside the limits the usage text states, and do not leave out what another option needs;
// and an address that is read is written back as it was. False when something does not hold.
[[nodiscard]] inline bool fuzz_mock_options(const std::uint8_t* data, std::size_t size)
{
    using namespace adl::mock::cli;
    const std::vector<std::string_view> arguments = fuzz_arguments(data, size);
    bool holds = true;
    for (const std::string_view argument : arguments)
    {
        if (const auto address = parse_ipv4(argument))
        {
            holds = holds && format_ipv4(*address) == argument;
        }
    }

    const auto parsed = parse_options(arguments);
    if (!parsed)
    {
        return holds && !parsed.error().message.empty();
    }
    if (parsed->action != Action::run)
    {
        return holds;
    }
    const Options& options = *parsed;
    const auto& settings = options.settings;
    holds = holds && settings.tftp_port >= 1 && settings.find_port >= 1;
    holds = holds && !settings.hardware_id.empty() && settings.hardware_id.size() <= 15;
    holds = holds && settings.position.size() <= 8;
    holds = holds && !settings.type_name.empty() && settings.type_name.size() <= 8;
    holds = holds && !settings.literal_name.empty() && settings.literal_name.size() <= 20;
    holds = holds && !settings.manufacturer_code.empty() && settings.manufacturer_code.size() <= 3;
    holds = holds && settings.think_time_ms >= 0 && settings.think_time_ms <= 60000;
    holds = holds && settings.status_period_sec >= 0 && settings.status_period_sec <= 3600;
    holds = holds && settings.wait_first_seconds >= 0 && settings.wait_first_seconds <= 65535;
    holds = holds && settings.drop_status_every >= 0 && settings.drop_status_every <= 1'000'000;
    holds = holds && settings.abort_after_statuses >= 0 && settings.abort_after_statuses <= 1'000'000;
    holds = holds && (!settings.silent || options.seconds.has_value());
    holds = holds && (settings.refuse_code == adl::arinc615a::StatusCode::operation_denied ||
                      settings.refuse_operation.has_value());
    holds = holds && (!settings.wait_every_request || settings.wait_first_seconds > 0);
    holds = holds && (!options.operations || *options.operations >= 1);
    holds = holds && (!options.seconds || *options.seconds >= 1);
    for (const adl::mock::HardwareItem& item : options.parts)
    {
        holds = holds && !item.literal_name.empty() && !item.serial_number.empty() && !item.parts.empty();
        for (const adl::mock::HardwarePart& part : item.parts)
        {
            holds = holds && !part.part_number.empty() && part.part_number.size() <= 254 &&
                    part.amendment.size() <= 254 && !part.designation.empty() && part.designation.size() <= 254;
        }
    }
    static_cast<void>(fault_options(options));
    return holds;
}

} // namespace adl::test
