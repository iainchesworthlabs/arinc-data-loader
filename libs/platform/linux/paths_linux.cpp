// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"
#include "detail/data_locations.hpp"
#include "posix/posix_os.hpp"

#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // readlink does not say when it truncated, so grow until the name fits.
    std::string buffer(256, '\0');
    for (;;)
    {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0)
        {
            return {};
        }
        if (static_cast<std::size_t>(length) < buffer.size())
        {
            buffer.resize(static_cast<std::size_t>(length));
            return std::filesystem::path{buffer};
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path user_data_directory()
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* const xdg_data_home = std::getenv("XDG_DATA_HOME");
    const std::string directory =
        detail::xdg_user_data_directory(xdg_data_home != nullptr ? xdg_data_home : "", posix_os::home_directory());
    return directory.empty() ? std::filesystem::path{} : std::filesystem::path{directory};
}

std::filesystem::path machine_config_directory()
{
    // Where an administrator keeps files for every user of a Linux computer.
    return std::filesystem::path{"/etc"};
}

} // namespace adl::platform
