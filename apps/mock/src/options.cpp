// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/cli/options.hpp"

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc665/portable_name.hpp"
#include "adl/platform/arguments.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <set>
#include <system_error>

namespace adl::mock::cli
{

namespace
{

using adl::arinc615a::StatusCode;

struct OptionSpec
{
    std::string_view name;
    bool takes_value;
    bool repeatable;
};

constexpr std::array<OptionSpec, 30> known_options{{
    {"--hardware-id", true, false},
    {"--position", true, false},
    {"--type-name", true, false},
    {"--literal-name", true, false},
    {"--manufacturer", true, false},
    {"--part", true, true},
    {"--holds", true, false},
    {"--storage", true, false},
    {"--address", true, false},
    {"--tftp-port", true, false},
    {"--find-port", true, false},
    {"--think-time-ms", true, false},
    {"--status-period", true, false},
    {"--wait-first", true, false},
    {"--wait-every", true, false},
    {"--refuse", true, false},
    {"--refuse-code", true, false},
    {"--invalid-init-file", false, false},
    {"--drop-status-every", true, false},
    {"--corrupt-served", false, false},
    {"--wrong-checksum", false, false},
    {"--abort-after", true, false},
    {"--silent", false, false},
    {"--scenario", true, false},
    {"--scenario-actions", false, false},
    {"--operations", true, false},
    {"--seconds", true, false},
    {"--json", false, false},
    {"--help", false, false},
    {"--version", false, false},
}};

[[nodiscard]] const OptionSpec* find_option(std::string_view name) noexcept
{
    const auto found = std::ranges::find(known_options, name, &OptionSpec::name);
    return found == known_options.end() ? nullptr : &*found;
}

[[nodiscard]] std::unexpected<UsageError> fail(std::string message)
{
    return std::unexpected(UsageError{std::move(message)});
}

// A whole number from `text`, nothing but digits, in [low, high].
[[nodiscard]] std::optional<std::uint64_t> parse_number(std::string_view text, std::uint64_t low, std::uint64_t high)
{
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() || value < low || value > high)
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool is_printable(std::string_view text) noexcept
{
    return std::ranges::all_of(text, [](char c) { return c >= 0x20 && c <= 0x7E; });
}

// A name that is part of a file name (THW_ID_POS is the stem of the protocol files): letters, digits and . _ -
[[nodiscard]] bool is_file_stem(std::string_view text) noexcept
{
    return std::ranges::all_of(text,
                               [](char c)
                               {
                                   return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                          c == '.' || c == '_' || c == '-';
                               });
}

// Why `value` cannot be the text of `option`, or nothing if it can: between `low` and `high` printable
// ASCII characters.
[[nodiscard]] std::optional<UsageError> check_text(std::string_view option, std::string_view value, std::size_t low,
                                                   std::size_t high)
{
    if (value.size() < low || value.size() > high || !is_printable(value))
    {
        const std::string count =
            low == high ? std::to_string(high) : std::to_string(low) + " to " + std::to_string(high);
        return UsageError{std::string{option} + " takes " + count + " printable ASCII characters"};
    }
    return std::nullopt;
}

[[nodiscard]] std::string number_range(std::uint64_t low, std::uint64_t high)
{
    return "a whole number from " + std::to_string(low) + " to " + std::to_string(high);
}

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> fields;
    while (true)
    {
        const std::size_t at = text.find(separator);
        fields.push_back(text.substr(0, at));
        if (at == std::string_view::npos)
        {
            return fields;
        }
        text.remove_prefix(at + 1);
    }
}

// Adds one --part to the list: <hardware>:<part number>[:<amendment>[:<designation>]]. The hardware is
// found by its name (the parts of one piece of hardware are given one at a time), the serial number is
// made up (SN-0001 for the first piece), and a part with no designation is designated by its hardware.
[[nodiscard]] std::optional<UsageError> add_part(std::string_view value, std::vector<HardwareItem>& parts)
{
    const std::vector<std::string_view> fields = split(value, ':');
    if (fields.size() < 2 || fields.size() > 4)
    {
        return UsageError{"--part takes <hardware>:<part number>[:<amendment>[:<designation>]]"};
    }
    const std::size_t most = adl::arinc615a::max_text_chars;
    if (const auto error = check_text("the hardware of --part", fields[0], 1, most))
    {
        return error;
    }
    if (const auto error = check_text("the part number of --part", fields[1], 1, most))
    {
        return error;
    }
    const std::string_view amendment = fields.size() > 2 ? fields[2] : std::string_view{};
    if (const auto error = check_text("the amendment of --part", amendment, 0, most))
    {
        return error;
    }
    const std::string_view designation = fields.size() > 3 ? fields[3] : fields[0];
    if (const auto error = check_text("the designation of --part", designation, 1, most))
    {
        return error;
    }
    auto item = std::ranges::find(parts, fields[0], &HardwareItem::literal_name);
    if (item == parts.end())
    {
        std::string serial = std::to_string(parts.size() + 1);
        serial.insert(0, serial.size() < 4 ? 4 - serial.size() : 0, '0');
        parts.push_back(HardwareItem{std::string{fields[0]}, "SN-" + serial, {}});
        item = std::prev(parts.end());
    }
    item->parts.push_back(HardwarePart{std::string{fields[1]}, std::string{amendment}, std::string{designation}});
    return std::nullopt;
}

} // namespace

std::optional<adl::arinc615a::Address> parse_ipv4(std::string_view text)
{
    const std::vector<std::string_view> fields = split(text, '.');
    if (fields.size() != 4)
    {
        return std::nullopt;
    }
    adl::arinc615a::Address address;
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto octet = parse_number(fields[i], 0, 255);
        if (!octet || fields[i].size() > 3 || (fields[i].size() > 1 && fields[i].front() == '0'))
        {
            return std::nullopt;
        }
        address.octets[i] = static_cast<std::uint8_t>(*octet);
    }
    return address;
}

std::string format_ipv4(const adl::arinc615a::Address& address)
{
    std::string text;
    for (std::size_t i = 0; i < 4; ++i)
    {
        text += i == 0 ? "" : ".";
        text += std::to_string(address.octets[i]);
    }
    return text;
}

std::expected<Options, UsageError> parse_options(std::span<const std::string_view> arguments)
{
    using adl::arinc615a::find_max_hardware_id;
    using adl::arinc615a::find_max_literal_name;
    using adl::arinc615a::find_max_manufacturer_code;
    using adl::arinc615a::find_max_position;
    using adl::arinc615a::find_max_type_name;

    Options options;
    TargetSettings& settings = options.settings;
    std::set<std::string_view> seen;
    std::optional<std::string_view> refuse_code;
    bool wait_first = false;
    bool wait_every = false;

    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string_view name = arguments[i];
        const OptionSpec* const spec = find_option(name);
        if (spec == nullptr)
        {
            return fail(name.starts_with("--") ? "unknown option " + std::string{name}
                                               : "unexpected argument " + std::string{name});
        }
        if (!spec->repeatable && !seen.insert(name).second)
        {
            return fail(std::string{name} + " is given twice");
        }
        std::string_view value;
        if (spec->takes_value)
        {
            if (i + 1 >= arguments.size())
            {
                return fail(std::string{name} + " needs a value");
            }
            value = arguments[++i];
        }

        // The numbers: one place that says what each takes.
        const auto number = [&name, &value](std::uint64_t low,
                                            std::uint64_t high) -> std::expected<std::uint64_t, UsageError>
        {
            if (const auto parsed = parse_number(value, low, high))
            {
                return *parsed;
            }
            return fail(std::string{name} + " takes " + number_range(low, high));
        };

        if (name == "--help")
        {
            options.action = Action::help;
            return options;
        }
        if (name == "--version")
        {
            options.action = Action::version;
            return options;
        }
        if (name == "--scenario-actions")
        {
            options.action = Action::scenario_actions;
            return options;
        }
        if (name == "--hardware-id" || name == "--position")
        {
            const bool is_position = name == "--position";
            if (const auto error = check_text(name, value, is_position ? 0 : 1,
                                              is_position ? find_max_position : find_max_hardware_id))
            {
                return std::unexpected(*error);
            }
            if (!is_file_stem(value))
            {
                return fail(std::string{name} + " takes letters, digits, '.', '_' and '-' (it is part of file names)");
            }
            (is_position ? settings.position : settings.hardware_id) = std::string{value};
        }
        else if (name == "--type-name")
        {
            if (const auto error = check_text(name, value, 1, find_max_type_name))
            {
                return std::unexpected(*error);
            }
            settings.type_name = std::string{value};
        }
        else if (name == "--literal-name")
        {
            if (const auto error = check_text(name, value, 1, find_max_literal_name))
            {
                return std::unexpected(*error);
            }
            settings.literal_name = std::string{value};
        }
        else if (name == "--manufacturer")
        {
            if (const auto error = check_text(name, value, 1, find_max_manufacturer_code))
            {
                return std::unexpected(*error);
            }
            settings.manufacturer_code = std::string{value};
        }
        else if (name == "--part")
        {
            if (const auto error = add_part(value, options.parts))
            {
                return std::unexpected(*error);
            }
        }
        else if (name == "--holds" || name == "--storage" || name == "--scenario")
        {
            if (value.empty())
            {
                return fail(std::string{name} + (name == "--scenario" ? " needs a file" : " needs a folder"));
            }
            (name == "--holds"     ? options.holds
             : name == "--storage" ? options.storage
                                   : options.scenario) = adl::platform::path_from_utf8(value);
        }
        else if (name == "--address")
        {
            const auto address = parse_ipv4(value);
            if (!address)
            {
                return fail("--address takes an IPv4 address such as 127.0.0.1");
            }
            options.address = *address;
        }
        else if (name == "--tftp-port" || name == "--find-port")
        {
            const auto port = number(1, 65535);
            if (!port)
            {
                return std::unexpected(port.error());
            }
            (name == "--tftp-port" ? settings.tftp_port : settings.find_port) = static_cast<std::uint16_t>(*port);
        }
        else if (name == "--think-time-ms" || name == "--status-period")
        {
            const bool think = name == "--think-time-ms";
            const auto count = number(0, think ? 60000 : 3600);
            if (!count)
            {
                return std::unexpected(count.error());
            }
            (think ? settings.think_time_ms : settings.status_period_sec) = static_cast<int>(*count);
        }
        else if (name == "--wait-first" || name == "--wait-every")
        {
            const auto seconds = number(1, adl::arinc615a::max_wait_seconds);
            if (!seconds)
            {
                return std::unexpected(seconds.error());
            }
            settings.wait_first_seconds = static_cast<int>(*seconds);
            wait_first = wait_first || name == "--wait-first";
            wait_every = wait_every || name == "--wait-every";
        }
        else if (name == "--refuse")
        {
            if (value == "information")
            {
                settings.refuse_operation = OperationKind::information;
            }
            else if (value == "upload")
            {
                settings.refuse_operation = OperationKind::upload;
            }
            else if (value == "download-media")
            {
                settings.refuse_operation = OperationKind::download_media;
            }
            else if (value == "download-operator")
            {
                settings.refuse_operation = OperationKind::download_operator;
            }
            else
            {
                return fail("--refuse takes information, upload, download-media or download-operator");
            }
        }
        else if (name == "--refuse-code")
        {
            if (value != "0x1000" && value != "0x1002")
            {
                return fail("--refuse-code takes 0x1000 or 0x1002");
            }
            refuse_code = value;
            settings.refuse_code =
                value == "0x1002" ? StatusCode::operation_not_supported : StatusCode::operation_denied;
        }
        else if (name == "--invalid-init-file")
        {
            settings.invalid_init_file = true;
        }
        else if (name == "--drop-status-every" || name == "--abort-after")
        {
            const auto count = number(1, 1'000'000);
            if (!count)
            {
                return std::unexpected(count.error());
            }
            (name == "--abort-after" ? settings.abort_after_statuses : settings.drop_status_every) =
                static_cast<int>(*count);
        }
        else if (name == "--corrupt-served")
        {
            settings.corrupt_served_file = true;
        }
        else if (name == "--wrong-checksum")
        {
            settings.wrong_checksum = true;
        }
        else if (name == "--silent")
        {
            settings.silent = true;
        }
        else if (name == "--operations" || name == "--seconds")
        {
            const auto count = number(1, name == "--seconds" ? 31'536'000 : 1'000'000'000);
            if (!count)
            {
                return std::unexpected(count.error());
            }
            (name == "--seconds" ? options.seconds : options.operations) = *count;
        }
        else if (name == "--json")
        {
            options.json = true;
        }
    }

    if (options.scenario)
    {
        // The scenario says which fault happens when, so the options that set one for the whole run are not
        // taken with it (they would have to be added to the scenario, and which wins would be a rule to learn).
        for (const std::string_view fault :
             {"--wait-first", "--wait-every", "--refuse", "--refuse-code", "--invalid-init-file", "--drop-status-every",
              "--corrupt-served", "--wrong-checksum", "--abort-after", "--silent"})
        {
            if (seen.contains(fault))
            {
                return fail("--scenario cannot be used with " + std::string{fault} + ": the scenario sets the faults");
            }
        }
    }
    if (wait_first && wait_every)
    {
        return fail("--wait-first and --wait-every cannot both be given");
    }
    settings.wait_every_request = wait_every;
    if (refuse_code && !settings.refuse_operation)
    {
        return fail("--refuse-code needs --refuse");
    }
    if (settings.silent && !options.seconds)
    {
        // The operation of a silent target never ends, so --operations would never be reached.
        return fail("--silent needs --seconds, because the operation never ends");
    }
    const std::string thw = settings.hardware_id + (settings.position.empty() ? "" : "_" + settings.position);
    if (!adl::arinc665::is_portable_file_name(thw))
    {
        return fail("the hardware ID and position make " + thw +
                    ", which is not a name every system can use for a file");
    }
    return options;
}

std::vector<std::string> fault_options(const Options& options)
{
    const TargetSettings& settings = options.settings;
    std::vector<std::string> faults;
    if (settings.wait_first_seconds > 0)
    {
        faults.emplace_back(settings.wait_every_request ? "--wait-every" : "--wait-first");
    }
    if (settings.refuse_operation)
    {
        faults.emplace_back("--refuse");
        if (settings.refuse_code != StatusCode::operation_denied)
        {
            faults.emplace_back("--refuse-code");
        }
    }
    if (settings.invalid_init_file)
    {
        faults.emplace_back("--invalid-init-file");
    }
    if (settings.drop_status_every > 0)
    {
        faults.emplace_back("--drop-status-every");
    }
    if (settings.corrupt_served_file)
    {
        faults.emplace_back("--corrupt-served");
    }
    if (settings.wrong_checksum)
    {
        faults.emplace_back("--wrong-checksum");
    }
    if (settings.abort_after_statuses > 0)
    {
        faults.emplace_back("--abort-after");
    }
    if (settings.silent)
    {
        faults.emplace_back("--silent");
    }
    if (options.scenario)
    {
        faults.emplace_back("--scenario");
    }
    return faults;
}

std::string usage_text()
{
    return "usage: adl_mock [options]\n"
           "\n"
           "A simulated ARINC 615A target, with no window, for test rigs and CI jobs. It is not a real\n"
           "target. It listens until it has served --operations operations or --seconds have passed, or\n"
           "until it is killed.\n"
           "\n"
           "identity\n"
           "  --hardware-id <text>      THW_ID, at most 15 characters (default HNPFMS)\n"
           "  --position <text>         POS, at most 8 characters; THW_ID_POS is the two joined by _ (default L)\n"
           "  --type-name <text>        type name in the FIND answer, at most 8 (default FMS)\n"
           "  --literal-name <text>     literal name in the FIND answer, at most 20 (default \"FMS LEFT\")\n"
           "  --manufacturer <text>     manufacturer code in the FIND answer, at most 3 (default HNP)\n"
           "  --part <hardware>:<part number>[:<amendment>[:<designation>]]\n"
           "                            a part to report in the configuration, repeated for more\n"
           "\n"
           "files\n"
           "  --holds <folder>          the files it holds for a download (default: one demonstration file)\n"
           "  --storage <folder>        where uploads are stored (default: served and not kept)\n"
           "\n"
           "network\n"
           "  --address <ipv4>          the local address (default 127.0.0.1; 0.0.0.0 is every interface)\n"
           "  --tftp-port <n>           (default 59)\n"
           "  --find-port <n>           (default 1001)\n"
           "\n"
           "pace\n"
           "  --think-time-ms <n>       0 to 60000 (default 1000)\n"
           "  --status-period <seconds> 0 to 3600 (default 4)\n"
           "\n"
           "faults (all off by default)\n"
           "  --wait-first <seconds>    answer the first request with WAIT\n"
           "  --wait-every <seconds>    answer every request with WAIT\n"
           "  --refuse <operation>      refuse information, upload, download-media or download-operator\n"
           "  --refuse-code <code>      refuse with 0x1000 (default) or 0x1002; needs --refuse\n"
           "  --invalid-init-file       serve an initialization file of an invalid structure\n"
           "  --drop-status-every <n>   lose every nth status file\n"
           "  --corrupt-served          flip a bit in the files it serves\n"
           "  --wrong-checksum          write a wrong check value option\n"
           "  --abort-after <n>         abort (0x1003) after n status files\n"
           "  --silent                  say nothing after the first status file; needs --seconds\n"
           "  --scenario <file>         take the faults from a scenario file, which says which happens when\n"
           "                            (instead of the fault options above); see the guide\n"
           "  --scenario-actions        print the actions a scenario file may use, one for each line, and exit\n"
           "\n"
           "stopping\n"
           "  --operations <n>          stop after n operations (a refused one counts)\n"
           "  --seconds <n>             stop after n seconds\n"
           "\n"
           "output\n"
           "  --json                    one JSON object a line, for a program to read\n"
           "  --help, --version\n"
           "\n"
           "exit codes: 0 stopped as asked, 1 the run failed, 2 wrong usage, 3 a folder cannot be used,\n"
           "4 a port cannot be opened, 5 a scenario file cannot be used\n";
}

} // namespace adl::mock::cli
