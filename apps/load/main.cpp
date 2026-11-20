// SPDX-License-Identifier: GPL-3.0-or-later
// adl_load: one ARINC 615A operation on one target, with no window. This is only the start: the options,
// the events and the run function are in the library beside it (apps/load/src), so that the tests can run
// a whole command on the fake platform.
//
// The arguments are UTF-8 text from adl::platform::arguments, and main never reads argv itself: on Windows
// argv is the command line in the system's code page, where a folder named with Japanese characters has already
// become question marks (docs/architecture.md, "Command-line programs").

#include "adl/load/exit_codes.hpp"
#include "adl/load/run.hpp"
#include "adl/load/system_licence.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/host.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    const std::vector<std::string> command_line = adl::platform::arguments(argc, argv);

    auto host = adl::platform::make_host();
    if (!host)
    {
        std::cerr << "adl_load: the system could not be set up for the network ("
                  << adl::platform::to_string(host.error().code) << ").\n";
        return adl::load::exit_local_error;
    }
    // The program's name is the first of them, and is not an argument.
    std::vector<std::string_view> arguments;
    for (std::size_t i = 1; i < command_line.size(); ++i)
    {
        arguments.emplace_back(command_line[i]);
    }
    // `serve` reads standard input on a thread of its own while the loop writes to standard output: the reading
    // must not flush the output as a tied stream does, from the other thread, in the middle of an event.
    std::cin.tie(nullptr);
    // The licence goes into the record of an operation, and nowhere else: nothing here depends on it.
    const int code = adl::load::run(**host, arguments, std::cin, std::cout, std::cerr, adl::load::system_licence());
    if (!arguments.empty() && arguments.front() == "serve")
    {
        // The thread that reads standard input may be blocked in a read that nothing can end: the program was told to
        // stop and the pipe it reads is still open. Leaving by `return` would wait for it on some systems, where the C
        // library takes the lock of standard input to flush it as it ends the process, and the lock is that thread's.
        // Everything has been written and flushed, and the host is let go, so the process ends here and now.
        host->reset();
        std::cout.flush();
        std::cerr.flush();
        std::quick_exit(code);
    }
    return code;
}
