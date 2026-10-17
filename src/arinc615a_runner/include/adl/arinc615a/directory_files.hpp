#pragma once

// A FileSource and FileSink over a directory of the platform's file system: the simplest
// places for the load's files to be read from and the target's files to be written to. A
// name the target (or the operation) gives is a name and nothing more: no separator, no
// "..", no control character, 254 characters at most, so that no path of the target's choosing
// reaches the file system.
//
// The sink writes each file under a temporary name and renames it when the whole file has
// arrived, so a transfer that fails leaves no half file, and it never overwrites a file that
// is already there.

#include <array>
#include <string>

#include "adl/arinc615a/file_io.hpp"
#include "adl/platform/files.hpp"

namespace adl::arinc615a {

// Whether `name` may be used as a file name inside a directory.
[[nodiscard]] bool is_safe_file_name(std::string_view name) noexcept;

class DirectoryFileSource final : public FileSource {
public:
    static constexpr std::size_t max_open = 8;

    DirectoryFileSource(adl::platform::FileSystem& files, std::string_view directory);

    IoResult<SourceFile> open(std::string_view name,
                              const std::optional<ChecksumOption>& checksum) noexcept override;
    IoResult<std::size_t> read(SourceHandle file, std::uint64_t offset,
                               std::span<std::byte> out) noexcept override;
    void close(SourceHandle file) noexcept override;

private:
    adl::platform::FileSystem& files_;
    std::string directory_;
    std::array<adl::platform::Owned<adl::platform::ReadFile>, max_open> open_;
};

class DirectoryFileSink final : public FileSink {
public:
    static constexpr std::size_t max_open = 8;

    DirectoryFileSink(adl::platform::FileSystem& files, std::string_view directory);

    IoResult<SinkHandle> open(std::string_view name,
                              std::optional<std::uint64_t> expected_size) noexcept override;
    IoResult<void> write(SinkHandle file, std::span<const std::byte> data) noexcept override;
    IoResult<void> finish(SinkHandle file, const ReceivedFile& received) noexcept override;
    void discard(SinkHandle file) noexcept override;

private:
    struct Open {
        adl::platform::Owned<adl::platform::WriteFile> file;
        std::string final_path;
        std::string temporary_path;
    };

    adl::platform::FileSystem& files_;
    std::string directory_;
    std::array<Open, max_open> open_;
};

}  // namespace adl::arinc615a
