// SPDX-License-Identifier: GPL-3.0-or-later
#include "record_file.hpp"

#include "adl/platform/paths.hpp"

#include <cstddef>
#include <filesystem>
#include <span>

namespace adl::load
{

namespace
{

// "<what> <path> (<why>)".
std::string problem(std::string_view what, const std::string& path, adl::platform::ErrorCode code)
{
    return std::string{what} + ' ' + path + " (" + adl::platform::to_string(code) + ')';
}

// The file written under its temporary name; false, with `why` set, when a step failed. The file is closed when
// this returns.
bool write_temporary(adl::platform::FileSystem& files, const std::string& temporary, const std::string& shown,
                     std::string_view text, std::string& why)
{
    auto created = files.create(temporary, adl::platform::CreateMode::replace);
    if (!created)
    {
        why = problem("cannot create", shown, created.error().code);
        return false;
    }
    adl::platform::WriteFile& file = **created;
    if (const auto written = file.write(std::as_bytes(std::span{text})); !written)
    {
        why = problem("cannot write", shown, written.error().code);
        return false;
    }
    if (const auto closed = file.close(); !closed)
    {
        why = problem("cannot write", shown, closed.error().code);
        return false;
    }
    return true;
}

} // namespace

std::optional<std::string> write_text_file(adl::platform::FileSystem& files, const std::string& path,
                                           std::string_view text)
{
    const std::string target = adl::platform::to_platform_path(std::filesystem::path{path});
    const std::string temporary = target + ".part";
    std::string why;
    if (!write_temporary(files, temporary, path + ".part", text, why))
    {
        static_cast<void>(files.remove(temporary));
        return why;
    }
    if (const auto renamed = files.rename(temporary, target); !renamed)
    {
        static_cast<void>(files.remove(temporary));
        return problem("cannot replace", path, renamed.error().code);
    }
    return std::nullopt;
}

} // namespace adl::load
