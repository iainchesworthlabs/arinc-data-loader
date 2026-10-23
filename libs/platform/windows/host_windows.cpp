#include "adl/platform/host.hpp"
#include "adl/platform/ring_ipc.hpp"
#include "detail/nothrow.hpp"
#include "windows/windows_services.hpp"

#include <memory>

namespace adl::platform
{
namespace
{

class WindowsHost final : public Host
{
public:
    [[nodiscard]] Result<void> start() noexcept
    {
        clock_ = windows::make_clock();
        memory_ = windows::make_memory();
        files_ = windows::make_files();
        if (!clock_ || !memory_ || !files_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        threads_ = windows::make_threading(*clock_);
        if (!threads_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        auto network = windows::make_network(*clock_);
        if (!network)
        {
            return std::unexpected(network.error());
        }
        network_ = std::move(*network);
        ipc_ = detail::make_unique_nothrow<RingIpc>(*threads_);
        if (!ipc_)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    HostInfo info() const noexcept override
    {
        return HostInfo{"windows", false};
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
    // In the order they depend on each other; destroyed in the reverse.
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
    auto host = detail::make_unique_nothrow<WindowsHost>();
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
