// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// One JSON object on one line, written by hand: what each event of `--json` is. Text is escaped the way JSON
// has it, and bytes that are not valid UTF-8 (a file name or a description the target sent) are each written
// as U+FFFD, so that every line is valid UTF-8 whatever a target says. Keys come out in the order they were
// added, and the line ends with a line feed.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace adl::load
{

// `text` as the inside of a JSON string, appended to `out`: a quote, a backslash and the control characters
// escaped, valid UTF-8 as it is, and U+FFFD for each byte that is not part of valid UTF-8.
void append_json_escaped(std::string& out, std::string_view text);

class JsonLine
{
public:
    JsonLine();

    void begin_object(std::string_view key = {});
    void end_object();
    void begin_array(std::string_view key);
    void end_array();

    void string(std::string_view key, std::string_view value);
    void number(std::string_view key, std::uint64_t value);
    // A whole number that may be negative.
    void integer(std::string_view key, std::int64_t value);
    // A number element of the array that is open.
    void number_element(std::uint64_t value);
    // A number of seconds with milliseconds, from a whole number of milliseconds: 12345 is 12.345.
    void milliseconds_as_seconds(std::string_view key, std::uint64_t milliseconds);
    void boolean(std::string_view key, bool value);
    void null(std::string_view key);

    // A string element of the array that is open.
    void string_element(std::string_view value);

    // The line with the closing brace of the object and the line feed. Every object and array opened after the
    // first has to have been closed.
    [[nodiscard]] std::string finish();

private:
    // The comma after the member before, and the key.
    void member(std::string_view key);

    std::string out_;
    std::vector<bool> first_; // for each open object or array: nothing has been written in it yet
};

} // namespace adl::load
