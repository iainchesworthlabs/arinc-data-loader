// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The operation record as the file other programs read (docs/file-reference.md, "The operation record"):
// one JSON object, UTF-8 with no byte order mark, two spaces of indentation, lines ended by a line feed
// and a line feed at the end, every key present in every record and in the same order. The same record
// gives the same bytes on every platform. Nothing in this repository reads the file back: a program that
// keeps, signs or compares records reads it with a JSON reader of its own.

#include "adl/loader_core/record.hpp"

#include <string>

namespace adl::loader_core
{

// The file's "format": the number of this layout. Removing a key, renaming one or changing what one
// means changes it; adding a key does not.
inline constexpr unsigned record_format = 1;

// The file's "kind".
inline constexpr std::string_view record_kind = "adl-operation-record";

// The file's text, which is UTF-8: bytes of a string in the record that are not valid UTF-8 (a description
// or a file name the target sent) are each written as U+FFFD. A time is written as 2026-11-19T10:15:30Z,
// or null when it is missing or its year is not in 0000 to 9999. "licence" is null: it is reserved for the
// edition and the licence number.
[[nodiscard]] std::string record_to_json(const OperationRecord& record);

} // namespace adl::loader_core
