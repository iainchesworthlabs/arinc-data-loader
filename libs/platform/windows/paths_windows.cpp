// SPDX-License-Identifier: GPL-3.0-or-later
// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <windows.h>

#include <filesystem>
#include <string>

#include "adl/platform/paths.hpp"
// clang-format on

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // MAX_PATH is not a limit on a long path, so grow until the name fits.
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size())
        {
            buffer.resize(length);
            return std::filesystem::path{buffer};
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace adl::platform
