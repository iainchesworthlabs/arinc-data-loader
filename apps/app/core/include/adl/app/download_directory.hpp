// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The directory a download is written into, made by adl::loader_core (adl/loader_core/
// download_directory.hpp), with what goes wrong said in the operator's words: the library's
// DirectoryError becomes a Message, translated when it is read.

#include "adl/app/message.hpp"
#include "adl/platform/files.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace adl::app
{

// adl::loader_core::make_directory_chain. On failure, what to tell the operator.
[[nodiscard]] std::expected<void, Message> make_directory_chain(adl::platform::FileSystem& files,
                                                                std::string_view path);

// adl::loader_core::make_download_directory. Returns the platform path (UTF-8, '/' separated) of the
// new directory, or what to tell the operator.
[[nodiscard]] std::expected<std::string, Message>
make_download_directory(adl::platform::FileSystem& files, std::string_view directory, std::string_view thw_id_pos);

} // namespace adl::app
