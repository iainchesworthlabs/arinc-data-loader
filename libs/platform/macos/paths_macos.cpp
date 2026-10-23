// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"

#include <mach-o/dyld.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // The first call fails on purpose: it reports the length the name needs.
    std::uint32_t size = 0;
    ::_NSGetExecutablePath(nullptr, &size);

    std::string buffer(size, '\0');
    if (::_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
        return {};
    }
    buffer.resize(std::strlen(buffer.c_str()));

    // The name may go through symlinks and "..": settle it, without throwing.
    std::error_code error;
    auto resolved = std::filesystem::weakly_canonical(buffer, error);
    return error ? std::filesystem::path{buffer} : resolved;
}

} // namespace adl::platform
