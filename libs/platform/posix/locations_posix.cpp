// SPDX-License-Identifier: GPL-3.0-or-later
// The two things about "where and who" that Linux and macOS answer the same way: the name of this
// computer and the home directory of the signed-in user. The folders that differ between the two
// are in linux/paths_linux.cpp and macos/paths_macos.cpp; the rules they apply to what is read
// here are in detail/data_locations.hpp, which is tested on every host.

#include "adl/platform/paths.hpp"
#include "detail/data_locations.hpp"
#include "posix/posix_os.hpp"

#include <pwd.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace adl::platform
{

std::string computer_name()
{
    // gethostname does not say that it cut a long name, and then need not end the text with a NUL,
    // so it is given one byte less than the buffer holds and that last byte stays NUL.
    std::string buffer(256 + 1, '\0');
    if (::gethostname(buffer.data(), buffer.size() - 1) != 0)
    {
        return {};
    }
    buffer.resize(std::strlen(buffer.c_str()));
    return buffer;
}

} // namespace adl::platform

namespace adl::platform::posix_os
{

std::string home_directory()
{
    // The environment is untrusted text: only an absolute path counts.
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* const home = std::getenv("HOME");
    if (home != nullptr && detail::is_absolute_posix_path(home))
    {
        return home;
    }

    // HOME is not set for every program (a service, a cleared environment): ask the password
    // database, with a buffer that grows until the entry fits.
    const long suggested = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    constexpr std::size_t first_size = 1024;
    constexpr std::size_t largest = std::size_t{1} << 20;
    std::vector<char> buffer(suggested > 0 ? static_cast<std::size_t>(suggested) : first_size);
    for (;;)
    {
        passwd entry{};
        passwd* found = nullptr;
        const int error = ::getpwuid_r(::getuid(), &entry, buffer.data(), buffer.size(), &found);
        if (error == ERANGE && buffer.size() < largest)
        {
            buffer.resize(buffer.size() * 2);
            continue;
        }
        if (error != 0 || found == nullptr || found->pw_dir == nullptr ||
            !detail::is_absolute_posix_path(found->pw_dir))
        {
            return {};
        }
        return found->pw_dir;
    }
}

} // namespace adl::platform::posix_os
