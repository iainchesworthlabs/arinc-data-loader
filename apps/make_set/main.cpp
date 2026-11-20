// SPDX-License-Identifier: GPL-3.0-or-later
// adl_make_set: makes a media set from a description file, verifies a media set folder, and says what one
// holds, with no window and no Qt. Everything it does is in command.cpp; this is only the call to it.

#include "adl/make_set/command.hpp"
#include "adl/platform/arguments.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    // Not argv: on Windows that has lost any character outside the code page (docs/architecture.md,
    // "Command-line programs"). The first element is the program's name, which run_command does not take.
    const std::vector<std::string> command_line = adl::platform::arguments(argc, argv);
    std::vector<std::string_view> arguments;
    for (std::size_t i = 1; i < command_line.size(); ++i)
    {
        arguments.emplace_back(command_line[i]);
    }
    return adl::make_set::run_command(arguments, std::cout, std::cerr);
}
