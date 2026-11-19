// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a test of a whole adl_load command needs: the loader's host and a simulated target on one fake
// network (tests/engine/rig.hpp), the files of a media set both on the disk (where the program checks the set)
// and in the fake file system (where an upload reads it from), and the command's output read back as events.

#include "adl/load/run.hpp"
#include "mini_json.hpp"
#include "rig.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

// What one command printed and returned.
struct CommandRun
{
    int exit_code = -1;
    std::string out;
    std::string err;

    // Standard output as the events of `--json`, each of which has to be one valid JSON object.
    [[nodiscard]] std::vector<Json> events() const
    {
        std::vector<Json> parsed = parse_json_lines(out);
        for (const Json& event : parsed)
        {
            INFO("a line of standard output that is not a JSON object: " << event.text);
            REQUIRE(event.is(Json::Kind::object));
        }
        return parsed;
    }
    // The `event` of each event, in order.
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> names;
        for (const Json& event : events())
        {
            names.push_back(event.at("event").text);
        }
        return names;
    }
    // The events called `name`.
    [[nodiscard]] std::vector<Json> events_named(std::string_view name) const
    {
        std::vector<Json> found;
        for (const Json& event : events())
        {
            if (event.at("event").text == name)
            {
                found.push_back(event);
            }
        }
        return found;
    }
    // The last event, which has to be `finished`.
    [[nodiscard]] Json finished() const
    {
        const std::vector<Json> all = events();
        REQUIRE_FALSE(all.empty());
        REQUIRE(all.back().at("event").text == "finished");
        return all.back();
    }
};

class CommandRig
{
public:
    explicit CommandRig(SimPlan plan = {}) : rig(std::move(plan)) {}

    // Runs a command on the loader's host, to its end, and lets the target see the loader's last datagram.
    CommandRun run(std::initializer_list<std::string_view> arguments)
    {
        const std::vector<std::string_view> list(arguments);
        std::ostringstream out;
        std::ostringstream err;
        CommandRun result;
        result.exit_code = adl::load::run(rig.loader_host, list, out, err);
        result.out = out.str();
        result.err = err.str();
        rig.world.advance_by(std::chrono::seconds(1));
        return result;
    }

    Rig rig;
};

} // namespace adl::test
