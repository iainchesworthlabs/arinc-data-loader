// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/options.hpp"

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/load/program.hpp"
#include "adl/loader_core/download_directory.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <cstddef>
#include <system_error>
#include <utility>

namespace adl::load
{

namespace
{

enum class Id : std::uint8_t
{
    seconds,
    local_address,
    json,
    target,
    position,
    control_port,
    target_port,
    record,
    media_set,
    load,
    all_loads,
    into,
    file,
    pick,
    pick_all,
    option_count
};

constexpr unsigned bit_of(Command command) noexcept
{
    return 1U << static_cast<unsigned>(command);
}

constexpr unsigned every_command =
    bit_of(Command::find) | bit_of(Command::information) | bit_of(Command::upload) | bit_of(Command::download);
constexpr unsigned operations = bit_of(Command::information) | bit_of(Command::upload) | bit_of(Command::download);

struct OptionSpec
{
    std::string_view name;
    Id id;
    bool takes_value;
    bool repeatable;
    unsigned commands; // the commands that have the option
};

constexpr std::array<OptionSpec, static_cast<std::size_t>(Id::option_count)> option_specs{{
    {"--seconds", Id::seconds, true, false, bit_of(Command::find)},
    {"--local-address", Id::local_address, true, false, every_command},
    {"--json", Id::json, false, false, every_command},
    {"--target", Id::target, true, false, operations},
    {"--position", Id::position, true, false, operations},
    {"--control-port", Id::control_port, true, false, operations},
    {"--target-port", Id::target_port, true, false, operations},
    {"--record", Id::record, true, false, operations},
    {"--media-set", Id::media_set, true, false, bit_of(Command::upload)},
    {"--load", Id::load, true, true, bit_of(Command::upload)},
    {"--all-loads", Id::all_loads, false, false, bit_of(Command::upload)},
    {"--into", Id::into, true, false, bit_of(Command::download)},
    {"--file", Id::file, true, true, bit_of(Command::download)},
    {"--pick", Id::pick, true, true, bit_of(Command::download)},
    {"--pick-all", Id::pick_all, false, false, bit_of(Command::download)},
}};

const OptionSpec* find_spec(std::string_view name) noexcept
{
    const auto found = std::ranges::find(option_specs, name, &OptionSpec::name);
    return found == option_specs.end() ? nullptr : &*found;
}

std::optional<Command> command_of(std::string_view word) noexcept
{
    for (const Command command : {Command::find, Command::information, Command::upload, Command::download})
    {
        if (word == to_string(command))
        {
            return command;
        }
    }
    return std::nullopt;
}

std::string in_quotes(std::string_view text)
{
    return '"' + std::string{text} + '"';
}

// A whole number written in digits, from `low` to `high`.
std::optional<unsigned> whole_number(std::string_view text, unsigned low, unsigned high) noexcept
{
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < low || value > high)
    {
        return std::nullopt;
    }
    return value;
}

// The reading of one option's value into the options. Returns the complaint, or nothing.
class Reader
{
public:
    explicit Reader(Options& options) : options_(options) {}

    [[nodiscard]] std::optional<std::string> apply(const OptionSpec& spec, std::string_view value)
    {
        switch (spec.id)
        {
        case Id::seconds:
            return number(spec, value, 1, max_find_seconds, options_.seconds);
        case Id::local_address:
            return address(spec, value, options_.local_address);
        case Id::json:
            options_.json = true;
            return std::nullopt;
        case Id::target:
            return address(spec, value, options_.target);
        case Id::position:
            options_.position = std::string{value};
            return std::nullopt;
        case Id::control_port:
            return port(spec, value, options_.control_port);
        case Id::target_port:
            return port(spec, value, options_.target_port);
        case Id::record:
            options_.record = std::string{value};
            return std::nullopt;
        case Id::media_set:
            options_.media_set = std::string{value};
            return std::nullopt;
        case Id::load:
            return add_name(spec, value, options_.loads);
        case Id::all_loads:
            options_.all_loads = true;
            return std::nullopt;
        case Id::into:
            options_.into = std::string{value};
            return std::nullopt;
        case Id::file:
            options_.choice = FileChoice::named;
            return add_name(spec, value, options_.names);
        case Id::pick:
            options_.choice = FileChoice::picked;
            return add_name(spec, value, options_.names);
        case Id::pick_all:
            options_.choice = FileChoice::picked;
            options_.pick_all = true;
            return std::nullopt;
        case Id::option_count:
            break;
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] static std::optional<std::string> number(const OptionSpec& spec, std::string_view value, unsigned low,
                                                           unsigned high, unsigned& into)
    {
        if (const auto parsed = whole_number(value, low, high))
        {
            into = *parsed;
            return std::nullopt;
        }
        return std::string{spec.name} + " takes a whole number from " + std::to_string(low) + " to " +
               std::to_string(high) + ", not " + in_quotes(value) + ".";
    }

    [[nodiscard]] static std::optional<std::string> port(const OptionSpec& spec, std::string_view value,
                                                         std::uint16_t& into)
    {
        unsigned parsed = 0;
        if (auto problem = number(spec, value, 1, 65535, parsed))
        {
            return problem;
        }
        into = static_cast<std::uint16_t>(parsed);
        return std::nullopt;
    }

    template <class Into>
    [[nodiscard]] static std::optional<std::string> address(const OptionSpec& spec, std::string_view value, Into& into)
    {
        if (const auto octets = adl::loader_core::parse_ipv4(value))
        {
            into = *octets;
            return std::nullopt;
        }
        return std::string{spec.name} + " takes an IPv4 address (four numbers of 0 to 255 separated by dots), not " +
               in_quotes(value) + ".";
    }

    [[nodiscard]] static std::optional<std::string> add_name(const OptionSpec& spec, std::string_view value,
                                                             std::vector<std::string>& names)
    {
        if (std::ranges::find(names, value) != names.end())
        {
            return std::string{spec.name} + ' ' + in_quotes(value) + " is given twice.";
        }
        names.emplace_back(value);
        return std::nullopt;
    }

    Options& options_;
};

// What a command line has to have as a whole, once every option has been read.
std::optional<std::string> check_whole(const Options& options, const std::bitset<option_specs.size()>& seen)
{
    const auto has = [&seen](Id id) { return seen.test(static_cast<std::size_t>(id)); };
    if (options.command == Command::find)
    {
        return std::nullopt;
    }
    if (!has(Id::target))
    {
        return "--target is missing.";
    }
    if (!has(Id::position))
    {
        return "--position is missing.";
    }
    const bool position_fits = options.command == Command::download
                                   ? adl::loader_core::can_download_from(options.position)
                                   : adl::arinc615a::is_valid_thw_id_pos(options.position);
    if (!position_fits)
    {
        return "--position " + in_quotes(options.position) +
               (options.command == Command::download
                    ? " is not a THW_ID_POS a download can be made for: 1 to 240 characters, none of them a control "
                      "character, a slash, a backslash or a character a folder name cannot hold."
                    : " is not a THW_ID_POS: 1 to 240 characters, none of them a control character, a slash or a "
                      "backslash.");
    }
    if (options.command == Command::upload)
    {
        if (!has(Id::media_set))
        {
            return "--media-set is missing.";
        }
        if (options.all_loads == !options.loads.empty())
        {
            return options.all_loads ? "--load and --all-loads cannot be given together."
                                     : "Name the loads to send with --load, or give --all-loads.";
        }
    }
    if (options.command == Command::download)
    {
        if (!has(Id::into))
        {
            return "--into is missing.";
        }
        const bool files = has(Id::file);
        const bool picks = has(Id::pick);
        const bool all = has(Id::pick_all);
        if (!files && !picks && !all)
        {
            return "Name the files to download with --file or --pick, or give --pick-all.";
        }
        if (static_cast<int>(files) + static_cast<int>(picks) + static_cast<int>(all) > 1)
        {
            return "--file, --pick and --pick-all cannot be given together.";
        }
    }
    return std::nullopt;
}

constexpr std::string_view usage = R"(usage:
  adl_load find         [--seconds <n>] [--local-address <ipv4>] [--json]
  adl_load information  --target <ipv4> --position <THW_ID_POS> [common]
  adl_load upload       --target <ipv4> --position <THW_ID_POS> --media-set <folder>
                        (--load <part number>... | --all-loads) [common]
  adl_load download     --target <ipv4> --position <THW_ID_POS> --into <folder>
                        (--file <name>... | --pick <name>... | --pick-all) [common]
  adl_load --help
  adl_load --version

common: [--local-address <ipv4>] [--control-port <n>] [--target-port <n>] [--json] [--record <file>]

Runs one ARINC 615A operation on one target, or looks for targets, with no window.
Options are written "--name value"; an option that takes several values is given
several times (--load A --load B).

  find          Broadcasts a FIND request and lists the targets that answer
                (--seconds, default 3, is how long it listens).
  information   Asks the target for its configuration.
  upload        Reads and checks the media set, then sends the loads: --load names
                them by part number, --all-loads takes every load of the set.
  download      Fetches files into a new folder inside --into. --file names the files;
                --pick and --pick-all let the target list its files first and choose
                from the list (--pick-all takes all of them).

  --json        Write one JSON object per line on standard output, one for each event,
                instead of a line for a person to read.
  --record      Write the operation record, a JSON file, when the operation ends.
  --control-port, --target-port, --local-address
                The port the loader listens on (default 59), the port the target
                listens on for the request that starts an operation (default 59;
                another is for a simulated target that cannot have 59) and the
                interface the sockets use (default: the loopback address for a
                target on this computer, every interface otherwise).

Exit codes: 0 completed, 1 the operation ran and failed, 2 wrong usage, 3 the media set
could not be read or changed after it was checked, 4 the target did not answer, 5 a
local error (a port, a folder or the record file).
The text is English. Killing the program abandons the operation and the target times
out by itself.
)";

} // namespace

std::expected<Parsed, std::string> parse_options(std::span<const std::string_view> arguments)
{
    if (arguments.empty())
    {
        return std::unexpected("No command given: find, information, upload or download.");
    }
    if (arguments.front() == "--help")
    {
        return Parsed{Action::help, {}};
    }
    if (arguments.front() == "--version")
    {
        return Parsed{Action::version, {}};
    }
    const std::optional<Command> command = command_of(arguments.front());
    if (!command)
    {
        return std::unexpected(arguments.front().starts_with("--")
                                   ? "The first argument has to be a command (find, information, upload or "
                                     "download), not " +
                                         in_quotes(arguments.front()) + "."
                                   : "Unknown command " + in_quotes(arguments.front()) +
                                         ": find, information, upload or download.");
    }

    Parsed parsed;
    parsed.options.command = *command;
    Reader reader(parsed.options);
    std::bitset<option_specs.size()> seen;

    for (std::size_t i = 1; i < arguments.size(); ++i)
    {
        const std::string_view argument = arguments[i];
        if (argument == "--help")
        {
            return Parsed{Action::help, {}};
        }
        if (argument == "--version")
        {
            return Parsed{Action::version, {}};
        }
        const OptionSpec* spec = find_spec(argument);
        if (spec == nullptr)
        {
            if (!argument.starts_with("--"))
            {
                return std::unexpected("Unexpected argument " + in_quotes(argument) +
                                       ": a value belongs to the option before it.");
            }
            const std::size_t equals = argument.find('=');
            return std::unexpected("Unknown option " + in_quotes(argument) +
                                   (equals == std::string_view::npos
                                        ? "."
                                        : " (write " + std::string{argument.substr(0, equals)} + ' ' +
                                              std::string{argument.substr(equals + 1)} + ", with a space)."));
        }
        if ((spec->commands & bit_of(*command)) == 0)
        {
            return std::unexpected(std::string{spec->name} + " is not an option of " +
                                   std::string{to_string(*command)} + ".");
        }
        const std::size_t index = static_cast<std::size_t>(spec->id);
        if (seen.test(index) && !spec->repeatable)
        {
            return std::unexpected(std::string{spec->name} + " is given twice.");
        }
        seen.set(index);

        std::string_view value;
        if (spec->takes_value)
        {
            if (i + 1 >= arguments.size() || arguments[i + 1].starts_with("--") || arguments[i + 1].empty())
            {
                return std::unexpected(std::string{spec->name} + " needs a value.");
            }
            value = arguments[++i];
        }
        if (auto problem = reader.apply(*spec, value))
        {
            return std::unexpected(std::move(*problem));
        }
    }

    if (auto problem = check_whole(parsed.options, seen))
    {
        return std::unexpected(std::move(*problem));
    }
    return parsed;
}

std::string_view usage_text() noexcept
{
    return usage;
}

std::string version_line()
{
    return std::string{program_name} + ' ' + std::string{program_version} + " events " + std::to_string(events_format);
}

} // namespace adl::load
