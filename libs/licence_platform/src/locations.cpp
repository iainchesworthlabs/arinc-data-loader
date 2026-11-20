// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence_platform/locations.hpp"

#include "adl/platform/paths.hpp"

namespace adl::licence_platform
{

namespace
{

constexpr std::string_view licences_folder = "licences";
constexpr std::string_view machine_licences_folder = "machine-licences";
constexpr std::string_view clock_file_name = "licence-clock.json";

[[nodiscard]] std::string join(std::string_view folder, std::string_view name)
{
    std::string path{folder};
    if (path.empty() || path.back() != '/')
    {
        path += '/';
    }
    path += name;
    return path;
}

} // namespace

std::string_view product_folder_name(std::string_view host_name) noexcept
{
    return host_name == "linux" ? "arinc-data-loader" : "ARINC Data Loader";
}

licence::store::Folders folders_for(const Roots& roots)
{
    licence::store::Folders folders;
    if (!roots.settings_directory.empty())
    {
        folders.user = join(roots.settings_directory, licences_folder);
        folders.machine = join(roots.settings_directory, machine_licences_folder);
        folders.clock_file = join(roots.settings_directory, clock_file_name);
        return folders;
    }
    const std::string_view product = product_folder_name(roots.host_name);
    if (!roots.user_data_directory.empty())
    {
        const std::string base = join(roots.user_data_directory, product);
        folders.user = join(base, licences_folder);
        folders.clock_file = join(base, clock_file_name);
    }
    if (!roots.machine_config_directory.empty())
    {
        folders.machine = join(join(roots.machine_config_directory, product), licences_folder);
    }
    return folders;
}

Roots current_roots(std::string_view host_name, const std::filesystem::path& settings_directory)
{
    Roots roots;
    roots.host_name = std::string{host_name};
    if (!settings_directory.empty())
    {
        roots.settings_directory = platform::to_platform_path(settings_directory);
    }
    roots.user_data_directory = platform::to_platform_path(platform::user_data_directory());
    roots.machine_config_directory = platform::to_platform_path(platform::machine_config_directory());
    return roots;
}

} // namespace adl::licence_platform
