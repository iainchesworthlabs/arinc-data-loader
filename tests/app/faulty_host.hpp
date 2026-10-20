#pragma once

// A host that is another host until a test makes its network or its files misbehave: the way to
// reach the branches of the worker that report a loader that could not run (a request the loader
// refuses, a port that cannot be opened, a wait that fails).

#include "adl/platform/host.hpp"
#include "fake_host.hpp"
#include "faulty_files.hpp"

namespace adl::test {

// The network of another host, with the calls a test can fail.
class FaultyNetwork final : public adl::platform::Network {
public:
    explicit FaultyNetwork(adl::platform::Network& inner) : inner_(inner) {}

    // The next call (and every one after it) fails with this error.
    std::optional<adl::platform::ErrorCode> open_fails;
    std::optional<adl::platform::ErrorCode> wait_fails;

    adl::platform::Result<adl::platform::Owned<adl::platform::UdpSocket>> open_udp(
        const adl::platform::UdpOptions& options) noexcept override {
        if (open_fails) {
            return adl::platform::fail(*open_fails);
        }
        return inner_.open_udp(options);
    }
    adl::platform::Result<std::size_t> wait_readable(
        std::span<adl::platform::UdpSocket* const> sockets, adl::platform::Duration timeout,
        std::span<bool> ready) noexcept override {
        if (wait_fails) {
            return adl::platform::fail(*wait_fails);
        }
        return inner_.wait_readable(sockets, timeout, ready);
    }
    std::size_t max_datagram_bytes() const noexcept override { return inner_.max_datagram_bytes(); }

private:
    adl::platform::Network& inner_;
};

class FaultyHost final : public adl::platform::Host {
public:
    explicit FaultyHost(adl::fake::FakeHost& inner)
        : inner_(inner), network_(inner.network()), files_(inner.fake_files()) {}

    adl::platform::HostInfo info() const noexcept override { return inner_.info(); }
    adl::platform::Clock& clock() noexcept override { return inner_.clock(); }
    adl::platform::MemoryProvider& memory() noexcept override { return inner_.memory(); }
    adl::platform::FileSystem& files() noexcept override { return files_; }
    adl::platform::Threading& threads() noexcept override { return inner_.threads(); }
    adl::platform::Network& network() noexcept override { return network_; }
    adl::platform::Ipc& ipc() noexcept override { return inner_.ipc(); }

    [[nodiscard]] FaultyNetwork& faulty_network() noexcept { return network_; }
    [[nodiscard]] FaultyFileSystem& faulty_files() noexcept { return files_; }

private:
    adl::fake::FakeHost& inner_;
    FaultyNetwork network_;
    FaultyFileSystem files_;
};

}  // namespace adl::test
