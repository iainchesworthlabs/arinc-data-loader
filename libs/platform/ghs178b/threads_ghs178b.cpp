// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// Threading over the native semaphores. A mutex is a CreateHighestLocker
// Semaphore (a priority-ceiling lock); a Semaphore is CreateSemaphore plus
// the vendor module's timed-wait pattern (arinc615a_targethardware.c): the
// wait arms an AsynchronousReceive on the semaphore, arms an alarm on a
// virtual clock, and dispatches on the id WaitForActivity returns - the
// semaphore's activity for a token, the clock's for the deadline. Each
// Semaphore owns a virtual clock because SetClockAlarm replaces the one
// alarm a clock has and several timed waits can be outstanding; the cost is
// kernel object-table entries.
//
// Assumptions named, each to verify against the vendor module's evidence and
// the product's kernel configuration: a completed semaphore activity means
// the semaphore was obtained (the module releases into exactly such armed
// activities and dispatches on the id, and kernel/transfer.c completes a
// receive with Status Success); a pending receive reports WaitingForReceive,
// not Success, so GetActivityStatus tells the two apart. TryToObtain
// Semaphore, GetClockTime, CreateActivity, AsynchronousReceive, SetClockAlarm
// and WaitForActivity carry no /*c*/ client marker in INTEGRITY.h, but the
// module uses the last four and the kernel dispatches the others. Activities
// belong to the task CreateActivity names (CurrentTask() here), so a wait
// must run on the task that built the Threading; activity ids are handed out
// per Threading and are only unique among its own objects. The ceiling is
// the object's own bookkeeping, because INTEGRITY may cap a semaphore at its
// creation value (the InitialValueExceedsMaximum error suggests a maximum
// exists). A highest-locker semaphore refuses an AsynchronousReceive
// (kernel/activity.c: CannotBlockOnHighestLockerSemaphore), which is why the
// mutex waits synchronously and only counting semaphores use the activity
// pattern. spawn() is unsupported on purpose: application tasks are
// configured into the kernel image, and the tree has no client task-creation
// call (docs/platform-ghs.md).

#include "adl/platform/threads.hpp"
#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "ghs/ghs_native.hpp"
#include "ghs178b/ghs178b_services.hpp"

#include <INTEGRITY.h>

#include <memory>

namespace adl::platform::ghs178b
{
namespace
{

// The priority of the clock and semaphore activities, a proposal in the
// vendor module's style of a constant per activity: 100 sits between the
// loader's `high` (60) and `normal` (127), so a wake-up is prompt without
// pre-empting the most urgent work.
constexpr Value activity_priority = 100;

// The first activity id handed out. The clock's sleep activity
// (clock_ghs178b.cpp) uses 32; this counter starts above it so a task that
// hosts both keeps distinct ids. Other modules' activities cannot be seen
// from here, which is what the status checks in wait() are for.
constexpr Value first_activity_id = 64;

class Ghs178bMutex final : public Mutex
{
public:
    explicit Ghs178bMutex(Semaphore semaphore) noexcept : semaphore_(semaphore) {}

    void lock() noexcept override
    {
        // Nothing is logged when the wait fails: there is no logging channel
        // here, and a failed wait means the semaphore was closed under us,
        // which the caller cannot act on either.
        (void)WaitForSemaphore(semaphore_);
    }

    bool try_lock() noexcept override
    {
        // Success means this task now holds the semaphore; any other value
        // means it does not, and the distinction is not one the interface
        // exposes.
        return TryToObtainSemaphore(semaphore_) == Success;
    }

    void unlock() noexcept override
    {
        (void)ReleaseSemaphore(semaphore_); // as in lock(): nothing to log
    }

    // Nothing to undo: the semaphore is a kernel object owned by the
    // partition.
    ~Ghs178bMutex() override = default;

private:
    Semaphore semaphore_{NULLSemaphore};
};

class Ghs178bSemaphore final : public Semaphore
{
public:
    Ghs178bSemaphore(Semaphore semaphore, Clock alarm_clock, Activity clock_activity, Activity sem_activity,
                     Value clock_activity_id, Value sem_activity_id, unsigned initial, unsigned ceiling,
                     Clock& clock) noexcept
        : semaphore_(semaphore), alarm_clock_(alarm_clock), clock_activity_(clock_activity),
          sem_activity_(sem_activity), clock_activity_id_(clock_activity_id), sem_activity_id_(sem_activity_id),
          count_(initial), ceiling_(ceiling), clock_(clock)
    {
    }

    Result<void> post() noexcept override
    {
        if (count_ >= ceiling_)
        {
            // The interface says so: a signal nobody has consumed is not lost
            // silently. Do not ReleaseSemaphore here - with the receive armed
            // the kernel would consume the count into the activity and the
            // token would sit where nobody asked for it.
            return fail(ErrorCode::resource_exhausted);
        }
        const Error released = ReleaseSemaphore(semaphore_);
        if (released != Success)
        {
            return ghs::fail_native(released);
        }
        ++count_;
        return {};
    }

    Result<void> wait(Duration timeout) noexcept override
    {
        // A post() that arrived while nobody waited completed the armed
        // receive; take that token first. (The vendor module re-arms an
        // activity right after it completed - the evidence this hand-off is
        // the way the kernel delivers.)
        if (GetActivityStatus(sem_activity_) == Success)
        {
            (void)AsynchronousReceive(sem_activity_, reinterpret_cast<Object>(semaphore_), NULL);
            take();
            return {};
        }
        // A leftover completed clock activity would swallow the next alarm;
        // re-arm it (an error here means it is already armed and waiting).
        (void)AsynchronousReceive(clock_activity_, reinterpret_cast<Object>(alarm_clock_), NULL);
        if (timeout <= Duration::zero())
        {
            if (TryToObtainSemaphore(semaphore_) == Success)
            {
                take();
                return {};
            }
            return fail(ErrorCode::timeout);
        }
        const bool finite = timeout != forever;
        Instant deadline{};
        if (finite)
        {
            // The time is read from the primary: the virtual alarm clock is
            // created with CLOCK_ALARM only and cannot be read itself; the
            // two share the timer driver, so the bases agree.
            Time now_time{};
            const Error read = GetClockTime(PrimaryClock, &now_time);
            if (read != Success)
            {
                // Without a time there is no deadline to honour; the caller
                // hears the native reason rather than a made-up one.
                return ghs::fail_native(read);
            }
            deadline = Instant{ghs::to_duration(now_time)} + timeout;
            // Absolute time, as kernel/clock.c documents; see clock_ghs178b.cpp.
            const Time deadline_time = ghs::add_duration(now_time, timeout);
            const Error armed = SetClockAlarm(alarm_clock_, false, NULL, &deadline_time);
            if (armed != Success)
            {
                return poll_until(deadline);
            }
        }
        else
        {
            (void)SetClockAlarm(alarm_clock_, false, NULL, NULL); // disarm, defensively
        }
        if (!receive_armed(AsynchronousReceive(sem_activity_, reinterpret_cast<Object>(semaphore_), NULL)))
        {
            // Without the armed receive a release lands in the kernel count,
            // where polling can still see it.
            return poll_until(finite ? deadline : Instant{Duration::max()});
        }
        for (;;)
        {
            Value id = 0;
            WaitForActivity(&id);
            if (id == sem_activity_id_)
            {
                if (GetActivityStatus(sem_activity_) == Success)
                {
                    (void)AsynchronousReceive(sem_activity_, reinterpret_cast<Object>(semaphore_), NULL);
                    if (finite)
                    {
                        (void)SetClockAlarm(alarm_clock_, false, NULL, NULL);
                    }
                    take();
                    return {};
                }
                // The same id on another activity of this task (ids are only
                // unique within one Threading): not our token; stay waiting.
                continue;
            }
            if (id == clock_activity_id_)
            {
                (void)AsynchronousReceive(clock_activity_, reinterpret_cast<Object>(alarm_clock_), NULL);
                if (finite && clock_.now() >= deadline)
                {
                    return fail(ErrorCode::timeout);
                }
                continue;
            }
            // Some other activity of the task completed - another semaphore's
            // clock, say - which is not a signal for this wait.
            if (GetActivityStatus(sem_activity_) == Success)
            {
                // A token also arrived in this wake: take it here rather than
                // lose it to a re-arm.
                (void)AsynchronousReceive(sem_activity_, reinterpret_cast<Object>(semaphore_), NULL);
                if (finite)
                {
                    (void)SetClockAlarm(alarm_clock_, false, NULL, NULL);
                }
                take();
                return {};
            }
            return fail(ErrorCode::interrupted);
        }
    }

    // Nothing to undo: semaphore, clock and activities are kernel objects
    // owned by the partition.
    ~Ghs178bSemaphore() override = default;

private:
    // AsynchronousReceive returns Success when it armed the receive, and
    // WaitingForSend (a receive also answers WaitingForReceive in the kernel)
    // when the activity was already waiting for the semaphore - both are the
    // state a wait needs; anything else means the receive is not pending.
    [[nodiscard]] static bool receive_armed(Error error) noexcept
    {
        return error == Success || error == WaitingForSend || error == WaitingForReceive;
    }

    void take() noexcept
    {
        // The count always includes this token (post() counted it into the
        // armed activity); the guard is defence in depth.
        if (count_ > 0)
        {
            --count_;
        }
    }

    Result<void> poll_until(Instant deadline) noexcept
    {
        // The degradation when the alarm or the receive cannot be armed: poll
        // the semaphore and yield, watching the same deadline. Only reached
        // with no armed receive, so a release lands in the kernel count where
        // TryToObtainSemaphore can see it. With an endless deadline this is a
        // busy loop - the price of a forever wait without the alarm machinery.
        for (;;)
        {
            if (TryToObtainSemaphore(semaphore_) == Success)
            {
                take();
                return {};
            }
            if (clock_.now() >= deadline)
            {
                return fail(ErrorCode::timeout);
            }
            Yield();
        }
    }

    Semaphore semaphore_{NULLSemaphore};
    Clock alarm_clock_{NULLClock};
    Activity clock_activity_{NULLActivity};
    Activity sem_activity_{NULLActivity};
    Value clock_activity_id_ = 0;
    Value sem_activity_id_ = 0;
    unsigned count_ = 0;
    unsigned ceiling_ = 0;
    Clock& clock_;
};

class Ghs178bThreading final : public Threading
{
public:
    explicit Ghs178bThreading(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<Thread>> spawn(const ThreadSpec&) noexcept override
    {
        // Application tasks are configured into the kernel image, and the
        // tree has no client task-creation call (docs/platform-ghs.md); the
        // runner and the application are static tasks the image starts. The
        // interface's spawn cannot express that, so it is unsupported.
        return fail(ErrorCode::unsupported);
    }

    Result<Owned<Mutex>> create_mutex() noexcept override
    {
        // A priority-ceiling lock (INTEGRITY's "highest locker"). The first
        // argument is the ceiling priority a locker is raised to; 1, the most
        // urgent, is passed so that every task of the partition may lock it
        // (the errors TaskPriorityTooHighToObtainHighestLockerSemaphore and
        // friends refuse a task whose own priority is above the ceiling). No
        // caller of this function exists anywhere in the tree, so the
        // argument is a guess to settle against the product's priority plan.
        Semaphore semaphore = NULLSemaphore;
        const Error created = CreateHighestLockerSemaphore(1, &semaphore);
        if (created != Success)
        {
            return ghs::fail_native(created);
        }
        auto mutex = make_owned_nothrow<Mutex, Ghs178bMutex>(semaphore);
        if (!mutex)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return mutex;
    }

    Result<Owned<Semaphore>> create_semaphore(unsigned initial, unsigned ceiling) noexcept override
    {
        if (initial > ceiling || ceiling == 0)
        {
            return fail(ErrorCode::invalid_argument);
        }
        Semaphore semaphore = NULLSemaphore;
        const Error created_semaphore = CreateSemaphore(initial, &semaphore);
        if (created_semaphore != Success)
        {
            return ghs::fail_native(created_semaphore);
        }
        // The timed wait's machinery, all per object (see the file comment):
        // a virtual alarm clock over the primary, and one activity each for
        // the clock and the semaphore. Unlike the Clock, a Semaphore whose
        // timed wait could not time out is not a usable degradation, so any
        // failure here fails the creation with the native reason.
        Clock alarm_clock = NULLClock;
        const Error created_clock = CreateVirtualClock(PrimaryClock, CLOCK_ALARM, &alarm_clock);
        if (created_clock != Success)
        {
            return ghs::fail_native(created_clock);
        }
        const Value clock_id = next_id_++;
        const Value sem_id = next_id_++;
        Activity clock_activity = NULLActivity;
        const Error created_clock_activity =
            CreateActivity(CurrentTask(), activity_priority, false, clock_id, &clock_activity);
        if (created_clock_activity != Success)
        {
            return ghs::fail_native(created_clock_activity);
        }
        const Error armed_clock = AsynchronousReceive(clock_activity, reinterpret_cast<Object>(alarm_clock), NULL);
        if (armed_clock != Success)
        {
            return ghs::fail_native(armed_clock);
        }
        Activity sem_activity = NULLActivity;
        const Error created_sem_activity =
            CreateActivity(CurrentTask(), activity_priority, false, sem_id, &sem_activity);
        if (created_sem_activity != Success)
        {
            return ghs::fail_native(created_sem_activity);
        }
        const Error armed_sem = AsynchronousReceive(sem_activity, reinterpret_cast<Object>(semaphore), NULL);
        if (armed_sem != Success)
        {
            return ghs::fail_native(armed_sem);
        }
        auto sem = make_owned_nothrow<Semaphore, Ghs178bSemaphore>(semaphore, alarm_clock, clock_activity, sem_activity,
                                                                   clock_id, sem_id, initial, ceiling, clock_);
        if (!sem)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return sem;
    }

private:
    // The activity ids handed out at creation; create_* runs once, at
    // start-up, on one thread, so the counter needs no locking.
    Clock& clock_;
    Value next_id_ = first_activity_id;
};

} // namespace

std::unique_ptr<Threading> make_threading(Clock& clock) noexcept
{
    return detail::make_unique_nothrow<Ghs178bThreading>(clock);
}

} // namespace adl::platform::ghs178b
