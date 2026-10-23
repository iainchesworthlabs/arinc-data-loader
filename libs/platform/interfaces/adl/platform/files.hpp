#pragma once

// Files and storage. What the loader does with a file system is small: read
// the files of a load at the offsets a TFTP block asks for, and write the
// files a target sends, one block after another. So the interface is that and
// no more, and it leaves out everything that std::filesystem brings
// (iterators, path objects, error_code and an allocation for each).
//
// Paths are UTF-8 text with '/' as the separator on every platform, absolute
// or relative to wherever the platform resolves relative paths (the current
// directory on a desktop, the volume root on an RTOS). A path is not empty and
// holds no NUL and no backslash; the platform refuses such a path with
// `invalid_argument` rather than let a backslash mean a separator on Windows
// and a letter on Linux. Names are case-sensitive as far as this interface is
// concerned: a caller never relies on two names differing only in case being
// different files.
//
// None of the calls allocate on the caller's behalf except the handles
// returned by open_read and create (an Owned<T>, which a platform with no heap
// draws from a fixed pool).

#include "adl/platform/error.hpp"
#include "adl/platform/owned.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace adl::platform
{

enum class FileKind : std::uint8_t
{
    regular,
    directory,
    other
};

struct FileInfo
{
    FileKind kind = FileKind::regular;
    std::uint64_t size = 0; // bytes; 0 for a directory
};

// A file open for reading. Several may be open on one file; a reader sees
// what a writer has written to it by the time the writer's call returned.
class ReadFile
{
public:
    virtual ~ReadFile() = default;

    [[nodiscard]] virtual Result<std::uint64_t> size() noexcept = 0;

    // Reads up to out.size() bytes starting at `offset`. A short count means the
    // end of the file was reached; 0 means `offset` is at or past it. It never
    // returns a short count for any other reason, so a caller can fill a TFTP
    // block with one call.
    [[nodiscard]] virtual Result<std::size_t> read_at(std::uint64_t offset, std::span<std::byte> out) noexcept = 0;
};

enum class CreateMode : std::uint8_t
{
    exclusive, // fail with `already_exists` if the name is taken: never overwrite
    replace,   // empty the file if it is there
};

// A file open for writing, positioned at the start; every write appends.
class WriteFile
{
public:
    virtual ~WriteFile() = default;

    // All of `data` or an error (`no_space` when the storage fills); never a
    // partial write the caller has to resume.
    [[nodiscard]] virtual Result<void> write(std::span<const std::byte> data) noexcept = 0;

    [[nodiscard]] virtual Result<std::uint64_t> size() noexcept = 0;

    // Asks the storage to make what was written durable. A file system with no
    // cache treats it as a no-op.
    [[nodiscard]] virtual Result<void> sync() noexcept = 0;

    // Releases the file and reports what the destructor cannot: the last flush
    // failing, as it can on a full disk or a network file system. After close()
    // the object accepts no more calls. The destructor closes a file the caller
    // did not, and ignores the error.
    [[nodiscard]] virtual Result<void> close() noexcept = 0;
};

// Called once per entry of a directory with the entry's name (not a path), in
// no particular order. Return false to stop. `context` is the caller's.
using DirectoryVisitor = bool (*)(void* context, std::string_view name, FileKind kind) noexcept;

class FileSystem
{
public:
    virtual ~FileSystem() = default;

    [[nodiscard]] virtual Result<Owned<ReadFile>> open_read(std::string_view path) noexcept = 0;

    [[nodiscard]] virtual Result<Owned<WriteFile>> create(std::string_view path, CreateMode mode) noexcept = 0;

    [[nodiscard]] virtual Result<FileInfo> stat(std::string_view path) noexcept = 0;

    // The parent has to exist. `already_exists` if the path is taken, by a
    // directory or by anything else.
    [[nodiscard]] virtual Result<void> make_directory(std::string_view path) noexcept = 0;

    // A file, or an empty directory (`directory_not_empty` otherwise).
    [[nodiscard]] virtual Result<void> remove(std::string_view path) noexcept = 0;

    // Moves a file to `to`, replacing a regular file already there. Together
    // with `create` it makes a download atomic: write to a temporary name,
    // verify, rename.
    [[nodiscard]] virtual Result<void> rename(std::string_view from, std::string_view to) noexcept = 0;

    // Visits the entries of a directory, not "." and "..".
    [[nodiscard]] virtual Result<void> list(std::string_view directory, DirectoryVisitor visitor,
                                            void* context) noexcept = 0;
};

// Convenience over `list` for a callable, so a caller need not write the
// function-pointer plumbing. `visit(std::string_view name, FileKind kind)`
// returns false to stop.
template <class Visit>
[[nodiscard]] Result<void> for_each_entry(FileSystem& files, std::string_view directory, Visit&& visit) noexcept
{
    auto callable = std::forward<Visit>(visit);
    using Callable = decltype(callable);
    struct Thunk
    {
        static bool call(void* context, std::string_view name, FileKind kind) noexcept
        {
            return (*static_cast<Callable*>(context))(name, kind);
        }
    };
    return files.list(directory, &Thunk::call, std::addressof(callable));
}

} // namespace adl::platform
