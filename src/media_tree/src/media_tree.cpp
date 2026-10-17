#include "adl/media_tree/media_tree.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <set>
#include <system_error>
#include <tuple>
#include <utility>

namespace adl::media_tree {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view member_prefix = "member_";
constexpr std::uint16_t max_members = 255;

std::unexpected<TreeFailure> fail(TreeError error, fs::path path = {}) {
    return std::unexpected(TreeFailure{error, std::move(path)});
}

char lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text) {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), lower);
    return result;
}

// CON, PRN, AUX, NUL, COM1 to COM9 and LPT1 to LPT9, with or without an
// extension: on Windows these are devices, whatever directory they are named in.
bool is_device_name(std::string_view name) {
    const std::string stem = lowered(name.substr(0, name.find('.')));
    constexpr std::array<std::string_view, 4> plain{"con", "prn", "aux", "nul"};
    if (std::find(plain.begin(), plain.end(), stem) != plain.end()) {
        return true;
    }
    return stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
           stem[3] >= '1' && stem[3] <= '9';
}

// One directory or file name, as the header says.
bool is_portable_name(std::string_view name) {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
        return false;
    }
    constexpr std::string_view forbidden = "/\\:*?\"<>|";
    const bool characters_ok = std::all_of(name.begin(), name.end(), [&](char c) {
        return c >= 0x20 && c <= 0x7E && forbidden.find(c) == std::string_view::npos;
    });
    return characters_ok && !is_device_name(name);
}

// "\a\b\" as {"a", "b"}; "\" as {}.
TreeResult<std::vector<std::string>> split_pathname(std::string_view pathname) {
    if (pathname.empty() || pathname.front() != '\\' || pathname.back() != '\\') {
        return fail(TreeError::invalid_pathname, fs::path{std::string{pathname}});
    }
    std::vector<std::string> directories;
    std::size_t start = 1;
    while (start < pathname.size()) {
        const std::size_t end = pathname.find('\\', start);
        const std::string_view name = pathname.substr(start, end - start);
        if (!is_portable_name(name)) {
            return fail(TreeError::invalid_pathname, fs::path{std::string{pathname}});
        }
        directories.emplace_back(name);
        start = end + 1;
    }
    return directories;
}

std::string to_pathname(const std::vector<std::string>& directories) {
    std::string pathname = "\\";
    for (const std::string& directory : directories) {
        pathname += directory;
        pathname += '\\';
    }
    return pathname;
}

// The sequence number a member directory name stands for, or 0.
std::uint16_t parse_member_directory(std::string_view name) {
    if (name.size() != member_prefix.size() + 3 || !name.starts_with(member_prefix)) {
        return 0;
    }
    unsigned value = 0;
    for (const char c : name.substr(member_prefix.size())) {
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    return value <= max_members ? static_cast<std::uint16_t>(value) : 0;
}

// Everything that can be checked without touching the disk.
TreeResult<void> validate(std::span<const arinc665::MediaMember> members) {
    if (members.empty()) {
        return fail(TreeError::no_members);
    }
    std::set<std::uint16_t> sequence_numbers;
    for (const arinc665::MediaMember& member : members) {
        if (member.sequence_number < 1 || member.sequence_number > max_members ||
            !sequence_numbers.insert(member.sequence_number).second) {
            return fail(TreeError::invalid_member, std::to_string(member.sequence_number));
        }
        std::set<std::string> seen;
        for (const arinc665::MediaFile& file : member.files) {
            const auto directories = split_pathname(file.pathname);
            if (!directories) {
                return std::unexpected(directories.error());
            }
            if (!is_portable_name(file.name)) {
                return fail(TreeError::invalid_file_name, fs::path{file.name});
            }
            if (!seen.insert(lowered(to_pathname(*directories) + file.name)).second) {
                return fail(TreeError::duplicate_file, fs::path{file.pathname + file.name});
            }
        }
    }
    return {};
}

TreeResult<void> write_file(const fs::path& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return fail(TreeError::cannot_write_file, path);
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
        return fail(TreeError::cannot_write_file, path);
    }
    return {};
}

TreeResult<std::vector<std::byte>> read_file(const fs::path& path) {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec) {
        return fail(TreeError::cannot_read_file, path);
    }
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!in || (size != 0 && !in.read(reinterpret_cast<char*>(bytes.data()),
                                      static_cast<std::streamsize>(size)))) {
        return fail(TreeError::cannot_read_file, path);
    }
    return bytes;
}

// Adds the files below `directory` to `files`. `names` is the chain of
// directory names from the member's root down to it.
TreeResult<void> read_directory(const fs::path& directory, std::vector<std::string>& names,
                                std::vector<arinc665::MediaFile>& files) {
    std::error_code ec;
    fs::directory_iterator it(directory, ec);
    if (ec) {
        return fail(TreeError::cannot_read_file, directory);
    }
    for (const fs::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        const fs::path path = it->path();
        const std::string name = path.filename().string();
        if (!is_portable_name(name)) {
            return fail(TreeError::invalid_file_name, path);
        }
        const fs::file_status status = it->symlink_status(ec);
        if (ec) {
            return fail(TreeError::cannot_read_file, path);
        }
        if (fs::is_directory(status)) {
            names.push_back(name);
            if (auto below = read_directory(path, names, files); !below) {
                return below;
            }
            names.pop_back();
        } else if (fs::is_regular_file(status)) {
            auto bytes = read_file(path);
            if (!bytes) {
                return std::unexpected(bytes.error());
            }
            files.push_back(arinc665::MediaFile{to_pathname(names), name, std::move(*bytes)});
        } else {
            return fail(TreeError::unexpected_entry, path);
        }
    }
    if (ec) {
        return fail(TreeError::cannot_read_file, directory);
    }
    return {};
}

}  // namespace

std::string_view to_string(TreeError error) noexcept {
    switch (error) {
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

std::string member_directory_name(std::uint16_t sequence_number) {
    std::string name{member_prefix};
    name += static_cast<char>('0' + sequence_number / 100 % 10);
    name += static_cast<char>('0' + sequence_number / 10 % 10);
    name += static_cast<char>('0' + sequence_number % 10);
    return name;
}

TreeResult<fs::path> native_directory(const fs::path& base, std::string_view pathname) {
    const auto directories = split_pathname(pathname);
    if (!directories) {
        return std::unexpected(directories.error());
    }
    fs::path path = base;
    for (const std::string& directory : *directories) {
        path /= directory;
    }
    return path;
}

TreeResult<void> write_media_set_tree(const fs::path& root,
                                      std::span<const arinc665::MediaMember> members) {
    if (auto valid = validate(members); !valid) {
        return valid;
    }

    std::error_code ec;
    for (const arinc665::MediaMember& member : members) {
        const fs::path directory = root / member_directory_name(member.sequence_number);
        const fs::file_status status = fs::symlink_status(directory, ec);
        if (fs::exists(status) && (!fs::is_directory(status) || !fs::is_empty(directory, ec))) {
            return fail(TreeError::already_exists, directory);
        }
    }

    fs::create_directories(root, ec);
    if (ec || !fs::is_directory(root, ec)) {
        return fail(TreeError::cannot_create_directory, root);
    }
    for (const arinc665::MediaMember& member : members) {
        const fs::path member_root = root / member_directory_name(member.sequence_number);
        fs::create_directories(member_root, ec);
        if (ec) {
            return fail(TreeError::cannot_create_directory, member_root);
        }
        for (const arinc665::MediaFile& file : member.files) {
            const auto directory = native_directory(member_root, file.pathname);
            if (!directory) {
                return std::unexpected(directory.error());
            }
            fs::create_directories(*directory, ec);
            if (ec) {
                return fail(TreeError::cannot_create_directory, *directory);
            }
            if (auto written = write_file(*directory / file.name, file.bytes); !written) {
                return written;
            }
        }
    }
    return {};
}

TreeResult<std::vector<arinc665::MediaMember>> read_media_set_tree(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return fail(TreeError::not_a_directory, root);
    }

    std::vector<arinc665::MediaMember> members;
    fs::directory_iterator it(root, ec);
    if (ec) {
        return fail(TreeError::cannot_read_file, root);
    }
    for (const fs::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        const fs::path path = it->path();
        const std::uint16_t sequence_number = parse_member_directory(path.filename().string());
        const fs::file_status status = it->symlink_status(ec);
        if (ec || sequence_number == 0 || !fs::is_directory(status)) {
            return fail(TreeError::unexpected_entry, path);
        }
        arinc665::MediaMember member;
        member.sequence_number = sequence_number;
        std::vector<std::string> names;
        if (auto read = read_directory(path, names, member.files); !read) {
            return std::unexpected(read.error());
        }
        std::sort(member.files.begin(), member.files.end(),
                  [](const arinc665::MediaFile& a, const arinc665::MediaFile& b) {
                      return std::tie(a.pathname, a.name) < std::tie(b.pathname, b.name);
                  });
        members.push_back(std::move(member));
    }
    if (ec) {
        return fail(TreeError::cannot_read_file, root);
    }
    if (members.empty()) {
        return fail(TreeError::no_members, root);
    }
    std::sort(members.begin(), members.end(),
              [](const arinc665::MediaMember& a, const arinc665::MediaMember& b) {
                  return a.sequence_number < b.sequence_number;
              });
    return members;
}

}  // namespace adl::media_tree
