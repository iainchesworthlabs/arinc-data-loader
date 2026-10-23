#pragma once

// Where the files of an operation come from and go to. The engine does not touch a file
// system: during an upload the target reads header and data files from the data loader, and
// during a download and an information operation it writes files to it, and the engine does
// both through the two interfaces below. The runner has implementations over the platform's
// files (runner.hpp); a test or an embedded loader supplies its own, over a media set in memory
// or a table of blobs in flash.
//
// Calls are made on the engine's thread and are expected to be quick: a read of one block, a
// write of one block. They report failure as a value and never throw.

#include "adl/arinc615a/checksum.hpp"
#include "adl/arinc615a/tftp.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace adl::arinc615a
{

enum class IoError : std::uint8_t
{
    not_found,     // there is no such file
    not_available, // there is, but not yet (media to be inserted): the target is told to wait
    access_denied, // the name is refused (not one the operation may touch)
    no_space,      // the storage is full
    io_error,      // anything else the storage reported
};

struct IoFailure
{
    IoError error = IoError::io_error;
    // For `not_available`: how long the target should wait before asking again (WAIT:x).
    std::uint16_t wait_seconds = 0;
};

template <typename T>
using IoResult = std::expected<T, IoFailure>;

[[nodiscard]] constexpr const char* to_string(IoError error) noexcept
{
    switch (error)
    {
    case IoError::not_found:
        return "file not found";
    case IoError::not_available:
        return "file not available yet";
    case IoError::access_denied:
        return "access denied";
    case IoError::no_space:
        return "no space";
    case IoError::io_error:
        return "I/O error";
    }
    return "unknown";
}

// ---- Files the target reads (an upload) ------------------------------------------------

using SourceHandle = std::uint32_t;

struct SourceFile
{
    SourceHandle handle = 0;
    std::uint64_t size = 0; // bytes; sent to the target as the "tsize" option
};

class FileSource
{
public:
    virtual ~FileSource() = default;

    // Opens the file the target asked for by `name`. `checksum` is the checksum option of the
    // request, if it sent one: the 16-bit CRC of the file's entry in the part header file, which
    // tells two files of one name apart (section 5.3.2.3.8.5). A source that holds one file of
    // the name ignores it.
    [[nodiscard]] virtual IoResult<SourceFile> open(std::string_view name,
                                                    const std::optional<ChecksumOption>& checksum) noexcept = 0;

    // Reads up to out.size() bytes at `offset`; fewer only at the end of the file.
    [[nodiscard]] virtual IoResult<std::size_t> read(SourceHandle file, std::uint64_t offset,
                                                     std::span<std::byte> out) noexcept = 0;

    virtual void close(SourceHandle file) noexcept = 0;
};

// ---- Files the target writes (a download) ------------------------------------------------

using SinkHandle = std::uint32_t;

// What is known when a received file is complete.
struct ReceivedFile
{
    std::uint64_t size = 0;
    // What the checksum option said about it. Whatever it is, the file is kept.
    ChecksumVerdict checksum = ChecksumVerdict::not_given;
};

class FileSink
{
public:
    virtual ~FileSink() = default;

    // Starts a file called `name`: one of the names the operation asked the target for, and
    // no other, so a sink need not defend itself against a path of the target's choosing (it
    // is still free to). `expected_size` is the "tsize" the target announced, if it did.
    [[nodiscard]] virtual IoResult<SinkHandle> open(std::string_view name,
                                                    std::optional<std::uint64_t> expected_size) noexcept = 0;

    [[nodiscard]] virtual IoResult<void> write(SinkHandle file, std::span<const std::byte> data) noexcept = 0;

    // The whole file has arrived: make it permanent.
    [[nodiscard]] virtual IoResult<void> finish(SinkHandle file, const ReceivedFile& received) noexcept = 0;

    // The transfer failed: throw away what was written.
    virtual void discard(SinkHandle file) noexcept = 0;
};

} // namespace adl::arinc615a
