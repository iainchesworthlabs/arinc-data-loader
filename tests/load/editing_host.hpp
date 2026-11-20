// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A host that is another host, except that a file is edited when a chosen number of files have been opened
// for reading: the way to make a media set change after it was checked and before the target reads from it.

#include "adl/platform/host.hpp"
#include "fake_host.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace adl::test
{

class EditingHost final : public adl::platform::Host
{
public:
    explicit EditingHost(adl::fake::FakeHost& inner) : inner_(inner), files_(*this, inner.files()) {}

    // The file at `path` becomes `content` as the file after `opens` others is opened for reading.
    void edit_after_opens(unsigned opens, std::string path, std::string content)
    {
        edit_after_ = opens;
        path_ = std::move(path);
        content_ = std::move(content);
    }

    adl::platform::HostInfo info() const noexcept override
    {
        return inner_.info();
    }
    adl::platform::Clock& clock() noexcept override
    {
        return inner_.clock();
    }
    adl::platform::MemoryProvider& memory() noexcept override
    {
        return inner_.memory();
    }
    adl::platform::FileSystem& files() noexcept override
    {
        return files_;
    }
    adl::platform::Threading& threads() noexcept override
    {
        return inner_.threads();
    }
    adl::platform::Network& network() noexcept override
    {
        return inner_.network();
    }
    adl::platform::Ipc& ipc() noexcept override
    {
        return inner_.ipc();
    }

private:
    class Files final : public adl::platform::FileSystem
    {
    public:
        Files(EditingHost& host, adl::platform::FileSystem& inner) : host_(host), inner_(inner) {}

        adl::platform::Result<adl::platform::Owned<adl::platform::ReadFile>>
        open_read(std::string_view path) noexcept override
        {
            if (host_.opened_++ == host_.edit_after_)
            {
                host_.inner_.fake_files().put(host_.path_, host_.content_);
            }
            return inner_.open_read(path);
        }
        adl::platform::Result<adl::platform::Owned<adl::platform::WriteFile>>
        create(std::string_view path, adl::platform::CreateMode mode) noexcept override
        {
            return inner_.create(path, mode);
        }
        adl::platform::Result<adl::platform::FileInfo> stat(std::string_view path) noexcept override
        {
            return inner_.stat(path);
        }
        adl::platform::Result<void> make_directory(std::string_view path) noexcept override
        {
            return inner_.make_directory(path);
        }
        adl::platform::Result<void> remove(std::string_view path) noexcept override
        {
            return inner_.remove(path);
        }
        adl::platform::Result<void> rename(std::string_view from, std::string_view to) noexcept override
        {
            return inner_.rename(from, to);
        }
        adl::platform::Result<void> list(std::string_view directory, adl::platform::DirectoryVisitor visitor,
                                         void* context) noexcept override
        {
            return inner_.list(directory, visitor, context);
        }

    private:
        EditingHost& host_;
        adl::platform::FileSystem& inner_;
    };

    adl::fake::FakeHost& inner_;
    Files files_;
    unsigned edit_after_ = ~0U;
    unsigned opened_ = 0;
    std::string path_;
    std::string content_;
};

} // namespace adl::test
