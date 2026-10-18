#pragma once

// Where the files of an upload come from: a media set that is on disk as a tree, one directory
// per member (what adl::media_tree writes). The target asks for a file by its name and, when the
// load's header file gives one, the 16-bit CRC of the file (5.3.2.3.8.5); a media set may hold
// several files of one name (in different directories or on different members), and the CRC is
// what tells them apart. FILES.LUM lists every file of the set with its member, directory and
// CRC, so an index made from it is all that is needed to find the file on disk.
//
// Nothing here is Qt, and the files are read through adl::platform::FileSystem, like the
// engine's own DirectoryFileSource, so a host that is not the desktop's (the tests' fake one)
// supplies the bytes.

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "adl/arinc615a/file_io.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/platform/files.hpp"

namespace adl::app {

// The files of a media set and where each is: built once, when the set is read.
class MediaSetIndex {
public:
    struct Entry {
        std::string name;
        std::string path;  // UTF-8, '/' separated, absolute if the root was
        std::uint16_t crc16 = 0;
    };

    // Maps each file of `list` to a path below `root` (member directory, then the file's own
    // directory). Fails with a description if a pathname is not one a tree can hold.
    [[nodiscard]] static std::expected<MediaSetIndex, std::string> make(
        const std::filesystem::path& root, const adl::arinc665::FilesList& list);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }

    // The entry for a request: the file called `name`, and among several of that name the one
    // whose CRC is `crc16`. With one file of the name, a CRC that differs is not a reason to
    // refuse it (the target will say if what it received does not match what it expected);
    // with several and no CRC that fits, there is no such file. Null if there is none.
    [[nodiscard]] const Entry* find(std::string_view name,
                                    std::optional<std::uint16_t> crc16) const noexcept;

private:
    std::vector<Entry> entries_;
};

// A FileSource over an index. Holds a reference to both, which have to outlive it.
class MediaTreeSource final : public adl::arinc615a::FileSource {
public:
    static constexpr std::size_t max_open = 8;

    MediaTreeSource(adl::platform::FileSystem& files, const MediaSetIndex& index);

    adl::arinc615a::IoResult<adl::arinc615a::SourceFile> open(
        std::string_view name,
        const std::optional<adl::arinc615a::ChecksumOption>& checksum) noexcept override;
    adl::arinc615a::IoResult<std::size_t> read(adl::arinc615a::SourceHandle file,
                                               std::uint64_t offset,
                                               std::span<std::byte> out) noexcept override;
    void close(adl::arinc615a::SourceHandle file) noexcept override;

private:
    adl::platform::FileSystem& files_;
    const MediaSetIndex& index_;
    std::array<adl::platform::Owned<adl::platform::ReadFile>, max_open> open_;
};

}  // namespace adl::app
