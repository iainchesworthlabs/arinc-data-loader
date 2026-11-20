// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The entry point where the C runtime hands the program its command line as UTF-16 (Windows). `wmain` is
// called with the wide command line, so no operating system header is needed and the code page is never
// involved: each argument is converted to UTF-8 here, and everything after this point works in UTF-8. CMake
// compiles this file and not os/posix/entry.cpp, chosen by the platform's name (tools/licence/CMakeLists.txt).

#include "messages.hpp"
#include "owner.hpp"
#include "utf8.hpp"

#include <iostream>
#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        std::u16string wide;
        for (const wchar_t* unit = argv[i]; *unit != L'\0'; ++unit)
        {
            wide.push_back(static_cast<char16_t>(*unit));
        }
        auto text = adl::licence_owner::utf16_to_utf8(wide);
        if (!text)
        {
            std::cerr << "adl_licence_owner: " << adl::licence_owner::message::bad_argument << '\n'
                      << adl::licence_owner::message::help_hint << '\n';
            return adl::licence_owner::exit_usage;
        }
        arguments.push_back(std::move(*text));
    }
    return adl::licence_owner::run(arguments, {std::cout, std::cerr});
}
