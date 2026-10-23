#include <windows.h>

#include <filesystem>
#include <string>

#include "adl/platform/paths.hpp"

namespace adl::platform {

std::filesystem::path executable_path() {
    // MAX_PATH is not a limit on a long path, so grow until the name fits.
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path{buffer};
        }
        buffer.resize(buffer.size() * 2);
    }
}

}  // namespace adl::platform
