// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <windows.h>

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "adl/platform/files.hpp"
#include "detail/nothrow.hpp"
#include "detail/path_rules.hpp"
#include "windows/windows_services.hpp"
// clang-format on

namespace adl::platform::windows
{
namespace
{

// A path converted from UTF-8 to the wide characters Windows wants, with '/'
// turned into '\'. Paths longer than this are refused with `name_too_long`
// rather than reaching for the \\?\ prefix; see docs/platform-abstraction.md.
class WidePath
{
public:
    static constexpr std::size_t capacity = 1024;

    explicit WidePath(std::string_view path) noexcept
    {
        if (auto ok = detail::check_path(path); !ok)
        {
            error_ = ok.error();
            return;
        }
        if (path.size() >= capacity)
        {
            error_ = Error{ErrorCode::name_too_long, 0};
            return;
        }
        const int length =
            ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
                                  buffer_.data(), static_cast<int>(capacity - 1));
        if (length <= 0)
        {
            error_ = Error{::GetLastError() == ERROR_INSUFFICIENT_BUFFER ? ErrorCode::name_too_long
                                                                         : ErrorCode::invalid_argument,
                           static_cast<int>(::GetLastError())};
            return;
        }
        buffer_[static_cast<std::size_t>(length)] = L'\0';
        for (std::size_t i = 0; i < static_cast<std::size_t>(length); ++i)
        {
            if (buffer_[i] == L'/')
            {
                buffer_[i] = L'\\';
            }
        }
        length_ = static_cast<std::size_t>(length);
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
    [[nodiscard]] const wchar_t* c_str() const noexcept
    {
        return buffer_.data();
    }
    [[nodiscard]] std::size_t length() const noexcept
    {
        return length_;
    }

private:
    std::array<wchar_t, capacity> buffer_{};
    std::size_t length_ = 0;
    std::optional<Error> error_;
};

class WindowsReadFile final : public ReadFile
{
public:
    explicit WindowsReadFile(HANDLE handle) noexcept : handle_(handle) {}
    ~WindowsReadFile() override
    {
        ::CloseHandle(handle_);
    }
    WindowsReadFile(const WindowsReadFile&) = delete;
    WindowsReadFile& operator=(const WindowsReadFile&) = delete;
    WindowsReadFile(WindowsReadFile&&) = delete;
    WindowsReadFile& operator=(WindowsReadFile&&) = delete;

    Result<std::uint64_t> size() noexcept override
    {
        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(handle_, &size))
        {
            return fail_win32(::GetLastError());
        }
        return static_cast<std::uint64_t>(size.QuadPart);
    }

    Result<std::size_t> read_at(std::uint64_t offset, std::span<std::byte> out) noexcept override
    {
        std::size_t total = 0;
        while (total < out.size())
        {
            const std::uint64_t position = offset + total;
            OVERLAPPED where{};
            where.Offset = static_cast<DWORD>(position & 0xffffffffu);
            where.OffsetHigh = static_cast<DWORD>(position >> 32);
            // ReadFile takes a 32-bit count; ask for at most 1 GiB at a time.
            const DWORD want = static_cast<DWORD>(out.size() - total > 0x40000000u ? 0x40000000u : out.size() - total);
            DWORD got = 0;
            if (!::ReadFile(handle_, out.data() + total, want, &got, &where))
            {
                const DWORD error = ::GetLastError();
                if (error == ERROR_HANDLE_EOF)
                {
                    break;
                }
                return fail_win32(error);
            }
            if (got == 0)
            {
                break;
            }
            total += got;
        }
        return total;
    }

private:
    HANDLE handle_;
};

class WindowsWriteFile final : public WriteFile
{
public:
    explicit WindowsWriteFile(HANDLE handle) noexcept : handle_(handle) {}
    ~WindowsWriteFile() override
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(handle_);
        }
    }
    WindowsWriteFile(const WindowsWriteFile&) = delete;
    WindowsWriteFile& operator=(const WindowsWriteFile&) = delete;
    WindowsWriteFile(WindowsWriteFile&&) = delete;
    WindowsWriteFile& operator=(WindowsWriteFile&&) = delete;

    Result<void> write(std::span<const std::byte> data) noexcept override
    {
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            return fail(ErrorCode::closed);
        }
        std::size_t done = 0;
        while (done < data.size())
        {
            const DWORD want = static_cast<DWORD>(data.size() - done > 0x40000000u ? 0x40000000u : data.size() - done);
            DWORD put = 0;
            if (!::WriteFile(handle_, data.data() + done, want, &put, nullptr))
            {
                return fail_win32(::GetLastError());
            }
            done += put;
            written_ += put;
        }
        return {};
    }

    Result<std::uint64_t> size() noexcept override
    {
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            return fail(ErrorCode::closed);
        }
        return written_;
    }

    Result<void> sync() noexcept override
    {
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            return fail(ErrorCode::closed);
        }
        if (!::FlushFileBuffers(handle_))
        {
            return fail_win32(::GetLastError());
        }
        return {};
    }

    Result<void> close() noexcept override
    {
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            return fail(ErrorCode::closed);
        }
        const HANDLE handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
        if (!::CloseHandle(handle))
        {
            return fail_win32(::GetLastError());
        }
        return {};
    }

private:
    HANDLE handle_;
    std::uint64_t written_ = 0;
};

[[nodiscard]] FileKind kind_of(DWORD attributes) noexcept
{
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? FileKind::directory : FileKind::regular;
}

class WindowsFileSystem final : public FileSystem
{
public:
    Result<Owned<ReadFile>> open_read(std::string_view path) noexcept override
    {
        const WidePath wide(path);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        // A reader sees what writers write, and others may delete or replace the
        // file while it is open, as on POSIX.
        const HANDLE handle =
            ::CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            // Opening a directory as a file is refused as access denied.
            const DWORD attributes = ::GetFileAttributesW(wide.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                return fail(ErrorCode::is_a_directory, static_cast<int>(error));
            }
            return fail_win32(error);
        }
        auto file = make_owned_nothrow<ReadFile, WindowsReadFile>(handle);
        if (!file)
        {
            ::CloseHandle(handle);
            return fail(ErrorCode::resource_exhausted);
        }
        return file;
    }

    Result<Owned<WriteFile>> create(std::string_view path, CreateMode mode) noexcept override
    {
        const WidePath wide(path);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        const HANDLE handle =
            ::CreateFileW(wide.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                          mode == CreateMode::exclusive ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            const DWORD attributes = ::GetFileAttributesW(wide.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                return fail(mode == CreateMode::exclusive ? ErrorCode::already_exists : ErrorCode::is_a_directory,
                            static_cast<int>(error));
            }
            return fail_win32(error);
        }
        auto file = make_owned_nothrow<WriteFile, WindowsWriteFile>(handle);
        if (!file)
        {
            ::CloseHandle(handle);
            return fail(ErrorCode::resource_exhausted);
        }
        return file;
    }

    Result<FileInfo> stat(std::string_view path) noexcept override
    {
        const WidePath wide(path);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!::GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &data))
        {
            return fail_win32(::GetLastError());
        }
        const FileKind kind = kind_of(data.dwFileAttributes);
        const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        return FileInfo{kind, kind == FileKind::regular ? size : 0};
    }

    Result<void> make_directory(std::string_view path) noexcept override
    {
        const WidePath wide(path);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        if (!::CreateDirectoryW(wide.c_str(), nullptr))
        {
            return fail_win32(::GetLastError());
        }
        return {};
    }

    Result<void> remove(std::string_view path) noexcept override
    {
        const WidePath wide(path);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        const DWORD attributes = ::GetFileAttributesW(wide.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return fail_win32(::GetLastError());
        }
        const BOOL removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? ::RemoveDirectoryW(wide.c_str())
                                                                          : ::DeleteFileW(wide.c_str());
        if (!removed)
        {
            return fail_win32(::GetLastError());
        }
        return {};
    }

    Result<void> rename(std::string_view from, std::string_view to) noexcept override
    {
        const WidePath a(from);
        if (!a.valid())
        {
            return std::unexpected(a.error());
        }
        const WidePath b(to);
        if (!b.valid())
        {
            return std::unexpected(b.error());
        }
        if (!::MoveFileExW(a.c_str(), b.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            return fail_win32(::GetLastError());
        }
        return {};
    }

    Result<void> list(std::string_view directory, DirectoryVisitor visitor, void* context) noexcept override
    {
        WidePath wide(directory);
        if (!wide.valid())
        {
            return std::unexpected(wide.error());
        }
        const DWORD attributes = ::GetFileAttributesW(wide.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return fail_win32(::GetLastError());
        }
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return fail(ErrorCode::not_a_directory);
        }
        std::array<wchar_t, WidePath::capacity + 3> pattern{};
        const std::size_t length = wide.length();
        if (length + 3 > pattern.size())
        {
            return fail(ErrorCode::name_too_long);
        }
        for (std::size_t i = 0; i < length; ++i)
        {
            pattern[i] = wide.c_str()[i];
        }
        std::size_t end = length;
        if (end != 0 && pattern[end - 1] != L'\\')
        {
            pattern[end++] = L'\\';
        }
        pattern[end++] = L'*';
        pattern[end] = L'\0';

        WIN32_FIND_DATAW found{};
        const HANDLE search =
            ::FindFirstFileExW(pattern.data(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);
        if (search == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            // An empty directory has "." and "..", so no match means no directory.
            return fail_win32(error == ERROR_FILE_NOT_FOUND ? ERROR_PATH_NOT_FOUND : error);
        }
        for (;;)
        {
            const std::wstring_view name(found.cFileName);
            if (name != L"." && name != L"..")
            {
                std::array<char, 1024> utf8{};
                const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, found.cFileName, static_cast<int>(name.size()),
                                                        utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
                if (bytes > 0)
                {
                    const FileKind kind = (found.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0
                                              ? FileKind::other
                                              : kind_of(found.dwFileAttributes);
                    if (!visitor(context, std::string_view(utf8.data(), static_cast<std::size_t>(bytes)), kind))
                    {
                        ::FindClose(search);
                        return {};
                    }
                }
            }
            if (!::FindNextFileW(search, &found))
            {
                const DWORD error = ::GetLastError();
                ::FindClose(search);
                if (error == ERROR_NO_MORE_FILES)
                {
                    return {};
                }
                return fail_win32(error);
            }
        }
    }
};

} // namespace

std::unique_ptr<FileSystem> make_files() noexcept
{
    return detail::make_unique_nothrow<WindowsFileSystem>();
}

} // namespace adl::platform::windows
