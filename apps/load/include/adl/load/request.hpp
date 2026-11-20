// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The requests of `adl_load serve` (docs/load-command.md): one line of UTF-8 on standard input each, the fields
// separated by one tab, taken as they are with no quoting. `run <id> <command and its options>` starts an
// operation, `cancel <id>` aborts one, `stop` ends the program once the running operations have. Both functions
// here take bytes from outside and so have a fuzz harness (apps/load/fuzz): whatever the input, they return
// a request or the sentence that tells why it is not one, and never read or write anything beyond the bytes.

#include "adl/load/options.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace adl::load
{

enum class RequestKind : std::uint8_t
{
    run,
    cancel,
    stop
};

// A request that was read. `id` is the caller's name for the operation (empty for `stop`); `options` are those of
// the command a `run` names (they have the defaults for the others).
struct Request
{
    RequestKind kind = RequestKind::stop;
    std::string id;
    Options options;
};

// A line that is not a request: the sentence for a person, and the id if the line had one that could be read
// (a `run` or `cancel` whose second field is a valid id).
struct RequestError
{
    std::optional<std::string> id;
    std::string detail;
};

// 1 to 64 of the characters A to Z, a to z, 0 to 9, `.`, `_` and `-`.
[[nodiscard]] bool is_valid_id(std::string_view id) noexcept;

// One line, without its line feed or the carriage return before it. A line that is not valid UTF-8, has a NUL
// in it, names no request, or whose arguments are wrong, is refused with the reason.
[[nodiscard]] std::expected<Request, RequestError> parse_request(std::string_view line);

// The longest line that is read. A longer one is skipped, and refused.
inline constexpr std::size_t max_request_line_bytes = std::size_t{1} << 20;

// What read_line found.
enum class LineEnd : std::uint8_t
{
    line,       // a line, in `text` without its end
    too_long,   // a line over `max_bytes`: it was read to its end and dropped, `text` is empty
    unfinished, // the input ended in the middle of a line: `text` is what there was, and it is not a request
    end         // the input has ended, with nothing more in it
};

// Reads the next line of `in` into `text` (replacing what was in it), taking a carriage return before the line
// feed as part of the end. It blocks until a line feed or the end of the input. A stream that has failed is the
// end of the input.
[[nodiscard]] LineEnd read_line(std::istream& in, std::string& text, std::size_t max_bytes = max_request_line_bytes);

} // namespace adl::load
