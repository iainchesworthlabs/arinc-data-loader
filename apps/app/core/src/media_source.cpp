#include "adl/app/media_source.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <tuple>

#include "adl/arinc665/check_value_type.hpp"
#include "adl/media_tree/media_tree.hpp"

#include "adl/arinc665/sha1.hpp"
#include "adl/platform/paths.hpp"

#include <adl/profiling.hpp>

namespace adl::app {

namespace {

using adl::arinc615a::IoError;
using adl::arinc615a::IoFailure;

[[nodiscard]] IoFailure to_failure(const adl::platform::Error& error) noexcept {
    using adl::platform::ErrorCode;
    switch (error.code) {
        case ErrorCode::not_found:
            return {IoError::not_found, 0};
        case ErrorCode::permission_denied:
        case ErrorCode::invalid_argument:
        case ErrorCode::name_too_long:
            return {IoError::access_denied, 0};
        case ErrorCode::no_space:
            return {IoError::no_space, 0};
        default:
            return {IoError::io_error, 0};
    }
}

using Fingerprint = MediaSetIndex::Fingerprint;

// The fingerprint of bytes in memory.
[[nodiscard]] Fingerprint fingerprint_of(std::span<const std::byte> bytes) noexcept {
    adl::arinc665::Sha1 sha1;
    sha1.update(bytes);
    return {bytes.size(), sha1.digest()};
}

// The fingerprint of an open file, read in blocks from the start to its end.
[[nodiscard]] adl::platform::Result<Fingerprint> fingerprint_of(
    adl::platform::ReadFile& file) noexcept {
    const auto size = file.size();
    if (!size) {
        return std::unexpected(size.error());
    }
    adl::arinc665::Sha1 sha1;
    std::array<std::byte, 64 * 1024> block;
    std::uint64_t offset = 0;
    while (offset < *size) {
        const auto count = file.read_at(offset, block);
        if (!count) {
            return std::unexpected(count.error());
        }
        if (*count == 0) {
            break;  // shorter than its size said: what was read will not match
        }
        sha1.update(std::span{block}.first(*count));
        offset += *count;
    }
    return Fingerprint{offset, sha1.digest()};
}

}  // namespace

std::string MediaSetIndex::Entry::describe() const {
    return name + " (member " + std::to_string(member) + ", " + pathname + ")";
}

std::expected<MediaSetIndex, std::string> MediaSetIndex::make(
    const std::filesystem::path& root, const adl::arinc665::FilesList& list) {
    const adl::profiling::Zone zone{"app: index the media set"};
    MediaSetIndex index;
    index.entries_.reserve(list.files.size());
    for (const auto& file : list.files) {
        const auto directory = adl::media_tree::native_directory(
            root / adl::media_tree::member_directory_name(file.member_sequence_number),
            file.pathname);
        if (!directory) {
            return std::unexpected(
                std::string{adl::media_tree::to_string(directory.error().error)} + " (" +
                file.pathname + file.file_name + ")");
        }
        index.entries_.push_back(
            Entry{file.file_name, adl::platform::to_platform_path(*directory / file.file_name),
                  file.crc, file.member_sequence_number, file.pathname, std::nullopt});
    }
    return index;
}

std::expected<MediaSetIndex, std::string> MediaSetIndex::make(
    const std::filesystem::path& root, const adl::arinc665::FilesList& list,
    std::span<const adl::arinc665::MediaMember> members) {
    auto index = make(root, list);
    if (!index) {
        return index;
    }
    using Key = std::tuple<std::uint16_t, std::string_view, std::string_view>;
    std::map<Key, const adl::arinc665::MediaFile*> files;
    for (const adl::arinc665::MediaMember& member : members) {
        for (const adl::arinc665::MediaFile& file : member.files) {
            files.emplace(Key{member.sequence_number, file.pathname, file.name}, &file);
        }
    }
    for (Entry& entry : index->entries_) {
        const auto found = files.find(Key{entry.member, entry.pathname, entry.name});
        if (found == files.end()) {
            return std::unexpected("not in the media set on the disk: " + entry.describe());
        }
        entry.fingerprint = fingerprint_of(found->second->bytes);
    }
    return index;
}

std::optional<std::string> MediaSetIndex::changed_file(adl::platform::FileSystem& files) const {
    const adl::profiling::Zone zone{"app: verify the media set"};
    for (const Entry& entry : entries_) {
        if (!entry.fingerprint) {
            continue;
        }
        auto file = files.open_read(entry.path);
        if (!file) {
            return entry.describe();
        }
        const auto now = fingerprint_of(**file);
        if (!now || *now != *entry.fingerprint) {
            return entry.describe();
        }
    }
    return std::nullopt;
}

const MediaSetIndex::Entry* MediaSetIndex::find(std::string_view name,
                                                std::optional<std::uint16_t> crc16) const noexcept {
    const Entry* first = nullptr;
    std::size_t same_name = 0;
    for (const Entry& entry : entries_) {
        if (entry.name != name) {
            continue;
        }
        if (crc16 && entry.crc16 == *crc16) {
            return &entry;
        }
        if (first == nullptr) {
            first = &entry;
        }
        ++same_name;
    }
    return (same_name == 1 || (same_name > 1 && !crc16)) ? first : nullptr;
}

MediaTreeSource::MediaTreeSource(adl::platform::FileSystem& files, const MediaSetIndex& index)
    : files_(files), index_(index) {}

adl::arinc615a::IoResult<adl::arinc615a::SourceFile> MediaTreeSource::open(
    std::string_view name, const std::optional<adl::arinc615a::ChecksumOption>& checksum) noexcept {
    const adl::profiling::Zone zone{"app: open a media set file"};
    std::optional<std::uint16_t> crc16;
    if (checksum && checksum->type == adl::arinc665::CheckValueType::crc16 &&
        checksum->length == 2) {
        crc16 = static_cast<std::uint16_t>((std::to_integer<unsigned>(checksum->value[0]) << 8) |
                                           std::to_integer<unsigned>(checksum->value[1]));
    }
    const MediaSetIndex::Entry* entry = index_.find(name, crc16);
    if (entry == nullptr) {
        return std::unexpected(IoFailure{IoError::not_found, 0});
    }
    const auto slot = std::ranges::find_if(open_, [](const auto& file) { return !file; });
    if (slot == open_.end()) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    auto file = files_.open_read(entry->path);
    if (!file) {
        return std::unexpected(to_failure(file.error()));
    }
    const auto size = (*file)->size();
    if (!size) {
        return std::unexpected(to_failure(size.error()));
    }
    if (entry->fingerprint) {
        // Not what the set was when it was checked: nothing of it is uploaded. The reason is kept
        // for the operator, who the target's error would not tell.
        const auto now = fingerprint_of(**file);
        if (!now) {
            return std::unexpected(to_failure(now.error()));
        }
        if (*now != *entry->fingerprint) {
            if (!changed_) {
                changed_ = entry->describe();
            }
            return std::unexpected(IoFailure{IoError::access_denied, 0});
        }
    }
    *slot = std::move(*file);
    return adl::arinc615a::SourceFile{
        static_cast<adl::arinc615a::SourceHandle>(slot - open_.begin()), *size};
}

adl::arinc615a::IoResult<std::size_t> MediaTreeSource::read(adl::arinc615a::SourceHandle file,
                                                            std::uint64_t offset,
                                                            std::span<std::byte> out) noexcept {
    if (file >= open_.size() || !open_[file]) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    const auto count = open_[file]->read_at(offset, out);
    if (!count) {
        return std::unexpected(to_failure(count.error()));
    }
    return *count;
}

void MediaTreeSource::close(adl::arinc615a::SourceHandle file) noexcept {
    if (file < open_.size()) {
        open_[file].reset();
    }
}

}  // namespace adl::app
