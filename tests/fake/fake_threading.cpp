// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_threading.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace adl::fake
{

using platform::ErrorCode;

class FakeThread final : public platform::Thread
{
public:
    FakeThread(FakeThreading& owner, const platform::ThreadSpec& spec)
        : owner_(owner), entry_(spec.entry), argument_(spec.argument)
    {
    }

    ~FakeThread() override
    {
        (void)join();
    }

    platform::Result<void> join() noexcept override
    {
        run();
        return {};
    }

    platform::PriorityOutcome priority_outcome() const noexcept override
    {
        return platform::PriorityOutcome::unsupported;
    }

    void run() noexcept
    {
        if (ran_)
        {
            return;
        }
        ran_ = true;
        auto& pending = owner_.pending_;
        pending.erase(std::remove(pending.begin(), pending.end(), this), pending.end());
        entry_(argument_);
    }

private:
    FakeThreading& owner_;
    platform::ThreadEntry entry_;
    void* argument_;
    bool ran_ = false;
};

namespace
{

class FakeMutex final : public platform::Mutex
{
public:
    void lock() noexcept override
    {
        if (locked_)
        {
            // One thread locking a mutex it holds waits for itself for ever. A real
            // mutex hangs; the fake says so.
            std::fputs("fake platform: lock() on a mutex that is already locked\n", stderr);
            std::abort();
        }
        locked_ = true;
    }
    bool try_lock() noexcept override
    {
        if (locked_)
        {
            return false;
        }
        locked_ = true;
        return true;
    }
    void unlock() noexcept override
    {
        locked_ = false;
    }

private:
    bool locked_ = false;
};

class FakeSemaphore final : public platform::Semaphore
{
public:
    FakeSemaphore(FakeWorld& world, unsigned initial, unsigned ceiling) noexcept
        : world_(world), count_(initial), ceiling_(ceiling)
    {
    }

    platform::Result<void> post() noexcept override
    {
        if (count_ >= ceiling_)
        {
            return platform::fail(ErrorCode::resource_exhausted);
        }
        ++count_;
        return {};
    }

    platform::Result<void> wait(platform::Duration timeout) noexcept override
    {
        if (!world_.wait_until([this] { return count_ > 0; }, timeout))
        {
            return platform::fail(ErrorCode::timeout);
        }
        --count_;
        return {};
    }

private:
    FakeWorld& world_;
    unsigned count_;
    unsigned ceiling_;
};

} // namespace

platform::Result<platform::Owned<platform::Thread>> FakeThreading::spawn(const platform::ThreadSpec& spec) noexcept
{
    if (spec.entry == nullptr)
    {
        return platform::fail(ErrorCode::invalid_argument);
    }
    auto thread = std::make_unique<FakeThread>(*this, spec);
    pending_.push_back(thread.get());
    ++spawned_;
    return platform::adopt<platform::Thread>(std::move(thread));
}

platform::Result<platform::Owned<platform::Mutex>> FakeThreading::create_mutex() noexcept
{
    return platform::adopt<platform::Mutex>(std::make_unique<FakeMutex>());
}

platform::Result<platform::Owned<platform::Semaphore>> FakeThreading::create_semaphore(unsigned initial,
                                                                                       unsigned ceiling) noexcept
{
    if (ceiling == 0 || initial > ceiling)
    {
        return platform::fail(ErrorCode::invalid_argument);
    }
    return platform::adopt<platform::Semaphore>(std::make_unique<FakeSemaphore>(world_, initial, ceiling));
}

std::size_t FakeThreading::run_pending()
{
    std::size_t ran = 0;
    while (!pending_.empty())
    {
        FakeThread* next = pending_.front();
        next->run();
        ++ran;
    }
    return ran;
}

} // namespace adl::fake
