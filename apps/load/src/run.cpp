// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/run.hpp"

#include "adl/load/events.hpp"
#include "adl/load/exit_codes.hpp"
#include "adl/load/options.hpp"
#include "adl/load/serve.hpp"
#include "commands.hpp"

#include <istream>
#include <ostream>
#include <sstream>

namespace adl::load
{

int run(adl::platform::Host& host, std::span<const std::string_view> arguments, std::istream& in, std::ostream& out,
        std::ostream& err)
{
    // `t` of every event counts from here.
    const adl::platform::Instant start = host.clock().now();

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

    const Options& options = parsed->options;
    if (options.command == Command::serve)
    {
        StreamLineSource requests(host, in);
        return run_serve(host, options, requests, out, err, start);
    }
    EventWriter events(out, options.json, host.clock(), start);
    if (options.command == Command::find)
    {
        return run_find_command(host, options, events);
    }
    return run_operation_command(host, options, events, err);
}

int run(adl::platform::Host& host, std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err)
{
    std::istringstream nothing;
    return run(host, arguments, nothing, out, err);
}

} // namespace adl::load
