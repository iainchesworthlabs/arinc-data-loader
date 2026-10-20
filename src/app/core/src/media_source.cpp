#include "adl/app/media_source.hpp"

#include <algorithm>

#include "adl/arinc665/check_value_type.hpp"
#include "adl/media_tree/media_tree.hpp"

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

[[nodiscard]] std::string to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

}  // namespace

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
            Entry{file.file_name, to_utf8(*directory / file.file_name), file.crc});
    }
    return index;
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
