// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the fuzz harness of the requests of `adl_load serve` (apps/load/fuzz) and the test that replays its seeds
// under the ordinary compilers share: the fuzzer's bytes taken as standard input, cut into lines the way the
// program cuts them, each line read as a request. A line that is not a request is not an error: refusing is what
// the reader is for. What has to hold is that whatever came out is consistent with the line it came from: a
// request has the id its line names, a `run` has the options that its command line would have, written out and
// read again they are the same, and a refusal says why and names an id only if the line had one.

#include "adl/load/request.hpp"
#include "load_options_fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

struct RequestFuzzResult
{
    bool consistent = true;
    std::size_t lines = 0;    // lines that were read whole
    std::size_t requests = 0; // of them, requests
};

// The arguments of a run request written out again: the options as a command line writes them, without the
// three a request does not take (which have their defaults in what a request gives).
[[nodiscard]] inline std::vector<std::string> request_arguments(const adl::load::Options& options)
{
    const std::vector<std::string> full = render_options(options);
    std::vector<std::string> kept;
    for (std::size_t i = 0; i < full.size(); ++i)
    {
        if (full[i] == "--json")
        {
            continue;
        }
        if (full[i] == "--local-address" || full[i] == "--control-port")
        {
            ++i;
            continue;
        }
        kept.push_back(full[i]);
    }
    return kept;
}

[[nodiscard]] inline RequestFuzzResult fuzz_requests(const std::uint8_t* data, std::size_t size)
{
    using adl::load::LineEnd;
    using adl::load::RequestKind;
    RequestFuzzResult result;
    std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
    std::string text;
    // A short limit, so that a line over it comes up in the fuzzer's small inputs.
    constexpr std::size_t limit = 512;
    for (;;)
    {
        const LineEnd end = adl::load::read_line(in, text, limit);
        if (end == LineEnd::end)
        {
            break;
        }
        if (end == LineEnd::too_long)
        {
            result.consistent = result.consistent && text.empty();
            continue;
        }
        if (end == LineEnd::unfinished)
        {
            break; // the input ended in the middle of a line: nothing more is read
        }
        ++result.lines;
        // A line is cut at the line feed, with the carriage return before it, and holds neither.
        result.consistent = result.consistent && text.find('\n') == std::string::npos && text.size() <= limit;

        const auto request = adl::load::parse_request(text);
        if (!request)
        {
            const auto& error = request.error();
            const bool id_is_in_the_line =
                !error.id || (adl::load::is_valid_id(*error.id) && text.find('\t' + *error.id) != std::string::npos);
            const bool request_is_the_first_word =
                !error.request ||
                ((*error.request == "run" || *error.request == "cancel" || *error.request == "stop") &&
                 text.starts_with(*error.request));
            result.consistent =
                result.consistent && !error.detail.empty() && id_is_in_the_line && request_is_the_first_word;
            continue;
        }
        ++result.requests;
        switch (request->kind)
        {
        case RequestKind::stop:
            result.consistent = result.consistent && request->id.empty() && text == "stop";
            break;
        case RequestKind::cancel:
            result.consistent =
                result.consistent && adl::load::is_valid_id(request->id) && text == "cancel\t" + request->id;
            break;
        case RequestKind::run:
        {
            result.consistent = result.consistent && adl::load::is_valid_id(request->id) &&
                                text.starts_with("run\t" + request->id + '\t') &&
                                request->options.command != adl::load::Command::find &&
                                request->options.command != adl::load::Command::serve;
            // The options, written out as the fields of a request and read again, are the same.
            std::string again = "run\t" + request->id + '\t' + std::string{to_string(request->options.command)};
            const std::vector<std::string> arguments = request_arguments(request->options);
            // render_options starts with the command; the line has it already.
            for (std::size_t i = 1; i < arguments.size(); ++i)
            {
                again += '\t' + arguments[i];
            }
            const auto reread = adl::load::parse_request(again);
            result.consistent =
                result.consistent && reread && reread->kind == RequestKind::run && reread->options == request->options;
            break;
        }
        }
    }
    return result;
}

} // namespace adl::test
