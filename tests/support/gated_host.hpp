#pragma once

// A Host that forwards everything to another one and can hold the thread that is using it at a
// chosen point of an operation. The fake platform runs in virtual time, so a whole transfer takes
// a few milliseconds of real time and a test on another thread cannot be sure it is still under
// way when it presses Abort; here the engine's thread is stopped inside a file call, with the
// transfer certainly half done, and let go once the test has done what it wanted to do.
//
//     GatedHost gated(rig.loader_host);
//     gated.arm(GatedHost::Event::open_read, 2);   // hold when the target asks for its 2nd file
//     ... start the upload ...
//     gated.wait_until_reached();                  // the first file is across, the second is not
//     model.abort();
//     gated.release();

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string_view>

#include "adl/platform/host.hpp"

namespace adl::test {

class GatedHost final : public adl::platform::Host {
public:
    // The file calls the gate can hold the caller in.
    enum class Event {
        open_read,  // the loader opens a file to serve (an upload: the target asked for a file)
        create,     // the loader creates a file to write (a download: a file begins to arrive)
    };

    explicit GatedHost(adl::platform::Host& inner) : inner_(inner), files_(*this, inner.files()) {}
    GatedHost(const GatedHost&) = delete;
    GatedHost& operator=(const GatedHost&) = delete;
    ~GatedHost() override { release(); }

    // From now on the `nth` (counting from 1) call of `event` blocks until release().
    void arm(Event event, unsigned nth) {
        const std::scoped_lock lock(mutex_);
        armed_ = true;
        released_ = false;
        reached_ = false;
        event_ = event;
        nth_ = nth;
        seen_ = 0;
    }

    // True once a call is being held.
    [[nodiscard]] bool reached() const {
        const std::scoped_lock lock(mutex_);
        return reached_;
    }

    // Waits (on the calling thread, in real time) until a call is held or `timeout` passes.
    [[nodiscard]] bool wait_until_reached(
        std::chrono::milliseconds timeout = std::chrono::seconds(20)) {
        std::unique_lock lock(mutex_);
        return arrived_.wait_for(lock, timeout, [this] { return reached_; });
    }

    // Lets the held call go on, and every call after it: the gate is spent until armed again.
    void release() {
        {
            const std::scoped_lock lock(mutex_);
            released_ = true;
            armed_ = false;
        }
        open_.notify_all();
    }

    [[nodiscard]] adl::platform::HostInfo info() const noexcept override { return inner_.info(); }
    [[nodiscard]] adl::platform::Clock& clock() noexcept override { return inner_.clock(); }
    [[nodiscard]] adl::platform::MemoryProvider& memory() noexcept override {
        return inner_.memory();
    }
    [[nodiscard]] adl::platform::FileSystem& files() noexcept override { return files_; }
    [[nodiscard]] adl::platform::Threading& threads() noexcept override { return inner_.threads(); }
    [[nodiscard]] adl::platform::Network& network() noexcept override { return inner_.network(); }
    [[nodiscard]] adl::platform::Ipc& ipc() noexcept override { return inner_.ipc(); }

private:
    void pass(Event event) {
        std::unique_lock lock(mutex_);
        if (!armed_ || event != event_ || ++seen_ != nth_) {
            return;
        }
        reached_ = true;
        arrived_.notify_all();
        open_.wait(lock, [this] { return released_; });
    }

    class Files final : public adl::platform::FileSystem {
    public:
        Files(GatedHost& gate, adl::platform::FileSystem& inner) : gate_(gate), inner_(inner) {}

        adl::platform::Result<adl::platform::Owned<adl::platform::ReadFile>> open_read(
            std::string_view path) noexcept override {
            gate_.pass(Event::open_read);
            return inner_.open_read(path);
        }
        adl::platform::Result<adl::platform::Owned<adl::platform::WriteFile>> create(
            std::string_view path, adl::platform::CreateMode mode) noexcept override {
            gate_.pass(Event::create);
            return inner_.create(path, mode);
        }
        adl::platform::Result<adl::platform::FileInfo> stat(
            std::string_view path) noexcept override {
            return inner_.stat(path);
        }
        adl::platform::Result<void> make_directory(std::string_view path) noexcept override {
            return inner_.make_directory(path);
        }
        adl::platform::Result<void> remove(std::string_view path) noexcept override {
            return inner_.remove(path);
        }
        adl::platform::Result<void> rename(std::string_view from,
                                           std::string_view to) noexcept override {
            return inner_.rename(from, to);
        }
        adl::platform::Result<void> list(std::string_view directory,
                                         adl::platform::DirectoryVisitor visitor,
                                         void* context) noexcept override {
            return inner_.list(directory, visitor, context);
        }

    private:
        GatedHost& gate_;
        adl::platform::FileSystem& inner_;
    };

    adl::platform::Host& inner_;
    Files files_;
    mutable std::mutex mutex_;
    std::condition_variable arrived_;
    std::condition_variable open_;
    bool armed_ = false;
    bool released_ = false;
    bool reached_ = false;
    Event event_ = Event::open_read;
    unsigned nth_ = 0;
    unsigned seen_ = 0;
};

}  // namespace adl::test
