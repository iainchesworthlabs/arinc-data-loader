// SPDX-License-Identifier: GPL-3.0-or-later
// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// The clock is the virtual-clock + WaitForActivity pattern the vendor's own
// ARINC 615A module uses (arinc615a_targethardware.c in the 178B tree): a
// virtual alarm clock over the primary clock, one activity waiting on it,
// SetClockAlarm to arm it and WaitForActivity to block until it completes.
//
// Assumptions named: the design audit's FindClock("Primary") is used as the
// extern PrimaryClock that INTEGRITY.h declares, because FindClock is
// declared only in kernel/boot.h, a kernel-internal header that also defines
// globals, and the vendor's application module uses the externs.
// GetClockTime, CreateActivity, AsynchronousReceive, SetClockAlarm and
// WaitForActivity carry no /*c*/ client marker in INTEGRITY.h, but the
// vendor's module uses the last four from application code and the kernel
// dispatches GetClockTime the same way; all of that must be verified against
// the product's kernel configuration. The virtual clock is created with
// CLOCK_ALARM only, exactly as the vendor does, so it cannot itself be read
// (kernel/clock.c demands CLOCK_READTIME of the clock being read); the time
// is read from the primary instead, and the two agree because a virtual
// clock shares the primary's timer driver. Activities belong to the task
// CreateActivity names (CurrentTask() here), so a Clock's sleep_for must run
// on the task that built the Host; the port never closes the objects, which
// the partition owns.
//
// When the alarm machinery cannot be built, the clock degrades - never
// silently: now() keeps reading the primary clock, and sleep_for busy-waits
// on it with Yield().

#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "ghs/ghs_native.hpp"
#include "ghs178b/ghs178b_services.hpp"

#include <INTEGRITY.h>

#include <cstdint>
#include <memory>

namespace adl::platform::ghs178b
{
namespace
{

// The Identification of the sleep activity - what WaitForActivity returns
// when the alarm completes it. An arbitrary constant in the vendor module's
// style (its TIMEOUT_ACT is 13); the semaphores in threads_ghs178b.cpp hand
// their ids out from a counter that starts above this one.
constexpr Value sleep_activity_id = 32;

class Ghs178bClock final : public Clock
{
public:
    Ghs178bClock() noexcept
    {
        // boot.c falls back to "StandardTick" when the image has no "Primary"
        // clock, so the extern is NULLClock only when the image has no clock
        // at all; nothing below can work then, and now() stays at zero.
        primary_ = PrimaryClock;
        if (primary_ == NULLClock)
        {
            return;
        }
        // A virtual clock over the primary: it shares the primary's timer
        // driver (kernel/clock.c), so it reads the same time, and CLOCK_ALARM
        // gives it the one alarm sleep_for arms. Its alarm cannot disturb
        // anyone else's clock.
        if (CreateVirtualClock(primary_, CLOCK_ALARM, &virtual_) != Success)
        {
            return; // degraded: now() from the primary, sleep_for busy-waits
        }
        // The activity the alarm completes. The priority is 100, a proposal
        // between the loader's `high` (60) and `normal` (127), in the vendor
        // module's style of a constant per activity; `false` is the Interrupt
        // flag (a task activity, not an interrupt handler).
        const Error created = CreateActivity(CurrentTask(), 100, false, sleep_activity_id, &sleep_activity_);
        if (created != Success ||
            AsynchronousReceive(sleep_activity_, reinterpret_cast<Object>(virtual_), NULL) != Success)
        {
            virtual_ = NULLClock;
            sleep_activity_ = NULLActivity;
            return;
        }
    }

    Instant now() noexcept override
    {
        // A virtual clock shares the primary's timer driver, so either reads
        // the same time; read the primary when it exists.
        const Clock source = primary_ != NULLClock ? primary_ : virtual_;
        if (source == NULLClock)
        {
            return last_; // no clock in the image: time never moves
        }
        Time time{};
        if (GetClockTime(source, &time) != Success)
        {
            return last_; // the interface forbids going backwards
        }
        const Instant current{ghs::to_duration(time)};
        if (current.since_origin < last_.since_origin)
        {
            return last_;
        }
        last_ = current;
        return current;
    }

    void sleep_for(Duration d) noexcept override
    {
        if (d <= Duration::zero())
        {
            return;
        }
        const Instant deadline = now() + d;
        if (virtual_ == NULLClock || sleep_activity_ == NULLActivity)
        {
            busy_wait(deadline);
            return;
        }
        // The deadline is read from the primary: the virtual clock lacks
        // CLOCK_READTIME (see the file comment) and shares the same time.
        Time now_time{};
        if (GetClockTime(primary_, &now_time) != Success)
        {
            busy_wait(deadline);
            return;
        }
        // Make sure the receive is pending before the first WaitForActivity:
        // a previous sleep may have left it un-armed (its own re-arm failed
        // and it finished in busy_wait).
        if (!receive_armed(AsynchronousReceive(sleep_activity_, reinterpret_cast<Object>(virtual_), NULL)))
        {
            busy_wait(deadline);
            return;
        }
        // SetClockAlarm takes an absolute time (kernel/clock.c: "The alarm is
        // to go off at *AlarmTime"), so the deadline is now + d, not d alone.
        // The vendor's module appears to pass a relative time here; its
        // virtual clock starts at zero, so the two coincide only until the
        // clock is set, and this port adds the current time.
        const Time alarm = ghs::add_duration(now_time, d);
        if (SetClockAlarm(virtual_, false, NULL, &alarm) != Success)
        {
            busy_wait(deadline);
            return;
        }
        for (;;)
        {
            // Blocks until any activity of this task completes; the alarm
            // completing the sleep activity is the usual wake, and a
            // semaphore wait on the same task wakes here too. WaitForActivity
            // takes no error and returns void.
            Value id = 0;
            WaitForActivity(&id);
            (void)id; // whatever woke us, the steps are the same
            // Re-arm after every wake, exactly as the vendor module does.
            if (!receive_armed(AsynchronousReceive(sleep_activity_, reinterpret_cast<Object>(virtual_), NULL)))
            {
                busy_wait(deadline);
                return;
            }
            if (now() >= deadline)
            {
                return;
            }
            // The wake was not the deadline: an alarm can fire early (or
            // another activity completed), so arm the same deadline again and
            // keep waiting.
            if (SetClockAlarm(virtual_, false, NULL, &alarm) != Success)
            {
                busy_wait(deadline);
                return;
            }
        }
    }

    Result<std::int64_t> wall_time_seconds() noexcept override
    {
        // The extern the design audit's FindClock("RealTimeClock") names
        // (boot.h is a kernel header; see the file comment). NULLClock when
        // the image has no real-time clock.
        if (RealTimeClock == NULLClock)
        {
            return fail(ErrorCode::unsupported);
        }
        Time time{};
        if (GetClockTime(RealTimeClock, &time) != Success)
        {
            // TheTimeHasNeverBeenSet, or the clock cannot be read: there is
            // no trustworthy wall time.
            return fail(ErrorCode::unsupported);
        }
        // For labelling a log line or a directory name; never measure an
        // interval with it, it can jump when the clock is set.
        return static_cast<std::int64_t>(time.Seconds);
    }

    // Nothing to undo: clocks and activities are kernel objects owned by the
    // partition, closed when it is; the port never calls Close on them.
    ~Ghs178bClock() override = default;

private:
    // AsynchronousReceive returns Success when it armed the receive, and
    // WaitingForSend (a receive also answers WaitingForReceive in the kernel)
    // when the activity was already waiting for the object - both are the
    // state sleep_for wants; anything else means the receive is not pending.
    [[nodiscard]] static bool receive_armed(Error error) noexcept
    {
        return error == Success || error == WaitingForSend || error == WaitingForReceive;
    }

    void busy_wait(Instant deadline) noexcept
    {
        // The documented degradation when the partition refuses the alarm
        // machinery: spin on the primary clock. It burns the CPU, which is
        // why it is only the fallback, and it relies on now() advancing; an
        // image with no readable clock at all has no way to sleep and this
        // cannot return.
        while (now() < deadline)
        {
            Yield();
        }
    }

    Clock primary_{NULLClock};
    Clock virtual_{NULLClock};
    Activity sleep_activity_{NULLActivity};
    Instant last_{};
};

} // namespace

std::unique_ptr<Clock> make_clock() noexcept
{
    return detail::make_unique_nothrow<Ghs178bClock>();
}

} // namespace adl::platform::ghs178b
