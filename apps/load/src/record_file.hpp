// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Writing the operation record to the file `--record` names, through the platform's file system. The text is
// written to the name with ".part" after it and then renamed, so that a reader that watches for the file never
// sees half of one, and a record that was there before is replaced as a whole or not at all.

#include "adl/platform/files.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace adl::load
{

// Writes `text` to `path` (as the operator wrote it). Returns what went wrong, in a few words ("cannot create
// out/r.json.part (not found)"), or nothing when the file is there.
[[nodiscard]] std::optional<std::string> write_text_file(adl::platform::FileSystem& files, const std::string& path,
                                                         std::string_view text);

} // namespace adl::load
