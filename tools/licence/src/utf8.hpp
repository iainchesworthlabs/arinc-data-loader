// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// Text as the tool handles it: UTF-8 everywhere, never the code page. On Windows the entry point
// (os/windows/entry.cpp) is given the command line as UTF-16 and converts it here; paths are built from UTF-8
// as std::u8string, which std::filesystem reads as UTF-8 on every system.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

// UTF-16 as UTF-8. Nothing when `text` holds a surrogate that is not half of a pair.
[[nodiscard]] std::optional<std::string> utf16_to_utf8(std::u16string_view text);

// The path a UTF-8 name stands for, and the UTF-8 name of a path.
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view text);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);

} // namespace adl::licence_owner
