// SPDX-License-Identifier: GPL-3.0-or-later
// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// The honest unsupported FileSystem. The 178B tree declares no application
// file API: fcntl.h has open, creat and fcntl, and read, write, lseek,
// close, mkdir, opendir/readdir, stat, rename and unlink are declared
// nowhere in the tree (docs/platform-ghs.md, "What the 178B tree does not
// provide"). A product supplies a storage adapter over its own flash table;
// that adapter is where this class is replaced, and every method below says
// unsupported until it is.

#include "adl/platform/files.hpp"
#include "detail/nothrow.hpp"
#include "ghs178b/ghs178b_services.hpp"

#include <memory>

namespace adl::platform::ghs178b
{
namespace
{

class UnsupportedFileSystem final : public FileSystem
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

std::unique_ptr<FileSystem> make_files() noexcept
{
    return detail::make_unique_nothrow<UnsupportedFileSystem>();
}

} // namespace adl::platform::ghs178b
