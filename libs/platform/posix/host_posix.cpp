// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/host.hpp"
#include "adl/platform/ring_ipc.hpp"
#include "detail/nothrow.hpp"
#include "posix/posix_os.hpp"

#include <memory>

namespace adl::platform
{
namespace
{

class PosixHost final : public Host
{
public:
    PosixHost() noexcept = default;

    [[nodiscard]] Result<void> start() noexcept
    {
        clock_ = posix::make_clock();
        memory_ = posix::make_memory();
        files_ = posix::make_files();
        if (!clock_ || !memory_ || !files_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        threads_ = posix::make_threading(*clock_);
        network_ = posix::make_network(*clock_);
        if (!threads_ || !network_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        ipc_ = detail::make_unique_nothrow<RingIpc>(*threads_);
        if (!ipc_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    HostInfo info() const noexcept override
    {
        return HostInfo{posix_os::host_name(), posix_os::is_real_time()};
    }
    Clock& clock() noexcept override
    {
        return *clock_;
    }
    MemoryProvider& memory() noexcept override
    {
        return *memory_;
    }
    FileSystem& files() noexcept override
    {
        return *files_;
    }
    Threading& threads() noexcept override
    {
        return *threads_;
    }
    Network& network() noexcept override
    {
        return *network_;
    }
    Ipc& ipc() noexcept override
    {
        return *ipc_;
    }

private:
    // Declared in the order they depend on each other, so that they are
    // destroyed in the reverse: the queues before the threading they use, and the
    // sockets and threads before the clock they ask the time of.
    std::unique_ptr<Clock> clock_;
    std::unique_ptr<MemoryProvider> memory_;
    std::unique_ptr<FileSystem> files_;
    std::unique_ptr<Threading> threads_;
    std::unique_ptr<Network> network_;
    std::unique_ptr<RingIpc> ipc_;
};

} // namespace

Result<std::unique_ptr<Host>> make_host() noexcept
{
    auto host = detail::make_unique_nothrow<PosixHost>();
    if (!host)
    {
        return fail(ErrorCode::resource_exhausted);
    }
    if (auto started = host->start(); !started)
    {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<Host>(std::move(host));
}

} // namespace adl::platform
