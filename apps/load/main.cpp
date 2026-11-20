// SPDX-License-Identifier: GPL-3.0-or-later
// adl_load: one ARINC 615A operation on one target, with no window. This is only the start: the options,
// the events and the run function are in the library beside it (apps/load/src), so that the tests can run
// a whole command on the fake platform.

#include "adl/load/exit_codes.hpp"
#include "adl/load/run.hpp"
#include "adl/platform/host.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    auto host = adl::platform::make_host();
    if (!host)
    {
        std::cerr << "adl_load: the system could not be set up for the network ("
                  << adl::platform::to_string(host.error().code) << ").\n";
        return adl::load::exit_local_error;
    }
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    return adl::load::run(**host, arguments, std::cout, std::cerr);
}
