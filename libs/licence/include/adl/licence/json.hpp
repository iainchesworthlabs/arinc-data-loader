// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The strict JSON reader of the signed files, for any other file that must mean one thing to every reader:
// a scenario file, a description file, a record. It reads strings, whole numbers, true, false, null, lists
// and objects, at most max_depth levels deep, and refuses what a plain parser lets through: an object with
// the same key twice, a comment, a trailing comma, a byte order mark, text after the value, a control
// character in a string, bytes that are not UTF-8, and a lone surrogate in an escape. A number with a
// fraction or an exponent, or outside the range of std::int64_t, is read as OtherNumber, and the field
// readers refuse it where they want a whole number. Every refusal is Error::malformed, with no position.
//
// The reader itself is src/json.hpp, where the licence code keeps using it as adl::licence::detail. This
// header is the way to reach it from outside the library: the names below stand for the same types and
// functions, and this file adds no code of its own. It includes src/json.hpp by its place beside this
// header's directory, so a program that takes this library, whole or from a pinned copy, needs no
// include directory for src/.

#include "../../../src/json.hpp"

#include <cstddef>

namespace adl::licence::json
{

using Value = detail::JsonValue;
using Member = detail::JsonMember;
using Array = detail::JsonArray;
using Object = detail::JsonObject;
using OtherNumber = detail::JsonOtherNumber;

// How many containers may be inside one another (eight): a deeper document is refused.
inline constexpr std::size_t max_depth = detail::max_json_depth;

// Parses one JSON document. Error::malformed for everything above and for text that is not JSON.
using detail::parse_strict;

// The member of an object with this key, or null.
using detail::find;

// The field readers return Error::malformed for a missing field, one of the wrong type, or one outside
// the size or range given. optional_string takes an absent field or a null as the empty string, and
// string_list takes an absent field as an empty list.
using detail::optional_string;
using detail::required_integer;
using detail::required_string;
using detail::string_list;

// Whether a text is 1 to max_length characters, each a lower-case letter, a digit or one of `extra`.
using detail::is_token;

// A text as a JSON string with its quotes: the quote, the backslash and the control characters are
// escaped and nothing else is.
using detail::quote;

} // namespace adl::licence::json
