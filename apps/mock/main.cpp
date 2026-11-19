// SPDX-License-Identifier: GPL-3.0-or-later
// adl_mock: the mock ARINC 615A target as a command, with no window and no Qt. Everything it does is in
// command.cpp; this is only the call to it.

#include "adl/mock/cli/command.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    return adl::mock::cli::run_command(arguments, std::cout, std::cerr);
}
