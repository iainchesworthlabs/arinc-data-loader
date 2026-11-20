// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The text of what adl_licence prints that came from a file, a folder or the command line.

#include "adl/licence/licence.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace adl::licence_cli
{

// `text` with every character that could move the cursor, change the colours or reorder the text of a terminal
// replaced by '?': the control characters (C0, DEL and C1), the line and paragraph separators, the characters
// that set the direction of text, and any byte that is not part of a UTF-8 character. A letter outside ASCII
// is kept as it is.
[[nodiscard]] std::string plain(std::string_view text);

// "2027-03-01": the date of a time, in UTC.
[[nodiscard]] std::string date_of(licence::TimePoint value);

// The items, each made `plain`, separated by ", "; `empty` when there are none.
[[nodiscard]] std::string joined(const std::vector<std::string>& items, std::string_view empty);

} // namespace adl::licence_cli
