#include <memory>

#include "adl/platform/host.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

namespace adl::platform {
namespace {

class FreeRtosHost final : public Host {
public:
    FreeRtosHost() noexcept = default;

    [[nodiscard]] Result<void> start() noexcept {
        const freertos::Environment& product = freertos_os::environment();
        clock_ = freertos::make_clock();
        memory_ = freertos::make_memory();
        files_ = freertos::make_files(product.file_system);
        if (!clock_ || !memory_ || !files_) {
            return fail(ErrorCode::resource_exhausted);
        }
        threads_ = freertos::make_threading(*clock_, product.network_task_priority);
        network_ = freertos::make_network(*clock_);
        ipc_ = freertos::make_ipc(*clock_);
        if (!threads_ || !network_ || !ipc_) {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    HostInfo info() const noexcept override { return HostInfo{"freertos", true}; }
    Clock& clock() noexcept override { return *clock_; }
    MemoryProvider& memory() noexcept override { return *memory_; }
    FileSystem& files() noexcept override { return *files_; }
    Threading& threads() noexcept override { return *threads_; }
    Network& network() noexcept override { return *network_; }
    Ipc& ipc() noexcept override { return *ipc_; }

private:
    // Declared in the order they depend on each other, so that they are destroyed in the
    // reverse: the queues, sockets and threads before the clock they ask the time of.
    std::unique_ptr<Clock> clock_;
    std::unique_ptr<MemoryProvider> memory_;
    std::unique_ptr<FileSystem> files_;
    std::unique_ptr<Threading> threads_;
    std::unique_ptr<Network> network_;
    std::unique_ptr<Ipc> ipc_;
};

}  // namespace

Result<std::unique_ptr<Host>> make_host() noexcept {
    // Nothing to start: the scheduler, the network stack and the file system are running
    // before the loader asks for a Host, and the product set them up (environment.hpp).
    auto host = detail::make_unique_nothrow<FreeRtosHost>();
    if (!host) {
        return fail(ErrorCode::resource_exhausted);
    }
    if (auto started = host->start(); !started) {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<Host>(std::move(host));
}

}  // namespace adl::platform
