#pragma once

#include <filesystem>

namespace adl::platform {

// The absolute path of the running executable, so that data installed beside
// it (configuration, media sets) can be found whatever the current directory is. Empty if
// the operating system will not say.
[[nodiscard]] std::filesystem::path executable_path();

}  // namespace adl::platform
