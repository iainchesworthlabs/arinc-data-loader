// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "owner.hpp"

#include "commands.hpp"
#include "messages.hpp"

#include <exception>
#include <ostream>
#include <string_view>

namespace adl::licence_owner
{

std::string usage_text()
{
    return std::string(message::usage);
}

int run(std::span<const std::string> arguments, const Console& console)
{
    try
    {
        if (arguments.empty())
        {
            return usage_error(console, message::no_command);
        }
        const std::string_view command = arguments.front();
        const std::span<const std::string> rest = arguments.subspan(1);
        if (command == "--help" || command == "-h" || command == "help")
        {
            console.out << usage_text();
            return exit_ok;
        }
        if (command == "inspect")
        {
            return inspect_command(rest, console);
        }
        if (command == "verify")
        {
            return verify_command(rest, console);
        }
        if (command == "issue")
        {
            return issue_command(rest, console);
        }
        if (command == "revocations")
        {
            return revocations_command(rest, console);
        }
        if (command == "keygen")
        {
            return keygen_command(rest, console);
        }
        return usage_error(console, format(message::unknown_command, {command}));
    }
    catch (const std::exception& error)
    {
        // The standard library reports an exhausted memory or a file system it cannot use by throwing.
        // Nothing here is expected to, and nothing leaves the program but an exit code.
        return fail(console, format(message::unexpected_failure, {error.what()}));
    }
}

} // namespace adl::licence_owner
