#include "adl/platform/files.hpp"
#include "detail/nothrow.hpp"
#include "detail/path_rules.hpp"
#include "zephyr/zephyr_os.hpp"

#include <zephyr/fs/fs.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace adl::platform::zephyr
{
namespace
{

using zephyr_os::fail_errno;

// The longest path handed to fs_*: Zephyr paths start at a mount point
// ("/lfs/DNLD_DATA_.../file") and a name is at most CONFIG_FS_..._NAME_MAX, so
// this is generous.
constexpr std::size_t max_path_bytes = 256;

// See CONFIG_ADL_FS_COHERENT_HANDLES.
constexpr bool coherent_handles = IS_ENABLED(CONFIG_ADL_FS_COHERENT_HANDLES);

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

[[nodiscard]] FileKind kind_of(const fs_dirent& entry) noexcept
{
    switch (entry.type)
    {
    case FS_DIR_ENTRY_FILE:
        return FileKind::regular;
    case FS_DIR_ENTRY_DIR:
        return FileKind::directory;
    default:
        return FileKind::other;
    }
}

class ZephyrReadFile final : public ReadFile
{
public:
    explicit ZephyrReadFile(const CPath& path) noexcept : path_(path)
    {
        fs_file_t_init(&files_[0]);
        fs_file_t_init(&files_[1]);
    }
    ~ZephyrReadFile() override
    {
        if (open_)
        {
            (void)fs_close(&files_[active_]);
        }
    }

    [[nodiscard]] Result<void> open() noexcept
    {
        if (const int error = fs_open(&files_[active_], path_.c_str(), FS_O_READ); error != 0)
        {
            return fail_errno(error);
        }
        open_ = true;
        known_size_ = size_of(files_[active_]);
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
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        {
            return std::size_t{0};
        }
        fs_file_t& file = files_[active_];
        if (const int error = fs_seek(&file, static_cast<off_t>(offset), FS_SEEK_SET); error != 0)
        {
            return fail_errno(error);
        }
        std::size_t total = 0;
        while (total < out.size())
        {
            const ssize_t got = fs_read(&file, out.data() + total, out.size() - total);
            if (got < 0)
            {
                return fail_errno(static_cast<int>(got));
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
    [[nodiscard]] static std::uint64_t size_of(fs_file_t& file) noexcept
    {
        if (fs_seek(&file, 0, FS_SEEK_END) != 0)
        {
            return 0;
        }
        const off_t end = fs_tell(&file);
        return end < 0 ? 0 : static_cast<std::uint64_t>(end);
    }

    // The interface promises that a reader sees what a writer wrote by the time
    // write() returned. littlefs opens a file as it is and keeps that view for the life
    // of the handle, so the file is asked after (one fs_stat per read) and opened again
    // when it has grown. A file that has been removed or replaced keeps being read
    // through the handle it already has, which is the other promise.
    void refresh() noexcept
    {
        if (!coherent_handles)
        {
            return;
        }
        fs_dirent now{};
        if (fs_stat(path_.c_str(), &now) != 0 || now.type != FS_DIR_ENTRY_FILE ||
            static_cast<std::uint64_t>(now.size) == known_size_)
        {
            return;
        }
        const unsigned other = 1U - active_;
        fs_file_t_init(&files_[other]);
        if (fs_open(&files_[other], path_.c_str(), FS_O_READ) != 0)
        {
            return;
        }
        (void)fs_close(&files_[active_]);
        active_ = other;
        known_size_ = size_of(files_[active_]);
    }

    CPath path_;
    std::array<fs_file_t, 2> files_{};
    unsigned active_ = 0;
    bool open_ = false;
    std::uint64_t known_size_ = 0;
};

class ZephyrWriteFile final : public WriteFile
{
public:
    ZephyrWriteFile() noexcept
    {
        fs_file_t_init(&file_);
    }
    ~ZephyrWriteFile() override
    {
        if (open_)
        {
            (void)fs_close(&file_);
        }
    }

    [[nodiscard]] Result<void> open(const char* path, fs_mode_t flags) noexcept
    {
        if (const int error = fs_open(&file_, path, flags); error != 0)
        {
            return fail_errno(error);
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
            const ssize_t put = fs_write(&file_, data.data() + done, data.size() - done);
            if (put < 0)
            {
                return fail_errno(static_cast<int>(put));
            }
            if (put == 0)
            {
                return fail(ErrorCode::no_space);
            }
            done += static_cast<std::size_t>(put);
            written_ += static_cast<std::uint64_t>(put);
        }
        // The interface promises that a reader sees what a writer wrote by the time
        // write() returned. A caching file system only gives that to a reader on
        // the same open file, so the data is pushed to the storage (and the reader
        // looks for it, above). A product that would rather have the speed turns
        // this off and gets the weaker promise, "after sync() or close()".
        if (coherent_handles && done != 0)
        {
            if (const int error = fs_sync(&file_); error != 0)
            {
                return fail_errno(error);
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
        if (const int error = fs_sync(&file_); error != 0)
        {
            return fail_errno(error);
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
        if (const int error = fs_close(&file_); error != 0)
        {
            return fail_errno(error);
        }
        return {};
    }

private:
    fs_file_t file_{};
    bool open_ = false;
    std::uint64_t written_ = 0;
};

class ZephyrFileSystem final : public FileSystem
{
public:
    Result<Owned<ReadFile>> open_read(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        auto file = make_owned_nothrow<ReadFile, ZephyrReadFile>(c);
        if (!file)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto opened = static_cast<ZephyrReadFile&>(*file).open(); !opened)
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
        fs_mode_t flags = FS_O_CREATE | FS_O_WRITE;
        if (mode == CreateMode::exclusive)
        {
            // fs_open has no O_EXCL, so look first. A loader has one writer per
            // name (the engine numbers each download directory), so the window
            // between the look and the open is not one it can fall into.
            fs_dirent existing{};
            const int found = fs_stat(c.c_str(), &existing);
            if (found == 0)
            {
                return fail(ErrorCode::already_exists);
            }
            if (found != -ENOENT)
            {
                return fail_errno(found);
            }
        }
        else
        {
            flags |= FS_O_TRUNC;
        }
        auto file = make_owned_nothrow<WriteFile, ZephyrWriteFile>();
        if (!file)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto opened = static_cast<ZephyrWriteFile&>(*file).open(c.c_str(), flags); !opened)
        {
            return std::unexpected(opened.error());
        }
        // A file system that caches (littlefs does) makes a new or emptied file real
        // only when it is synced; until then another handle, and stat, still see the
        // old file or none.
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
        fs_dirent entry{};
        if (const int error = fs_stat(c.c_str(), &entry); error != 0)
        {
            return fail_errno(error);
        }
        const FileKind kind = kind_of(entry);
        return FileInfo{kind, kind == FileKind::regular ? static_cast<std::uint64_t>(entry.size) : std::uint64_t{0}};
    }

    Result<void> make_directory(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        if (const int error = fs_mkdir(c.c_str()); error != 0)
        {
            return fail_errno(error);
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
        if (const int error = fs_unlink(c.c_str()); error != 0)
        {
            return fail_errno(error);
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
        int error = fs_rename(a.c_str(), b.c_str());
        if (error == -EEXIST || error == -EBUSY || error == -EINVAL)
        {
            // A file system whose rename will not replace (FAT) is asked to
            // remove the old file first. That is not atomic, and the table in
            // docs/platform-abstraction.md says so. Only a regular file is
            // ever replaced.
            fs_dirent existing{};
            if (fs_stat(b.c_str(), &existing) == 0 && existing.type == FS_DIR_ENTRY_FILE && fs_unlink(b.c_str()) == 0)
            {
                error = fs_rename(a.c_str(), b.c_str());
            }
        }
        if (error != 0)
        {
            return fail_errno(error);
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
        fs_dir_t dir;
        fs_dir_t_init(&dir);
        if (const int error = fs_opendir(&dir, c.c_str()); error != 0)
        {
            return fail_errno(error);
        }
        for (;;)
        {
            fs_dirent entry{};
            const int error = fs_readdir(&dir, &entry);
            if (error != 0)
            {
                (void)fs_closedir(&dir);
                return fail_errno(error);
            }
            if (entry.name[0] == '\0')
            {
                (void)fs_closedir(&dir);
                return {};
            }
            const std::string_view name(entry.name);
            if (name == "." || name == "..")
            {
                continue;
            }
            if (!visitor(context, name, kind_of(entry)))
            {
                (void)fs_closedir(&dir);
                return {};
            }
        }
    }
};

} // namespace

std::unique_ptr<FileSystem> make_files() noexcept
{
    return detail::make_unique_nothrow<ZephyrFileSystem>();
}

} // namespace adl::platform::zephyr
