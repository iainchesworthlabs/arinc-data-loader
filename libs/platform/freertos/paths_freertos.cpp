#include <filesystem>

#include "adl/platform/paths.hpp"

namespace adl::platform {

std::filesystem::path executable_path() {
    // A kernel image is not a file the running program can find, so there is no path to
    // give; the interface allows an empty one.
    return {};
}

}  // namespace adl::platform
