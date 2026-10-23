// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// The Host: ipflite_client_initialize() once, before the services, then the
// clock, the static memory, the (unsupported) file system, the threading
// over the clock, the network and the portable RingIpc over the threading.
//
// Assumptions named: ipflite_client_initialize is per task ("to be called by
// every ipflite task", ipflite.h), so this call initialises the task that
// builds the Host; a task that later uses sockets - the runner, in the
// static-task model this port runs in, since Threading::spawn is unsupported
// - must call it for itself and open its own sockets, because the
// descriptors index the calling task's table. There is nothing to undo when
// make_host() fails part-way: the kernel objects are owned by the partition,
// so the Host's destructor (unlike the Windows one) undoes nothing.

#include <INTEGRITY.h>
#include <ipflite/ipflite.h>

#include <memory>

#include "adl/platform/host.hpp"
#include "adl/platform/ring_ipc.hpp"
#include "detail/nothrow.hpp"
#include "ghs/ghs_native.hpp"
#include "ghs178b/ghs178b_services.hpp"

namespace adl::platform {
namespace {

class Ghs178bHost final : public Host {
public:
    Ghs178bHost() noexcept = default;

    [[nodiscard]] Result<void> start() noexcept {
        const Error initialized = ipflite_client_initialize();
        if (initialized != Success) {
            return ghs::fail_native(initialized);
        }
        clock_ = ghs178b::make_clock();
        memory_ = ghs178b::make_memory();
        files_ = ghs178b::make_files();
        if (!clock_ || !memory_ || !files_) {
            return fail(ErrorCode::resource_exhausted);
        }
        threads_ = ghs178b::make_threading(*clock_);
        network_ = ghs178b::make_network();
        if (!threads_ || !network_) {
            return fail(ErrorCode::resource_exhausted);
        }
        ipc_ = detail::make_unique_nothrow<RingIpc>(*threads_);
        if (!ipc_) {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    HostInfo info() const noexcept override { return HostInfo{"integrity", true}; }

    Clock& clock() noexcept override { return *clock_; }
    MemoryProvider& memory() noexcept override { return *memory_; }
    FileSystem& files() noexcept override { return *files_; }
    Threading& threads() noexcept override { return *threads_; }
    Network& network() noexcept override { return *network_; }
    Ipc& ipc() noexcept override { return *ipc_; }

private:
    // Declared in the order they depend on each other, so that they are
    // destroyed in the reverse: the queues before the threading they use,
    // and the sockets and threads before the clock they ask the time of
    // (posix/host_posix.cpp carries the same note).
    std::unique_ptr<Ipc> ipc_;
    std::unique_ptr<Threading> threads_;
    std::unique_ptr<FileSystem> files_;
    std::unique_ptr<Network> network_;
    std::unique_ptr<MemoryProvider> memory_;
    std::unique_ptr<Clock> clock_;
};

}  // namespace

Result<std::unique_ptr<Host>> make_host() noexcept {
    auto host = detail::make_unique_nothrow<Ghs178bHost>();
    if (!host) {
        return fail(ErrorCode::resource_exhausted);
    }
    if (auto started = host->start(); !started) {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<Host>(std::move(host));
}

}  // namespace adl::platform
