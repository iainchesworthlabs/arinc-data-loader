// SPDX-License-Identifier: GPL-3.0-or-later
// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "adl/platform/paths.hpp"
// clang-format on

namespace adl::platform
{

namespace
{

struct TaskMemoryFree
{
    void operator()(wchar_t* text) const noexcept
    {
        ::CoTaskMemFree(text);
    }
};

// The path of a known folder, or an empty path when Windows does not give an absolute one. The
// folder is not made when it is not there. Windows is asked, and the environment variable of the
// same name is not read: anyone can set a variable for a program they start, and a `ProgramData`
// pointing into a user's own folder would move the administrator's folder there.
std::filesystem::path known_folder(const KNOWNFOLDERID& id)
{
    PWSTR text = nullptr;
    const HRESULT result = ::SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &text);
    const std::unique_ptr<wchar_t, TaskMemoryFree> keep{text};
    if (FAILED(result) || text == nullptr)
    {
        return {};
    }
    std::filesystem::path path{text};
    return path.is_absolute() ? path : std::filesystem::path{};
}

} // namespace

std::string computer_name()
{
    DWORD length = 0;
    // The first call has no room, fails, and says how much room the name needs.
    ::GetComputerNameExW(ComputerNameDnsHostname, nullptr, &length);
    if (length == 0)
    {
        return {};
    }
    std::wstring wide(length, L'\0');
    if (!::GetComputerNameExW(ComputerNameDnsHostname, wide.data(), &length) || length == 0)
    {
        return {};
    }
    wide.resize(length);

    const int size =
        ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return {};
    }
    std::string name(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), name.data(), size, nullptr,
                              nullptr) != size)
    {
        return {};
    }
    return name;
}

std::filesystem::path user_data_directory()
{
    return known_folder(FOLDERID_LocalAppData);
}

std::filesystem::path machine_config_directory()
{
    return known_folder(FOLDERID_ProgramData);
}

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
