#include "adl/arinc615a/directory_files.hpp"

#include <algorithm>

namespace adl::arinc615a {
namespace {

constexpr std::string_view temporary_suffix = ".part";

[[nodiscard]] IoFailure to_failure(const adl::platform::Error& error) noexcept {
    using adl::platform::ErrorCode;
    switch (error.code) {
        case ErrorCode::not_found:
            return {IoError::not_found, 0};
        case ErrorCode::permission_denied:
        case ErrorCode::already_exists:
        case ErrorCode::invalid_argument:
        case ErrorCode::name_too_long:
            return {IoError::access_denied, 0};
        case ErrorCode::no_space:
            return {IoError::no_space, 0};
        default:
            return {IoError::io_error, 0};
    }
}

[[nodiscard]] std::string join(std::string_view directory, std::string_view name,
                               std::string_view suffix = {}) {
    std::string path;
    path.reserve(directory.size() + 1 + name.size() + suffix.size());
    path.append(directory);
    if (!path.empty() && path.back() != '/') {
        path.push_back('/');
    }
    path.append(name);
    path.append(suffix);
    return path;
}

}  // namespace

bool is_safe_file_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > 254 || name == "." || name == "..") {
        return false;
    }
    return std::ranges::none_of(name, [](char c) {
        return static_cast<unsigned char>(c) < 0x20 || c == '/' || c == '\\' || c == 0x7F;
    });
}

// ---- DirectoryFileSource --------------------------------------------------------------------

DirectoryFileSource::DirectoryFileSource(adl::platform::FileSystem& files,
                                         std::string_view directory)
    : files_(files), directory_(directory) {}

IoResult<SourceFile> DirectoryFileSource::open(
    std::string_view name, const std::optional<ChecksumOption>& /*checksum*/) noexcept {
    // A directory holds one file of a name, so there is nothing for the checksum to choose
    // between.
    if (!is_safe_file_name(name)) {
        return std::unexpected(IoFailure{IoError::access_denied, 0});
    }
    const auto slot = std::ranges::find_if(open_, [](const auto& file) { return !file; });
    if (slot == open_.end()) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    auto file = files_.open_read(join(directory_, name));
    if (!file) {
        return std::unexpected(to_failure(file.error()));
    }
    const auto size = (*file)->size();
    if (!size) {
        return std::unexpected(to_failure(size.error()));
    }
    *slot = std::move(*file);
    return SourceFile{static_cast<SourceHandle>(slot - open_.begin()), *size};
}

IoResult<std::size_t> DirectoryFileSource::read(SourceHandle file, std::uint64_t offset,
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

void DirectoryFileSource::close(SourceHandle file) noexcept {
    if (file < open_.size()) {
        open_[file].reset();
    }
}

// ---- DirectoryFileSink ----------------------------------------------------------------------

DirectoryFileSink::DirectoryFileSink(adl::platform::FileSystem& files, std::string_view directory)
    : files_(files), directory_(directory) {}

IoResult<SinkHandle> DirectoryFileSink::open(std::string_view name,
                                             std::optional<std::uint64_t> /*expected_size*/) noexcept {
    if (!is_safe_file_name(name)) {
        return std::unexpected(IoFailure{IoError::access_denied, 0});
    }
    const auto slot = std::ranges::find_if(open_, [](const Open& open) { return !open.file; });
    if (slot == open_.end()) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    std::string final_path = join(directory_, name);
    if (files_.stat(final_path)) {
        return std::unexpected(IoFailure{IoError::access_denied, 0});  // never overwrite
    }
    std::string temporary_path = join(directory_, name, temporary_suffix);
    auto file = files_.create(temporary_path, adl::platform::CreateMode::replace);
    if (!file) {
        return std::unexpected(to_failure(file.error()));
    }
    slot->file = std::move(*file);
    slot->final_path = std::move(final_path);
    slot->temporary_path = std::move(temporary_path);
    return static_cast<SinkHandle>(slot - open_.begin());
}

IoResult<void> DirectoryFileSink::write(SinkHandle file, std::span<const std::byte> data) noexcept {
    if (file >= open_.size() || !open_[file].file) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    if (const auto written = open_[file].file->write(data); !written) {
        return std::unexpected(to_failure(written.error()));
    }
    return {};
}

IoResult<void> DirectoryFileSink::finish(SinkHandle file, const ReceivedFile& /*received*/) noexcept {
    if (file >= open_.size() || !open_[file].file) {
        return std::unexpected(IoFailure{IoError::io_error, 0});
    }
    Open& open = open_[file];
    const auto closed = open.file->close();
    open.file.reset();
    if (!closed) {
        static_cast<void>(files_.remove(open.temporary_path));
        return std::unexpected(to_failure(closed.error()));
    }
    if (const auto renamed = files_.rename(open.temporary_path, open.final_path); !renamed) {
        static_cast<void>(files_.remove(open.temporary_path));
        return std::unexpected(to_failure(renamed.error()));
    }
    return {};
}

void DirectoryFileSink::discard(SinkHandle file) noexcept {
    if (file >= open_.size() || !open_[file].file) {
        return;
    }
    Open& open = open_[file];
    open.file.reset();
    static_cast<void>(files_.remove(open.temporary_path));
}

}  // namespace adl::arinc615a
