// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.

#include <filesystem>

#include "adl/platform/paths.hpp"

namespace adl::platform {

std::filesystem::path executable_path() {
    // An application partition has no file from which it came: tasks are
    // configured into the kernel image. The empty path is the honest answer.
    return {};
}

}  // namespace adl::platform
