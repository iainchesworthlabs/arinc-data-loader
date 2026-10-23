// QNX Neutrino. UNBUILT AND UNTESTED, like everything in this directory; see
// docs/platform-abstraction.md ("Status of each platform").

#include "adl/platform/paths.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // QNX names the running program in /proc/self/exefile, as text.
    std::FILE* file = std::fopen("/proc/self/exefile", "r");
    if (file == nullptr)
    {
        return {};
    }
    std::string name;
    char chunk[256];
    std::size_t got = 0;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0)
    {
        name.append(chunk, got);
    }
    std::fclose(file);
    while (!name.empty() && (name.back() == '\n' || name.back() == '\0'))
    {
        name.pop_back();
    }
    return std::filesystem::path{name};
}

} // namespace adl::platform
