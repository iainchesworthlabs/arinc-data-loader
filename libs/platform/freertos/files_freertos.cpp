// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/files.hpp"
#include "detail/nothrow.hpp"
#include "detail/path_rules.hpp"
#include "freertos/freertos_os.hpp"

#include <lfs.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace adl::platform::freertos
{
namespace
{

// littlefs reports a negative LFS_ERR_* code, each its own value rather than a system's
// errno.
[[nodiscard]] ErrorCode map_lfs(int error) noexcept
{
    switch (error)
    {
    case LFS_ERR_NOENT:
        return ErrorCode::not_found;
    case LFS_ERR_EXIST:
        return ErrorCode::already_exists;
    case LFS_ERR_NOTDIR:
        return ErrorCode::not_a_directory;
    case LFS_ERR_ISDIR:
        return ErrorCode::is_a_directory;
    case LFS_ERR_NOTEMPTY:
        return ErrorCode::directory_not_empty;
    case LFS_ERR_INVAL:
    case LFS_ERR_BADF:
        return ErrorCode::invalid_argument;
    case LFS_ERR_NAMETOOLONG:
        return ErrorCode::name_too_long;
    case LFS_ERR_NOSPC:
    case LFS_ERR_FBIG:
        return ErrorCode::no_space;
    case LFS_ERR_IO:
    case LFS_ERR_CORRUPT:
        return ErrorCode::io_error;
    case LFS_ERR_NOMEM:
        return ErrorCode::resource_exhausted;
    default:
        return ErrorCode::other;
    }
}

[[nodiscard]] std::unexpected<Error> fail_lfs(int error) noexcept
{
    return fail(map_lfs(error), error);
}

// The longest path handed to littlefs. Its own limit is per name (LFS_NAME_MAX); this is
// the whole path.
constexpr std::size_t max_path_bytes = 256;

// A path as a NUL-terminated string, built on the stack.
class CPath
{
public:
    explicit CPath(std::string_view path) noexcept
    {
        if (auto ok = detail::check_path(path); !ok)
        {
            error_ = ok.error();
            return;
        }
        if (path.size() >= buffer_.size())
        {
            error_ = Error{ErrorCode::name_too_long, 0};
            return;
        }
        std::memcpy(buffer_.data(), path.data(), path.size());
        buffer_[path.size()] = '\0';
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return !error_.has_value();
    }
    [[nodiscard]] Error error() const noexcept
    {
        return *error_;
    }
    [[nodiscard]] const char* c_str() const noexcept
    {
        return buffer_.data();
    }

private:
    std::array<char, max_path_bytes> buffer_{};
    std::optional<Error> error_;
};

[[nodiscard]] FileKind kind_of(const lfs_info& info) noexcept
{
    switch (info.type)
    {
    case LFS_TYPE_REG:
        return FileKind::regular;
    case LFS_TYPE_DIR:
        return FileKind::directory;
    default:
        return FileKind::other;
    }
}

// One open littlefs file. littlefs allocates the file's cache with its malloc, so a task
// that opens a file needs the heap that malloc uses.
struct OpenFile
{
    lfs_file_t file{};
    bool open = false;
};

class FreeRtosReadFile final : public ReadFile
{
public:
    FreeRtosReadFile(lfs_t& fs, const CPath& path) noexcept : fs_(fs), path_(path) {}
    ~FreeRtosReadFile() override
    {
        if (files_[active_].open)
        {
            (void)lfs_file_close(&fs_, &files_[active_].file);
        }
    }

    [[nodiscard]] Result<void> open() noexcept
    {
        const int error = lfs_file_open(&fs_, &files_[active_].file, path_.c_str(), LFS_O_RDONLY);
        if (error < 0)
        {
            return fail_lfs(error);
        }
        files_[active_].open = true;
        known_size_ = size_of(files_[active_].file);
        return {};
    }

    Result<std::uint64_t> size() noexcept override
    {
        refresh();
        return known_size_;
    }

    Result<std::size_t> read_at(std::uint64_t offset, std::span<std::byte> out) noexcept override
    {
        if (out.empty())
        {
            return std::size_t{0};
        }
        refresh();
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<lfs_soff_t>::max()))
        {
            return std::size_t{0};
        }
        lfs_file_t& file = files_[active_].file;
        const lfs_soff_t moved = lfs_file_seek(&fs_, &file, static_cast<lfs_soff_t>(offset), LFS_SEEK_SET);
        if (moved < 0)
        {
            return fail_lfs(static_cast<int>(moved));
        }
        std::size_t total = 0;
        while (total < out.size())
        {
            const auto wanted = static_cast<lfs_size_t>(
                std::min<std::size_t>(out.size() - total, std::numeric_limits<lfs_size_t>::max()));
            const lfs_ssize_t got = lfs_file_read(&fs_, &file, out.data() + total, wanted);
            if (got < 0)
            {
                return fail_lfs(static_cast<int>(got));
            }
            if (got == 0)
            {
                break;
            }
            total += static_cast<std::size_t>(got);
        }
        return total;
    }

private:
    [[nodiscard]] std::uint64_t size_of(lfs_file_t& file) noexcept
    {
        const lfs_soff_t size = lfs_file_size(&fs_, &file);
        return size < 0 ? 0 : static_cast<std::uint64_t>(size);
    }

    // The interface promises that a reader sees what a writer wrote by the time write()
    // returned. littlefs opens a file as it is and keeps that view for the life of the
    // handle, so the file is asked after (one lfs_stat per read) and opened again when it has
    // grown. A file that has been removed or replaced keeps being read through the handle it
    // already has, which is the other promise.
    void refresh() noexcept
    {
        lfs_info now{};
        if (lfs_stat(&fs_, path_.c_str(), &now) < 0 || now.type != LFS_TYPE_REG ||
            static_cast<std::uint64_t>(now.size) == known_size_)
        {
            return;
        }
        const unsigned other = 1U - active_;
        if (lfs_file_open(&fs_, &files_[other].file, path_.c_str(), LFS_O_RDONLY) < 0)
        {
            return;
        }
        files_[other].open = true;
        (void)lfs_file_close(&fs_, &files_[active_].file);
        files_[active_].open = false;
        active_ = other;
        known_size_ = size_of(files_[active_].file);
    }

    lfs_t& fs_;
    CPath path_;
    std::array<OpenFile, 2> files_{};
    unsigned active_ = 0;
    std::uint64_t known_size_ = 0;
};

class FreeRtosWriteFile final : public WriteFile
{
public:
    explicit FreeRtosWriteFile(lfs_t& fs) noexcept : fs_(fs) {}
    ~FreeRtosWriteFile() override
    {
        if (open_)
        {
            (void)lfs_file_close(&fs_, &file_);
        }
    }

    [[nodiscard]] Result<void> open(const char* path, int flags) noexcept
    {
        if (const int error = lfs_file_open(&fs_, &file_, path, flags); error < 0)
        {
            return fail_lfs(error);
        }
        open_ = true;
        return {};
    }

    Result<void> write(std::span<const std::byte> data) noexcept override
    {
        if (!open_)
        {
            return fail(ErrorCode::closed);
        }
        std::size_t done = 0;
        while (done < data.size())
        {
            const auto wanted = static_cast<lfs_size_t>(
                std::min<std::size_t>(data.size() - done, std::numeric_limits<lfs_size_t>::max()));
            const lfs_ssize_t put = lfs_file_write(&fs_, &file_, data.data() + done, wanted);
            if (put < 0)
            {
                return fail_lfs(static_cast<int>(put));
            }
            if (put == 0)
            {
                return fail(ErrorCode::no_space);
            }
            done += static_cast<std::size_t>(put);
            written_ += static_cast<std::uint64_t>(put);
        }
        // Pushed to the storage so that a reader on another handle sees it (see the reader).
        if (done != 0)
        {
            if (const int error = lfs_file_sync(&fs_, &file_); error < 0)
            {
                return fail_lfs(error);
            }
        }
        return {};
    }

    Result<std::uint64_t> size() noexcept override
    {
        if (!open_)
        {
            return fail(ErrorCode::closed);
        }
        return written_;
    }

    Result<void> sync() noexcept override
    {
        if (!open_)
        {
            return fail(ErrorCode::closed);
        }
        if (const int error = lfs_file_sync(&fs_, &file_); error < 0)
        {
            return fail_lfs(error);
        }
        return {};
    }

    Result<void> close() noexcept override
    {
        if (!open_)
        {
            return fail(ErrorCode::closed);
        }
        open_ = false;
        if (const int error = lfs_file_close(&fs_, &file_); error < 0)
        {
            return fail_lfs(error);
        }
        return {};
    }

private:
    lfs_t& fs_;
    lfs_file_t file_{};
    bool open_ = false;
    std::uint64_t written_ = 0;
};

class FreeRtosFileSystem final : public FileSystem
{
public:
    explicit FreeRtosFileSystem(lfs_t& fs) noexcept : fs_(fs) {}

    Result<Owned<ReadFile>> open_read(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        auto file = make_owned_nothrow<ReadFile, FreeRtosReadFile>(fs_, c);
        if (!file)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto opened = static_cast<FreeRtosReadFile&>(*file).open(); !opened)
        {
            return std::unexpected(opened.error());
        }
        return file;
    }

    Result<Owned<WriteFile>> create(std::string_view path, CreateMode mode) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        const int flags = LFS_O_WRONLY | LFS_O_CREAT | (mode == CreateMode::exclusive ? LFS_O_EXCL : LFS_O_TRUNC);
        auto file = make_owned_nothrow<WriteFile, FreeRtosWriteFile>(fs_);
        if (!file)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto opened = static_cast<FreeRtosWriteFile&>(*file).open(c.c_str(), flags); !opened)
        {
            return std::unexpected(opened.error());
        }
        // littlefs makes a new or emptied file real only when it is synced; until then
        // another handle, and stat, still see the old file or none.
        if (auto synced = file->sync(); !synced)
        {
            return std::unexpected(synced.error());
        }
        return file;
    }

    Result<FileInfo> stat(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        lfs_info info{};
        if (const int error = lfs_stat(&fs_, c.c_str(), &info); error < 0)
        {
            return fail_lfs(error);
        }
        const FileKind kind = kind_of(info);
        return FileInfo{kind, kind == FileKind::regular ? static_cast<std::uint64_t>(info.size) : std::uint64_t{0}};
    }

    Result<void> make_directory(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        if (const int error = lfs_mkdir(&fs_, c.c_str()); error < 0)
        {
            return fail_lfs(error);
        }
        return {};
    }

    Result<void> remove(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        if (const int error = lfs_remove(&fs_, c.c_str()); error < 0)
        {
            return fail_lfs(error);
        }
        return {};
    }

    Result<void> rename(std::string_view from, std::string_view to) noexcept override
    {
        const CPath a(from);
        if (!a.valid())
        {
            return std::unexpected(a.error());
        }
        const CPath b(to);
        if (!b.valid())
        {
            return std::unexpected(b.error());
        }
        // littlefs replaces a file already at the destination, atomically.
        if (const int error = lfs_rename(&fs_, a.c_str(), b.c_str()); error < 0)
        {
            return fail_lfs(error);
        }
        return {};
    }

    Result<void> list(std::string_view directory, DirectoryVisitor visitor, void* context) noexcept override
    {
        const CPath c(directory);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        lfs_dir_t dir{};
        if (const int error = lfs_dir_open(&fs_, &dir, c.c_str()); error < 0)
        {
            return fail_lfs(error);
        }
        for (;;)
        {
            lfs_info entry{};
            const int more = lfs_dir_read(&fs_, &dir, &entry);
            if (more < 0)
            {
                (void)lfs_dir_close(&fs_, &dir);
                return fail_lfs(more);
            }
            if (more == 0)
            {
                (void)lfs_dir_close(&fs_, &dir);
                return {};
            }
            const std::string_view name(entry.name);
            if (name == "." || name == "..")
            {
                continue;
            }
            if (!visitor(context, name, kind_of(entry)))
            {
                (void)lfs_dir_close(&fs_, &dir);
                return {};
            }
        }
    }

private:
    lfs_t& fs_;
};

// The file system of a Host made with no littlefs: every call says so.
class NoFileSystem final : public FileSystem
{
public:
    Result<Owned<ReadFile>> open_read(std::string_view) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<Owned<WriteFile>> create(std::string_view, CreateMode) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<FileInfo> stat(std::string_view) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<void> make_directory(std::string_view) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<void> remove(std::string_view) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<void> rename(std::string_view, std::string_view) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
    Result<void> list(std::string_view, DirectoryVisitor, void*) noexcept override
    {
        return fail(ErrorCode::unsupported);
    }
};

} // namespace

std::unique_ptr<FileSystem> make_files(::lfs* mounted) noexcept
{
    if (mounted == nullptr)
    {
        return detail::make_unique_nothrow<NoFileSystem>();
    }
    return detail::make_unique_nothrow<FreeRtosFileSystem>(*mounted);
}

} // namespace adl::platform::freertos
