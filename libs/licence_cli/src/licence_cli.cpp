// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence_cli/licence_cli.hpp"

#include "commands.hpp"
#include "messages.hpp"
#include "text.hpp"

#include <exception>
#include <ostream>
#include <string>
#include <string_view>

namespace adl::licence_cli
{

namespace
{

// A command line that cannot be used: the reason, then the usage, on the error output.
[[nodiscard]] int usage_error(const Console& console, const std::string& sentence)
{
    console.err << "adl_licence: " << sentence << '\n' << message::usage;
    return exit_usage;
}

[[nodiscard]] int dispatch(std::span<const std::string> arguments, const Environment& environment,
                           const Console& console)
{
    if (arguments.empty())
    {
        return usage_error(console, std::string(message::no_command));
    }
    const std::string_view command = arguments[0];
    if (command == "--help" || command == "--version")
    {
        if (arguments.size() != 1)
        {
            return usage_error(console, format(message::help_alone, {command}));
        }
        if (command == "--help")
        {
            console.out << message::usage;
        }
        else
        {
            console.out << "adl_licence " << (environment.version.empty() ? "unknown" : plain(environment.version))
                        << '\n';
        }
        return exit_ok;
    }
    if (command.starts_with("--"))
    {
        return usage_error(console, format(message::unknown_option, {plain(command)}));
    }

    const bool known = command == "show" || command == "import" || command == "remove" || command == "verify";
    if (!known)
    {
        return usage_error(console, format(message::unknown_command, {plain(command)}));
    }
    // An argument that starts with "--" is an option, and no command has one.
    for (std::size_t i = 1; i < arguments.size(); ++i)
    {
        if (std::string_view{arguments[i]}.starts_with("--"))
        {
            return usage_error(console, format(message::unknown_option, {plain(arguments[i])}));
        }
    }
    if (command == "show")
    {
        if (arguments.size() != 1)
        {
            return usage_error(console, format(message::takes_no_argument, {command}));
        }
        return command_show(environment, console);
    }
    if (arguments.size() != 2)
    {
        return usage_error(console, command == "remove" ? std::string(message::needs_one_name)
                                                        : format(message::needs_one_file, {command}));
    }
    if (command == "import")
    {
        return command_import(environment, arguments[1], console);
    }
    if (command == "remove")
    {
        return command_remove(environment, arguments[1], console);
    }
    return command_verify(environment, arguments[1], console);
}

} // namespace

std::string usage_text()
{
    return std::string(message::usage);
}

int run(std::span<const std::string> arguments, const Environment& environment, const Console& console)
{
    int code = exit_failed;
    try
    {
        code = dispatch(arguments, environment, console);
    }
    catch (const std::exception& error)
    {
        // Nothing here throws on purpose: this is the net under a failed allocation.
        console.err << "adl_licence: " << format(message::unexpected_failure, {plain(error.what())}) << '\n';
        code = exit_failed;
    }
    console.out.flush();
    console.err.flush();
    return code;
}

} // namespace adl::licence_cli
