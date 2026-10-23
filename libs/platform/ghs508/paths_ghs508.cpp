// INTEGRITY 5.0.8 (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY headers; see docs/platform-ghs.md.

#include <filesystem>

#include "adl/platform/paths.hpp"

namespace adl::platform {

std::filesystem::path executable_path() {
    // INTEGRITY does not name the running program to it, and the loader takes
    // its data locations from configuration, so the empty path is the honest
    // answer (the interface allows it).
    return {};
}

}  // namespace adl::platform
