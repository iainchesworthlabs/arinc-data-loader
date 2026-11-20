// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The files the tool reads and writes, through the standard library only. Every failure is a plain
// sentence from messages.hpp; nothing throws.

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

// The whole file, or why not. `max_size` bytes at most.
[[nodiscard]] std::expected<std::string, std::string> read_file(const std::filesystem::path& path,
                                                                std::size_t max_size);

// Writes a file that does not exist yet, byte for byte (no line ending is changed). `private_file` asks for
// the file to be readable by its owner only where the system has such a thing: it is set before the first
// byte is written.
[[nodiscard]] std::expected<void, std::string> write_new_file(const std::filesystem::path& path,
                                                              std::string_view bytes, bool private_file);

// Removes a file the tool wrote, and says nothing if it is not there.
void remove_file(const std::filesystem::path& path) noexcept;

// Whether `folder`, or any folder above it, holds a `.git`: a work tree of a git repository.
[[nodiscard]] bool inside_git_work_tree(const std::filesystem::path& folder);

} // namespace adl::licence_owner
