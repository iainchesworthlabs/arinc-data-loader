#include <memory>

#include "adl/platform/host.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

namespace adl::platform {
namespace {

class ZephyrHost final : public Host {
public:
    ZephyrHost() noexcept = default;

    [[nodiscard]] Result<void> start() noexcept {
        clock_ = zephyr::make_clock();
        memory_ = zephyr::make_memory();
        files_ = zephyr::make_files();
        if (!clock_ || !memory_ || !files_) {
            return fail(ErrorCode::resource_exhausted);
        }
        threads_ = zephyr::make_threading(*clock_);
        network_ = zephyr::make_network(*clock_);
        ipc_ = zephyr::make_ipc(*clock_);
        if (!threads_ || !network_ || !ipc_) {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    HostInfo info() const noexcept override { return HostInfo{"zephyr", true}; }
    Clock& clock() noexcept override { return *clock_; }
    MemoryProvider& memory() noexcept override { return *memory_; }
    FileSystem& files() noexcept override { return *files_; }
    Threading& threads() noexcept override { return *threads_; }
    Network& network() noexcept override { return *network_; }
    Ipc& ipc() noexcept override { return *ipc_; }

private:
    // Declared in the order they depend on each other, so that they are destroyed
    // in the reverse: the queues, sockets and threads before the clock they ask
    // the time of.
    std::unique_ptr<Clock> clock_;
    std::unique_ptr<MemoryProvider> memory_;
    std::unique_ptr<FileSystem> files_;
    std::unique_ptr<Threading> threads_;
    std::unique_ptr<Network> network_;
    std::unique_ptr<Ipc> ipc_;
};

}  // namespace

Result<std::unique_ptr<Host>> make_host() noexcept {
    // Nothing to start: Zephyr's kernel, network stack and mounted file systems
    // are up before main, and the product configured them (docs/building.md).
    auto host = detail::make_unique_nothrow<ZephyrHost>();
    if (!host) {
        return fail(ErrorCode::resource_exhausted);
    }
    if (auto started = host->start(); !started) {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<Host>(std::move(host));
}

}  // namespace adl::platform
