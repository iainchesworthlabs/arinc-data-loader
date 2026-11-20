// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/media_tree/media_tree.hpp"

#include "adl/arinc665/portable_name.hpp"

#include <adl/profiling.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace adl::media_tree
{

namespace
{

namespace fs = std::filesystem;

constexpr std::string_view member_prefix = "member_";
constexpr std::uint16_t max_members = 255;

std::unexpected<TreeFailure> fail(TreeError error, fs::path path = {})
{
    return std::unexpected(TreeFailure{error, std::move(path)});
}

// The length of the valid UTF-8 sequence at the start of `text` (which is not empty), or 0 if what is
// there is not one: a continuation byte on its own, an overlong form, a surrogate, a value above
// U+10FFFF, or a sequence that stops short.
std::size_t utf8_sequence_length(std::string_view text) noexcept
{
    const auto byte = [&](std::size_t index) { return static_cast<unsigned char>(text[index]); };
    const unsigned char lead = byte(0);
    std::size_t length = 1;
    unsigned char lowest = 0x80;
    unsigned char highest = 0xBF;
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        length = 2;
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        length = 3;
        lowest = lead == 0xE0 ? 0xA0 : 0x80;
        highest = lead == 0xED ? 0x9F : 0xBF;
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        length = 4;
        lowest = lead == 0xF0 ? 0x90 : 0x80;
        highest = lead == 0xF4 ? 0x8F : 0xBF;
    }
    else
    {
        return 0;
    }
    if (text.size() < length || byte(1) < lowest || byte(1) > highest)
    {
        return 0;
    }
    for (std::size_t index = 2; index < length; ++index)
    {
        if ((byte(index) & 0xC0) != 0x80)
        {
            return 0;
        }
    }
    return length;
}

// The path that `text` names, read as UTF-8. Every path this library makes from text is made here,
// because a std::filesystem::path made from a std::string is read in the system's code page on Windows
// (another name, with no error, for anything that is not ASCII), and one made from a std::u8string throws
// there for bytes that are not UTF-8. A byte that is not part of valid UTF-8 becomes U+FFFD, on every
// system, so that the path can always be made and always means the same.
fs::path path_from_text(std::string_view text)
{
    std::u8string utf8;
    utf8.reserve(text.size());
    for (std::size_t index = 0; index < text.size();)
    {
        const std::size_t length = utf8_sequence_length(text.substr(index));
        if (length == 0)
        {
            utf8 += u8"\uFFFD";
            ++index;
            continue;
        }
        for (std::size_t offset = 0; offset < length; ++offset)
        {
            utf8 += static_cast<char8_t>(text[index + offset]);
        }
        index += length;
    }
    return fs::path{std::move(utf8)};
}

// The last name of `path` as text, if every character of it is printable ASCII, the only characters a
// name in a media set has (arinc665::is_portable_file_name). Nothing is converted: the units of the path's
// own text are looked at, so a name that is not text at all (half of a surrogate pair, which Windows lets a
// name hold, or bytes that are not UTF-8, which Linux does) is only a name that is not ASCII, and nothing
// can throw. (path::string() is the UTF-16 name read in the code page on Windows, and throws for a
// character outside it.)
std::optional<std::string> ascii_name(const fs::path& path)
{
    const fs::path filename = path.filename();
    std::string name;
    for (const fs::path::value_type unit : filename.native())
    {
        const auto code = std::char_traits<fs::path::value_type>::to_int_type(unit);
        if (code < 0x20 || code > 0x7E)
        {
            return std::nullopt;
        }
        name += static_cast<char>(code);
    }
    return name;
}

// The directory of a member below the root of a tree.
fs::path member_path(const fs::path& root, std::uint16_t sequence_number)
{
    return root / path_from_text(member_directory_name(sequence_number));
}

char lower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text)
{
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), lower);
    return result;
}

// "\a\b\" as {"a", "b"}; "\" as {}.
TreeResult<std::vector<std::string>> split_pathname(std::string_view pathname)
{
    if (pathname.empty() || pathname.front() != '\\' || pathname.back() != '\\')
    {
        return fail(TreeError::invalid_pathname, path_from_text(pathname));
    }
    std::vector<std::string> directories;
    std::size_t start = 1;
    while (start < pathname.size())
    {
        const std::size_t end = pathname.find('\\', start);
        const std::string_view name = pathname.substr(start, end - start);
        if (!arinc665::is_portable_file_name(name))
        {
            return fail(TreeError::invalid_pathname, path_from_text(pathname));
        }
        directories.emplace_back(name);
        start = end + 1;
    }
    return directories;
}

std::string to_pathname(const std::vector<std::string>& directories)
{
    std::string pathname = "\\";
    for (const std::string& directory : directories)
    {
        pathname += directory;
        pathname += '\\';
    }
    return pathname;
}

// The sequence number a member directory name stands for, or 0.
std::uint16_t parse_member_directory(std::string_view name)
{
    if (name.size() != member_prefix.size() + 3 || !name.starts_with(member_prefix))
    {
        return 0;
    }
    unsigned value = 0;
    for (const char c : name.substr(member_prefix.size()))
    {
        if (c < '0' || c > '9')
        {
            return 0;
        }
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    return value <= max_members ? static_cast<std::uint16_t>(value) : 0;
}

// Everything that can be checked without touching the disk.
TreeResult<void> validate(std::span<const arinc665::MediaMember> members)
{
    if (members.empty())
    {
        return fail(TreeError::no_members);
    }
    std::set<std::uint16_t> sequence_numbers;
    for (const arinc665::MediaMember& member : members)
    {
        if (member.sequence_number < 1 || member.sequence_number > max_members ||
            !sequence_numbers.insert(member.sequence_number).second)
        {
            return fail(TreeError::invalid_member, path_from_text(std::to_string(member.sequence_number)));
        }
        std::set<std::string> seen;
        for (const arinc665::MediaFile& file : member.files)
        {
            const auto directories = split_pathname(file.pathname);
            if (!directories)
            {
                return std::unexpected(directories.error());
            }
            if (!arinc665::is_portable_file_name(file.name))
            {
                return fail(TreeError::invalid_file_name, path_from_text(file.name));
            }
            if (!seen.insert(lowered(to_pathname(*directories) + file.name)).second)
            {
                return fail(TreeError::duplicate_file, path_from_text(file.pathname + file.name));
            }
        }
    }
    return {};
}

TreeResult<void> write_file(const fs::path& path, std::span<const std::byte> bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return fail(TreeError::cannot_write_file, path);
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out)
    {
        return fail(TreeError::cannot_write_file, path);
    }
    return {};
}

TreeResult<std::vector<std::byte>> read_file(const fs::path& path)
{
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec)
    {
        return fail(TreeError::cannot_read_file, path);
    }
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!in || (size != 0 && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))))
    {
        return fail(TreeError::cannot_read_file, path);
    }
    return bytes;
}

// Adds the files below `directory` to `files`. `names` is the chain of
// directory names from the member's root down to it.
TreeResult<void> read_directory(const fs::path& directory, std::vector<std::string>& names,
                                std::vector<arinc665::MediaFile>& files)
{
    std::error_code ec;
    fs::directory_iterator it(directory, ec);
    if (ec)
    {
        return fail(TreeError::cannot_read_file, directory);
    }
    for (const fs::directory_iterator end; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        const fs::path path = it->path();
        // A name that is not one every platform accepts refuses the set, whatever it is made of.
        const std::optional<std::string> name = ascii_name(path);
        if (!name || !arinc665::is_portable_file_name(*name))
        {
            return fail(TreeError::invalid_file_name, path);
        }
        const fs::file_status status = it->symlink_status(ec);
        if (ec)
        {
            return fail(TreeError::cannot_read_file, path);
        }
        if (fs::is_directory(status))
        {
            names.push_back(*name);
            if (auto below = read_directory(path, names, files); !below)
            {
                return below;
            }
            names.pop_back();
        }
        else if (fs::is_regular_file(status))
        {
            auto bytes = read_file(path);
            if (!bytes)
            {
                return std::unexpected(bytes.error());
            }
            files.push_back(arinc665::MediaFile{to_pathname(names), *name, std::move(*bytes)});
        }
        else
        {
            return fail(TreeError::unexpected_entry, path);
        }
    }
    if (ec)
    {
        return fail(TreeError::cannot_read_file, directory);
    }
    return {};
}

} // namespace

std::string_view to_string(TreeError error) noexcept
{
    switch (error)
    {
    case TreeError::no_members:
        return "no media set members";
    case TreeError::invalid_member:
        return "invalid or repeated media sequence number";
    case TreeError::invalid_pathname:
        return "invalid pathname";
    case TreeError::invalid_file_name:
        return "file name that cannot be a file on every platform";
    case TreeError::duplicate_file:
        return "two files of one name in one directory";
    case TreeError::already_exists:
        return "member directory exists and is not empty";
    case TreeError::cannot_create_directory:
        return "cannot create directory";
    case TreeError::cannot_write_file:
        return "cannot write file";
    case TreeError::cannot_read_file:
        return "cannot read file or directory";
    case TreeError::not_a_directory:
        return "not a directory";
    case TreeError::unexpected_entry:
        return "unexpected entry in the tree";
    }
    return "unknown error";
}

std::string member_directory_name(std::uint16_t sequence_number)
{
    std::string name{member_prefix};
    name += static_cast<char>('0' + sequence_number / 100 % 10);
    name += static_cast<char>('0' + sequence_number / 10 % 10);
    name += static_cast<char>('0' + sequence_number % 10);
    return name;
}

TreeResult<fs::path> native_directory(const fs::path& base, std::string_view pathname)
{
    const auto directories = split_pathname(pathname);
    if (!directories)
    {
        return std::unexpected(directories.error());
    }
    fs::path path = base;
    for (const std::string& directory : *directories)
    {
        path /= path_from_text(directory);
    }
    return path;
}

TreeResult<void> write_media_set_tree(const fs::path& root, std::span<const arinc665::MediaMember> members)
{
    const adl::profiling::Zone zone{"media_tree: write set"};
    if (auto valid = validate(members); !valid)
    {
        return valid;
    }

    std::error_code ec;
    for (const arinc665::MediaMember& member : members)
    {
        const fs::path directory = member_path(root, member.sequence_number);
        const fs::file_status status = fs::symlink_status(directory, ec);
        if (fs::exists(status) && (!fs::is_directory(status) || !fs::is_empty(directory, ec)))
        {
            return fail(TreeError::already_exists, directory);
        }
    }

    // What this call makes, so that a failure part of the way takes it away again: a full disk
    // must not leave the first members behind looking like a set.
    const bool root_existed = fs::exists(root, ec);
    struct Made
    {
        fs::path directory;
        bool existed; // an empty member directory that was there: emptied, not removed
    };
    std::vector<Made> made;
    const auto undo = [&](TreeFailure failure)
    {
        std::error_code ignored;
        for (const Made& member : made)
        {
            if (member.existed)
            {
                for (const auto& entry : fs::directory_iterator(member.directory, ignored))
                {
                    fs::remove_all(entry.path(), ignored);
                }
            }
            else
            {
                fs::remove_all(member.directory, ignored);
            }
        }
        if (!root_existed)
        {
            fs::remove(root, ignored); // only if it is empty again, which it is unless it was used
        }
        return std::unexpected(std::move(failure));
    };

    fs::create_directories(root, ec);
    if (ec || !fs::is_directory(root, ec))
    {
        return undo(TreeFailure{TreeError::cannot_create_directory, root});
    }
    for (const arinc665::MediaMember& member : members)
    {
        const fs::path member_root = member_path(root, member.sequence_number);
        made.push_back({member_root, fs::exists(member_root, ec)});
        fs::create_directories(member_root, ec);
        if (ec)
        {
            return undo(TreeFailure{TreeError::cannot_create_directory, member_root});
        }
        for (const arinc665::MediaFile& file : member.files)
        {
            const auto directory = native_directory(member_root, file.pathname);
            if (!directory)
            {
                return undo(directory.error());
            }
            fs::create_directories(*directory, ec);
            if (ec)
            {
                return undo(TreeFailure{TreeError::cannot_create_directory, *directory});
            }
            if (auto written = write_file(*directory / path_from_text(file.name), file.bytes); !written)
            {
                return undo(written.error());
            }
        }
    }
    return {};
}

TreeResult<std::vector<arinc665::MediaMember>> read_media_set_tree(const fs::path& root)
{
    const adl::profiling::Zone zone{"media_tree: read set"};
    std::error_code ec;
    if (!fs::is_directory(root, ec))
    {
        return fail(TreeError::not_a_directory, root);
    }

    std::vector<arinc665::MediaMember> members;
    fs::directory_iterator it(root, ec);
    if (ec)
    {
        return fail(TreeError::cannot_read_file, root);
    }
    for (const fs::directory_iterator end; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        const fs::path path = it->path();
        const std::optional<std::string> name = ascii_name(path);
        const std::uint16_t sequence_number = name ? parse_member_directory(*name) : 0;
        const fs::file_status status = it->symlink_status(ec);
        if (ec || sequence_number == 0 || !fs::is_directory(status))
        {
            return fail(TreeError::unexpected_entry, path);
        }
        arinc665::MediaMember member;
        member.sequence_number = sequence_number;
        std::vector<std::string> names;
        if (auto read = read_directory(path, names, member.files); !read)
        {
            return std::unexpected(read.error());
        }
        std::sort(member.files.begin(), member.files.end(),
                  [](const arinc665::MediaFile& a, const arinc665::MediaFile& b)
                  { return std::tie(a.pathname, a.name) < std::tie(b.pathname, b.name); });
        members.push_back(std::move(member));
    }
    if (ec)
    {
        return fail(TreeError::cannot_read_file, root);
    }
    if (members.empty())
    {
        return fail(TreeError::no_members, root);
    }
    std::sort(members.begin(), members.end(), [](const arinc665::MediaMember& a, const arinc665::MediaMember& b)
              { return a.sequence_number < b.sequence_number; });
    return members;
}

} // namespace adl::media_tree
