// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/run.hpp"

#include "adl/load/exit_codes.hpp"
#include "adl/load/options.hpp"

#include <ostream>

namespace adl::load
{

int run(adl::platform::Host& /*host*/, std::span<const std::string_view> arguments, std::ostream& out,
        std::ostream& err)
{
    const auto parsed = parse_options(arguments);
    if (!parsed)
    {
        err << "adl_load: " << parsed.error() << "\n\n" << usage_text();
        return exit_usage;
    }
    switch (parsed->action)
    {
    case Action::help:
        out << usage_text();
        return exit_completed;
    case Action::version:
        out << version_line() << '\n';
        return exit_completed;
    case Action::run:
        break;
    }
    err << "adl_load: the " << to_string(parsed->options.command) << " command is not built yet.\n";
    return exit_local_error;
}

} // namespace adl::load
