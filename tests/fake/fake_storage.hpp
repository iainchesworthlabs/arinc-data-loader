#pragma once

// Files and memory in memory. MemoryFileSystem is a whole file system held in
// RAM; it is what the engine runs against in tests, and it is also the shape of
// the answer for a target that has no file system at all (a load held in flash
// as a table of blobs would be served through this interface by a small adapter
// of the same kind). FakeMemory is a MemoryProvider with a budget, to test what
// happens when a start-up allocation fails.

#include "adl/platform/files.hpp"
#include "adl/platform/memory.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::fake
{

class MemoryFileSystem final : public platform::FileSystem
{
public:
    MemoryFileSystem();

    platform::Result<platform::Owned<platform::ReadFile>> open_read(std::string_view path) noexcept override;
    platform::Result<platform::Owned<platform::WriteFile>> create(std::string_view path,
                                                                  platform::CreateMode mode) noexcept override;
    platform::Result<platform::FileInfo> stat(std::string_view path) noexcept override;
    platform::Result<void> make_directory(std::string_view path) noexcept override;
    platform::Result<void> remove(std::string_view path) noexcept override;
    platform::Result<void> rename(std::string_view from, std::string_view to) noexcept override;
    platform::Result<void> list(std::string_view directory, platform::DirectoryVisitor visitor,
                                void* context) noexcept override;

    // ---- for tests --------------------------------------------------------------

    // Writes a whole file, making its directories; replaces what was there.
    void put(std::string_view path, std::string_view content);
    void put(std::string_view path, const std::vector<std::byte>& content);
    // The whole file, or nothing if there is no such file.
    [[nodiscard]] std::optional<std::vector<std::byte>> get(std::string_view path) const;

    // The most bytes all files together may hold; a write beyond it fails with
    // `no_space`, as a full flash does. Unlimited by default.
    void set_capacity(std::uint64_t bytes) noexcept
    {
        capacity_ = bytes;
    }
    [[nodiscard]] std::uint64_t used_bytes() const noexcept;

    // The next write fails with this error, once.
    void fail_next_write(platform::ErrorCode code) noexcept
    {
        write_failure_ = code;
    }

private:
    friend class MemoryWriteFile;
    struct Content
    {
        std::vector<std::byte> bytes;
    };
    struct Node
    {
        bool directory = false;
        std::shared_ptr<Content> content;
    };

    // Normalised absolute path ("/a/b"), or an error for a path the interface refuses.
    [[nodiscard]] platform::Result<std::string> normalise(std::string_view path) const;
    [[nodiscard]] static std::string parent_of(const std::string& path);

    std::map<std::string, Node> nodes_;
    std::uint64_t capacity_ = ~std::uint64_t{0};
    std::optional<platform::ErrorCode> write_failure_;
};

class FakeMemory final : public platform::MemoryProvider
{
public:
    platform::Result<platform::MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                                     platform::MemoryUse use) noexcept override;
    void release(std::span<std::byte> bytes) noexcept override;

    // At most this many bytes may be out at once; further requests fail with
    // `resource_exhausted`.
    void set_budget(std::size_t bytes) noexcept
    {
        budget_ = bytes;
    }
    // Refuse `resident` requests as a platform with no locked memory would.
    void deny_resident(bool deny) noexcept
    {
        deny_resident_ = deny;
    }
    [[nodiscard]] std::size_t outstanding_bytes() const noexcept
    {
        return outstanding_;
    }

private:
    std::size_t budget_ = ~std::size_t{0};
    std::size_t outstanding_ = 0;
    bool deny_resident_ = false;
};

} // namespace adl::fake
