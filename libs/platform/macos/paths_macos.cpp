// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"
#include "detail/data_locations.hpp"
#include "posix/posix_os.hpp"

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

std::filesystem::path user_data_directory()
{
    const std::string directory = detail::macos_user_data_directory(posix_os::home_directory());
    return directory.empty() ? std::filesystem::path{} : std::filesystem::path{directory};
}

std::filesystem::path machine_config_directory()
{
    // Where an administrator keeps files for every user of a Mac.
    return std::filesystem::path{"/Library/Application Support"};
}

} // namespace adl::platform
