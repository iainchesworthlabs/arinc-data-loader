// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// JSON reading for signed files, stricter than a plain parse, and the typed field readers both layers
// use. Internal to adl_licence. It reads what a licence file holds (strings, whole numbers, lists,
// objects, true, false and null) and nothing more: a number with a fraction or an exponent is read,
// and then refused by every field that wants a whole number.

#include "adl/licence/error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace adl::licence::detail
{

struct JsonValue;
struct JsonMember;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<JsonMember>;

// A number that is not a whole number in the range of std::int64_t.
struct JsonOtherNumber
{
};

struct JsonValue
{
    std::variant<std::monostate, bool, std::int64_t, JsonOtherNumber, std::string, JsonArray, JsonObject> data;

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] const std::string* as_string() const noexcept;
    [[nodiscard]] const std::int64_t* as_integer() const noexcept;
    [[nodiscard]] const JsonArray* as_array() const noexcept;
    [[nodiscard]] const JsonObject* as_object() const noexcept;
};

struct JsonMember
{
    std::string key;
    JsonValue value;
};

// Deep enough for an envelope and a payload, and no deeper.
inline constexpr std::size_t max_json_depth = 8;

// Parses one JSON document and refuses what a plain parser lets through, so that no two readers can
// disagree about what a file says: an object with the same key twice, a comment, a trailing comma, a
// byte order mark, text after the value, a string with a control character or with bytes that are not
// UTF-8, and a lone surrogate in an escape. Error::malformed for all of them.
[[nodiscard]] Result<JsonValue> parse_strict(std::string_view text);

// The member of `object` with this key, or null.
[[nodiscard]] const JsonValue* find(const JsonObject& object, std::string_view key) noexcept;

// The field readers return Error::malformed for a missing field or one of the wrong type or size.
[[nodiscard]] Result<std::string> required_string(const JsonObject& object, std::string_view key,
                                                  std::size_t max_length);

// An absent field or a JSON null is the empty string.
[[nodiscard]] Result<std::string> optional_string(const JsonObject& object, std::string_view key,
                                                  std::size_t max_length);

// A whole number in [minimum, maximum]. A number written with a fraction or an exponent is refused.
[[nodiscard]] Result<std::int64_t> required_integer(const JsonObject& object, std::string_view key,
                                                    std::int64_t minimum, std::int64_t maximum);

// An array of strings. An absent field is an empty list.
[[nodiscard]] Result<std::vector<std::string>> string_list(const JsonObject& object, std::string_view key,
                                                           std::size_t max_entries, std::size_t max_length);

// Whether `text` is 1 to `max_length` characters, each a lower-case letter, a digit or one of `extra`.
[[nodiscard]] bool is_token(std::string_view text, std::size_t max_length, std::string_view extra) noexcept;

// `text` as a JSON string, with its quotes. Only what must be escaped is: the quote, the backslash and
// the control characters.
[[nodiscard]] std::string quote(std::string_view text);

} // namespace adl::licence::detail
