// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The operator's controls of a running operation, handed to the runner as its controller. The runner
// blocks and polls between its waits, so what goes to a running operation cannot be a call: it is a flag
// or a value that one thread writes and the runner's thread reads. Two things go that way: the operator's
// abort, and the file choice of an operator defined download, which the target's list arrives before.
//
// Both are written from another thread (request_abort, post_selection) and turned into a call to the
// loader by the runner's thread in on_poll: the request into one DataLoader::abort, the choice into one
// DataLoader::provide_selection. An empty choice - "none" - is what a listener answering the target's
// list with SelectionDecision::cancelled would do: one abort, with aborted_by_operator. The abort wins if
// both have been made: once it has been asked for, a choice no longer matters. Each is issued once per
// operation, and clear() is called before an operation starts, so a request or a choice left over from an
// earlier one cannot stop or answer the next.

#include "adl/arinc615a/runner.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace adl::loader_core
{

class OperationGate : public adl::arinc615a::RunController
{
public:
    // Thread safe. Asks the operation that is running (or the next one to start) to abort.
    void request_abort() noexcept
    {
        abort_requested_.store(true, std::memory_order_release);
    }
    // Thread safe. Hands the operator's file choice to the operation that is running. The names are
    // taken at once; an empty list is the choice "none".
    void post_selection(std::vector<std::string> names) noexcept;
    // Forgets a request and a choice. Called before an operation is started, never while one is running.
    void clear() noexcept;
    [[nodiscard]] bool abort_requested() const noexcept
    {
        return abort_requested_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool selection_posted() const noexcept
    {
        return selection_ready_.load(std::memory_order_acquire);
    }

    void on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept override;

private:
    // The choice that was posted, once the runner's thread is reading it.
    [[nodiscard]] std::vector<std::string> take_selection() noexcept;

    std::atomic<bool> abort_requested_{false};
    std::atomic<bool> selection_ready_{false};
    std::mutex selection_mutex_;
    std::vector<std::string> selection_;
    bool abort_issued_ = false;     // the runner's thread only
    bool selection_issued_ = false; // the runner's thread only
};

} // namespace adl::loader_core
