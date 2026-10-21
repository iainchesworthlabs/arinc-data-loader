#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace adl::platform {

// The absolute path of the running executable, so that data installed beside
// it (configuration, media sets) can be found whatever the current directory is. Empty if
// the operating system will not say.
[[nodiscard]] std::filesystem::path executable_path();

// The bytes of UTF-8 text held as char8_t (what std::filesystem::path::u8string() returns) as the
// std::string the rest of the code holds its text in.
[[nodiscard]] inline std::string to_utf8(std::u8string_view text) {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

// A path as the FileSystem interface (files.hpp) wants it: UTF-8 with '/' as the separator.
[[nodiscard]] inline std::string to_platform_path(const std::filesystem::path& path) {
    return to_utf8(path.generic_u8string());
}

}  // namespace adl::platform
