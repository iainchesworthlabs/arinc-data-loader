// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The command line of adl_load: the four commands, their options, and what a command line has to be to be
// accepted. The parser is a function of the arguments alone (it reads no file and asks the system nothing),
// so that any text can be given to it, which is what its fuzz harness does. Options are written
// `--name value`; an option that takes several values is given several times; a value that begins with `--`
// is not a value, it is the next option, so a folder or a name that does begin that way is written
// `./--name`.

#include "adl/loader_core/address.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::load
{

enum class Command : std::uint8_t
{
    find,
    information,
    upload,
    download
};

// "find", "information", "upload", "download": the word the command is typed as and the `command` of the
// `started` event.
[[nodiscard]] constexpr std::string_view to_string(Command command) noexcept
{
    switch (command)
    {
    case Command::find:
        return "find";
    case Command::information:
        return "information";
    case Command::upload:
        return "upload";
    case Command::download:
        return "download";
    }
    return "find";
}

// How a download says which files it wants.
enum class FileChoice : std::uint8_t
{
    named, // --file: the loader names the files (the media defined download)
    picked // --pick or --pick-all: the target lists its files and the command chooses from the list
};

// The longest FIND window, in seconds.
inline constexpr unsigned max_find_seconds = 3600;

// The window's three seconds.
inline constexpr unsigned default_find_seconds = 3;

// A command line that was accepted.
struct Options
{
    Command command = Command::find;
    bool json = false;

    // The interface the sockets are bound to. Empty: what the window does, the loopback address for a target
    // on this computer and every interface otherwise.
    std::optional<adl::loader_core::Ipv4Octets> local_address;

    // find: how long to listen.
    unsigned seconds = default_find_seconds;

    // An operation: where it goes. `position` is the target's THW_ID_POS.
    adl::loader_core::Ipv4Octets target{};
    std::string position;
    std::uint16_t control_port = 59;
    // The port the target takes the request that starts the operation on: the standard's 59, unless it is a simulated
    // target that cannot have it.
    std::uint16_t target_port = 59;
    // The file the operation record is written to, as given.
    std::optional<std::string> record;

    // upload: the folder of the media set as given, and the part numbers of the loads to send, or all of them.
    std::string media_set;
    std::vector<std::string> loads;
    bool all_loads = false;

    // download: the folder the files go into (a folder of its own is made inside it), and the files.
    std::string into;
    FileChoice choice = FileChoice::named;
    std::vector<std::string> names; // --file or --pick, in the order given
    bool pick_all = false;

    friend bool operator==(const Options&, const Options&) = default;
};

// What the command line asks for.
enum class Action : std::uint8_t
{
    run,    // the command in `options`
    help,   // print the usage and exit 0
    version // print the version line and exit 0
};

struct Parsed
{
    Action action = Action::run;
    Options options; // meaningful for Action::run
};

// Reads the arguments (not the program's name). `--help` and `--version` are taken as soon as they are met,
// before the arguments after them are looked at. On wrong usage the error is one English sentence that
// names the argument at fault.
[[nodiscard]] std::expected<Parsed, std::string> parse_options(std::span<const std::string_view> arguments);

// The usage text, the one that `--help` prints: the commands, their options and what they mean.
[[nodiscard]] std::string_view usage_text() noexcept;

// "adl_load 0.1.1 events 1".
[[nodiscard]] std::string version_line();

} // namespace adl::load
