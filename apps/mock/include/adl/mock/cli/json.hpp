// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The little JSON that adl_mock writes: one object on a line, built by hand so that the program
// takes no dependency for it. Only writing is here (no reader), and only what the events need:
// strings, whole numbers, booleans, null, a number with milliseconds, and a list of strings.

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace adl::mock::cli
{

// `text` as a JSON string, quotes included. Quotation mark and backslash are escaped, so are the
// control characters below U+0020 (as \b \f \n \r \t where JSON has them, \u00XX otherwise). The
// result is always valid UTF-8: text that is not (a name that came off the wire can be anything) has
// each byte that is not part of a well-formed sequence written as U+FFFD.
[[nodiscard]] std::string json_string(std::string_view text);

// One object on one line, with its members in the order they were added: {"a":1,"b":"x"}. No
// spaces. The keys are the program's own words and are written as they are.
class JsonObject
{
public:
    JsonObject& string(std::string_view key, std::string_view value);
    JsonObject& number(std::string_view key, std::int64_t value);
    JsonObject& boolean(std::string_view key, bool value);
    JsonObject& null(std::string_view key);
    // Seconds as a number with milliseconds: 1.250, never 1.25 or 1.2500 and never an exponent.
    JsonObject& seconds(std::string_view key, std::chrono::milliseconds value);
    JsonObject& strings(std::string_view key, std::span<const std::string> values);
    JsonObject& numbers(std::string_view key, std::span<const std::int64_t> values);

    // The object, without a line feed.
    [[nodiscard]] std::string text() const;

private:
    void key(std::string_view name);

    std::string members_;
};

// 1250 ms as "1.250". A negative span is written with its sign.
[[nodiscard]] std::string seconds_text(std::chrono::milliseconds value);

} // namespace adl::mock::cli
