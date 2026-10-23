// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/files.hpp"
#include "detail/nothrow.hpp"
#include "detail/path_rules.hpp"
#include "posix/posix_os.hpp"

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace adl::platform::posix
{
namespace
{

using posix_os::fail_errno;

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
        // Callers ask for the error only when valid() is false. The fallback
        // keeps the access total; it is never reached by contract.
        return error_.value_or(Error{ErrorCode::invalid_argument, 0});
    }
    [[nodiscard]] const char* c_str() const noexcept
    {
        return buffer_.data();
    }

private:
    std::array<char, PATH_MAX> buffer_{};
    std::optional<Error> error_;
};

[[nodiscard]] FileKind kind_of(mode_t mode) noexcept
{
    if (S_ISREG(mode))
    {
        return FileKind::regular;
    }
    if (S_ISDIR(mode))
    {
        return FileKind::directory;
    }
    return FileKind::other;
}

class PosixReadFile final : public ReadFile
{
public:
    explicit PosixReadFile(int fd) noexcept : fd_(fd) {}
    ~PosixReadFile() override
    {
        ::close(fd_);
    }

    Result<std::uint64_t> size() noexcept override
    {
        struct stat st{};
        if (::fstat(fd_, &st) != 0)
        {
            return fail_errno(errno);
        }
        return static_cast<std::uint64_t>(st.st_size);
    }

    Result<std::size_t> read_at(std::uint64_t offset, std::span<std::byte> out) noexcept override
    {
        std::size_t total = 0;
        while (total < out.size())
        {
            const std::uint64_t position = offset + total;
            if (position > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
            {
                break;
            }
            const ssize_t got = ::pread(fd_, out.data() + total, out.size() - total, static_cast<off_t>(position));
            if (got < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return fail_errno(errno);
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
    int fd_;
};

class PosixWriteFile final : public WriteFile
{
public:
    explicit PosixWriteFile(int fd) noexcept : fd_(fd) {}
    ~PosixWriteFile() override
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
        }
    }

    Result<void> write(std::span<const std::byte> data) noexcept override
    {
        if (fd_ < 0)
        {
            return fail(ErrorCode::closed);
        }
        std::size_t done = 0;
        while (done < data.size())
        {
            const ssize_t put = ::write(fd_, data.data() + done, data.size() - done);
            if (put < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return fail_errno(errno);
            }
            done += static_cast<std::size_t>(put);
            written_ += static_cast<std::uint64_t>(put);
        }
        return {};
    }

    Result<std::uint64_t> size() noexcept override
    {
        if (fd_ < 0)
        {
            return fail(ErrorCode::closed);
        }
        return written_;
    }

    Result<void> sync() noexcept override
    {
        if (fd_ < 0)
        {
            return fail(ErrorCode::closed);
        }
        if (const int error = posix_os::flush_to_storage(fd_); error != 0)
        {
            return fail_errno(error);
        }
        return {};
    }

    Result<void> close() noexcept override
    {
        if (fd_ < 0)
        {
            return fail(ErrorCode::closed);
        }
        const int fd = std::exchange(fd_, -1);
        if (::close(fd) != 0)
        {
            return fail_errno(errno);
        }
        return {};
    }

private:
    int fd_;
    std::uint64_t written_ = 0;
};

class PosixFileSystem final : public FileSystem
{
public:
    Result<Owned<ReadFile>> open_read(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        const int fd = ::open(c.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
        {
            return fail_errno(errno);
        }
        struct stat st{};
        if (::fstat(fd, &st) == 0 && S_ISDIR(st.st_mode))
        {
            ::close(fd);
            return fail(ErrorCode::is_a_directory);
        }
        auto file = make_owned_nothrow<ReadFile, PosixReadFile>(fd);
        if (!file)
        {
            ::close(fd);
            return fail(ErrorCode::resource_exhausted);
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
        const int flags = O_WRONLY | O_CREAT | O_CLOEXEC | (mode == CreateMode::exclusive ? O_EXCL : O_TRUNC);
        const int fd = ::open(c.c_str(), flags, 0644);
        if (fd < 0)
        {
            return fail_errno(errno);
        }
        auto file = make_owned_nothrow<WriteFile, PosixWriteFile>(fd);
        if (!file)
        {
            ::close(fd);
            return fail(ErrorCode::resource_exhausted);
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
        struct stat st{};
        if (::stat(c.c_str(), &st) != 0)
        {
            return fail_errno(errno);
        }
        const FileKind kind = kind_of(st.st_mode);
        return FileInfo{kind, kind == FileKind::regular ? static_cast<std::uint64_t>(st.st_size) : 0};
    }

    Result<void> make_directory(std::string_view path) noexcept override
    {
        const CPath c(path);
        if (!c.valid())
        {
            return std::unexpected(c.error());
        }
        if (::mkdir(c.c_str(), 0755) != 0)
        {
            return fail_errno(errno);
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
        struct stat st{};
        if (::lstat(c.c_str(), &st) != 0)
        {
            return fail_errno(errno);
        }
        const int result = S_ISDIR(st.st_mode) ? ::rmdir(c.c_str()) : ::unlink(c.c_str());
        if (result != 0)
        {
            return fail_errno(errno);
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
        if (::rename(a.c_str(), b.c_str()) != 0)
        {
            return fail_errno(errno);
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
        DIR* dir = ::opendir(c.c_str());
        if (dir == nullptr)
        {
            return fail_errno(errno);
        }
        const int dir_fd = ::dirfd(dir);
        for (;;)
        {
            errno = 0;
            // readdir is thread-safe for distinct DIR streams, and this one is
            // local to the call; the check flags the function by name.
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            const dirent* entry = ::readdir(dir);
            if (entry == nullptr)
            {
                const int error = errno;
                ::closedir(dir);
                if (error != 0)
                {
                    return fail_errno(error);
                }
                return {};
            }
            const std::string_view name(entry->d_name);
            if (name == "." || name == "..")
            {
                continue;
            }
            // d_type is a hint that some file systems do not fill in, so ask.
            FileKind kind = FileKind::other;
            struct stat st{};
            if (::fstatat(dir_fd, entry->d_name, &st, 0) == 0)
            {
                kind = kind_of(st.st_mode);
            }
            if (!visitor(context, name, kind))
            {
                ::closedir(dir);
                return {};
            }
        }
    }
};

} // namespace

std::unique_ptr<FileSystem> make_files() noexcept
{
    return detail::make_unique_nothrow<PosixFileSystem>();
}

} // namespace adl::platform::posix
