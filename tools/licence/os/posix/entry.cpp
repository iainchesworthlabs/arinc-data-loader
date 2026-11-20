// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The entry point where the C runtime hands the program its command line as bytes (Linux and macOS). They are
// the text the shell gave, which is UTF-8 on every system the programs run on, and they are passed on as they
// are.

#include "owner.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }
    return adl::licence_owner::run(arguments, {std::cout, std::cerr});
}
