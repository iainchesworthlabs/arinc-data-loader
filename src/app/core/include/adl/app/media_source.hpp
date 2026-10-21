#pragma once

// Where the files of an upload come from: a media set that is on disk as a tree, one directory
// per member (what adl::media_tree writes). The target asks for a file by its name and, when the
// load's header file gives one, the 16-bit CRC of the file (5.3.2.3.8.5); a media set may hold
// several files of one name (in different directories or on different members), and the CRC is
// what tells them apart. FILES.LUM lists every file of the set with its member, directory and
// CRC, so an index made from it is all that is needed to find the file on disk.
//
// What was checked is what is uploaded. When the set is read to be shown and checked, the bytes
// of every file are in memory, so the index takes a fingerprint of each one then (its size and
// its SHA-1: free at that moment, the bytes being there already). A file edited, replaced or
// taken away afterwards no longer has that fingerprint, and the set is not uploaded: the whole
// set is verified when an upload starts (one more read of it, as long as the check's own), and
// each file again when the target opens it. A hash of the content and not the file's size and
// modification time because the platform's file system interface offers no modification time,
// and because a time can be put back or be too coarse to see a second edit, where a changed
// byte always changes the hash.
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
#include "adl/arinc665/media_set.hpp"
#include "adl/arinc665/sha1.hpp"
#include "adl/platform/files.hpp"

namespace adl::app {

// The files of a media set and where each is: built once, when the set is read.
class MediaSetIndex {
public:
    // What a file was when the set was checked.
    struct Fingerprint {
        std::uint64_t size = 0;
        adl::arinc665::Sha1::Digest sha1{};

        friend bool operator==(const Fingerprint&, const Fingerprint&) = default;
    };

    struct Entry {
        std::string name;
        std::string path;  // UTF-8, '/' separated, absolute if the root was
        std::uint16_t crc16 = 0;
        std::uint16_t member = 0;
        std::string pathname;                    // the directory in the member, as FILES.LUM has it
        std::optional<Fingerprint> fingerprint;  // empty: the index was made without the bytes

        // "DATA.BIN (member 2, \LOAD\)": a file as the operator can find it.
        [[nodiscard]] std::string describe() const;
    };

    // Maps each file of `list` to a path below `root` (member directory, then the file's own
    // directory). Fails with a description if a pathname is not one a tree can hold.
    [[nodiscard]] static std::expected<MediaSetIndex, std::string> make(
        const std::filesystem::path& root, const adl::arinc665::FilesList& list);

    // As above, and takes the fingerprint of every file from `members`, the set as it was read
    // (and checked). Fails with a description if a listed file is not among them.
    [[nodiscard]] static std::expected<MediaSetIndex, std::string> make(
        const std::filesystem::path& root, const adl::arinc665::FilesList& list,
        std::span<const adl::arinc665::MediaMember> members);

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }

    // The description of the first file (in the order of FILES.LUM) that is missing from `files`
    // or no longer what it was when the index was made, reading each one in full; empty if the
    // set is as it was checked. Files without a fingerprint are not looked at. Blocks for as
    // long as the set takes to read.
    [[nodiscard]] std::optional<std::string> changed_file(adl::platform::FileSystem& files) const;

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

    // The description of the file that open() refused because it was not what the set was
    // when it was checked (the first one, if several); empty if there was none.
    [[nodiscard]] const std::optional<std::string>& changed_file() const noexcept {
        return changed_;
    }

private:
    adl::platform::FileSystem& files_;
    const MediaSetIndex& index_;
    std::array<adl::platform::Owned<adl::platform::ReadFile>, max_open> open_;
    std::optional<std::string> changed_;
};

}  // namespace adl::app
