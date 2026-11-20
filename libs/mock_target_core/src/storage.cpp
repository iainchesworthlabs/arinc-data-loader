// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/storage.hpp"

#include "adl/arinc665/portable_name.hpp"
#include "adl/platform/paths.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace adl::mock
{

namespace
{

[[nodiscard]] bool is_plain_character(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == '-';
}

[[nodiscard]] Bytes to_bytes(std::string_view text)
{
    Bytes bytes(text.size());
    std::ranges::transform(text, bytes.begin(), [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
    return bytes;
}

} // namespace

bool is_storable_name(std::string_view name) noexcept
{
    return name.size() <= 254 && adl::arinc665::is_portable_file_name(name) &&
           std::ranges::all_of(name, is_plain_character);
}

std::string storage_directory_name(std::string_view part_number)
{
    std::string result;
    result.reserve(part_number.size());
    for (const char c : part_number.substr(0, 254))
    {
        result.push_back(is_plain_character(c) ? c : '_');
    }
    if (result.empty())
    {
        return "_";
    }
    // Windows drops a trailing dot (and with it a name made of dots only), and a device name is
    // a device in any directory.
    if (result.back() == '.')
    {
        result.back() = '_';
    }
    if (adl::arinc665::is_device_name(result))
    {
        result.insert(result.begin(), '_');
    }
    return result;
}

StoreOutcome store_upload(const std::filesystem::path& storage, const UploadRecord& record)
{
    StoreOutcome outcome;
    if (storage.empty())
    {
        outcome.issues.push_back(StoreIssue{StoreProblem::no_storage_folder, {}, {}});
        return outcome;
    }
    std::error_code ec;
    std::filesystem::create_directories(storage, ec);
    if (ec)
    {
        outcome.issues.push_back(StoreIssue{StoreProblem::storage_folder_failed, {}, ec.message()});
        return outcome;
    }
    // Plain ASCII, whatever the part number was (storage_directory_name), so no conversion of
    // encodings is involved in making it a path.
    const std::filesystem::path load_folder = storage / storage_directory_name(record.load_part_number);
    std::filesystem::create_directories(load_folder, ec);
    if (ec)
    {
        outcome.issues.push_back(StoreIssue{StoreProblem::load_folder_failed, {}, ec.message()});
        return outcome;
    }
    for (const StoredFile& file : record.files)
    {
        if (!is_storable_name(file.name))
        {
            outcome.issues.push_back(StoreIssue{StoreProblem::unsafe_file_name, file.name, {}});
            continue;
        }
        const std::filesystem::path path = load_folder / file.name; // plain ASCII, as is_storable_name checked
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(file.bytes.data()), static_cast<std::streamsize>(file.bytes.size()));
        out.close();
        if (!out)
        {
            outcome.issues.push_back(StoreIssue{StoreProblem::file_write_failed, file.name, {}});
            continue;
        }
        outcome.stored.push_back(StoredOnDisk{file.name, static_cast<std::uint64_t>(file.bytes.size()), path});
    }
    return outcome;
}

std::expected<std::vector<HeldFile>, std::string> read_held_files(const std::filesystem::path& folder)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec))
    {
        return std::unexpected(ec ? ec.message() : std::string{"not a folder"});
    }
    // The non-throwing iteration: a folder the account cannot list (or one that changes while it is
    // read) ends the list, it does not end the program.
    std::filesystem::directory_iterator it(folder, ec);
    if (ec)
    {
        return std::unexpected(ec.message());
    }
    std::vector<HeldFile> found;
    for (const std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
    {
        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec))
        {
            ec.clear();
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        Bytes bytes(text.size());
        std::ranges::transform(text, bytes.begin(), [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
        found.push_back(HeldFile{adl::platform::to_utf8(entry.path().filename().u8string()), std::move(bytes), ""});
    }
    std::ranges::sort(found, [](const HeldFile& a, const HeldFile& b) { return a.name < b.name; });
    return found;
}

HeldFile demonstration_file(std::string_view where)
{
    const std::string note = "The mock target serves the files it holds from " + std::string{where} +
                             " in a download.\nPut files there and ask for them by name.\n";
    return HeldFile{"DEMO.TXT", to_bytes(note), "a demonstration file"};
}

} // namespace adl::mock
