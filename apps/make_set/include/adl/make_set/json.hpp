// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The little JSON that adl_make_set writes, by hand so that the program takes no dependency for it: its
// events (one object on a line) and the inspection document (one object on a line). Only writing is here;
// the description file is read by the strict reader of adl::licence.
//
// What is written is always valid UTF-8, so a reader that insists on it is not surprised by a name that
// came off a disk or out of a file: a byte that is not part of a well-formed sequence is written as U+FFFD.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace adl::make_set
{

// `text` as a JSON string, quotes included. The quotation mark and the backslash are escaped, and so are
// the control characters below U+0020 (as \b \f \n \r \t where JSON has them, \u00XX otherwise). Text that is
// not UTF-8 has each byte that is not part of a well-formed sequence written as U+FFFD.
[[nodiscard]] std::string json_string(std::string_view text);

// 1250 ms as "1.250": seconds with milliseconds, never 1.25 or 1.2500 and never an exponent. A negative span
// is written with its sign.
[[nodiscard]] std::string seconds_text(std::chrono::milliseconds value);

// Writes one JSON value as text, in the order it is told, with no spaces and the commas where they belong.
// It does not check that what it is told makes JSON beyond that: a key goes inside an object and a value
// follows a key, which the callers keep to.
class JsonWriter
{
public:
    JsonWriter& begin_object();
    JsonWriter& end_object();
    JsonWriter& begin_array();
    JsonWriter& end_array();

    // The key of the next member of the object being written.
    JsonWriter& key(std::string_view name);

    // A value: after a key, or as the next item of an array, or as the document.
    JsonWriter& string(std::string_view value);
    JsonWriter& integer(std::int64_t value);
    JsonWriter& boolean(bool value);
    JsonWriter& null();
    JsonWriter& seconds(std::chrono::milliseconds value);

    // A member in one call: key and value.
    JsonWriter& member(std::string_view name, std::string_view value);
    JsonWriter& member(std::string_view name, const char* value);
    JsonWriter& member_int(std::string_view name, std::int64_t value);
    JsonWriter& member_bool(std::string_view name, bool value);
    JsonWriter& member_null(std::string_view name);
    // A list of strings as a member.
    JsonWriter& member(std::string_view name, const std::vector<std::string>& values);

    [[nodiscard]] const std::string& text() const noexcept
    {
        return text_;
    }

private:
    void before_value();

    std::string text_;
    std::vector<bool> has_member_; // for each open container: does it hold something already
    bool after_key_ = false;
};

} // namespace adl::make_set
