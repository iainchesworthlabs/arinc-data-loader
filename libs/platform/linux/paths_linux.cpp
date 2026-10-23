#include <unistd.h>

#include <cstddef>
#include <filesystem>
#include <string>

#include "adl/platform/paths.hpp"

namespace adl::platform {

std::filesystem::path executable_path() {
    // readlink does not say when it truncated, so grow until the name fits.
    std::string buffer(256, '\0');
    for (;;) {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return {};
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<std::size_t>(length));
            return std::filesystem::path{buffer};
        }
        buffer.resize(buffer.size() * 2);
    }
}

}  // namespace adl::platform
