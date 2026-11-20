// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/gate.hpp"

#include <string_view>
#include <utility>

namespace adl::loader_core
{

void OperationGate::post_selection(std::vector<std::string> names) noexcept
{
    std::lock_guard lock(selection_mutex_);
    selection_ = std::move(names);
    selection_ready_.store(true, std::memory_order_release);
}

void OperationGate::clear() noexcept
{
    abort_requested_.store(false, std::memory_order_release);
    selection_ready_.store(false, std::memory_order_release);
    std::lock_guard lock(selection_mutex_);
    selection_.clear();
    // The runner's thread is idle when clear() runs: the flags of the last operation are reset here,
    // never while one is running.
    abort_issued_ = false;
    selection_issued_ = false;
}

std::vector<std::string> OperationGate::take_selection() noexcept
{
    std::lock_guard lock(selection_mutex_);
    std::vector<std::string> choice = std::move(selection_);
    selection_.clear();
    return choice;
}

void OperationGate::on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept
{
    if (abort_requested() && !abort_issued_ && !loader.done())
    {
        abort_issued_ = true;
        loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
        return;
    }
    if (selection_posted() && !selection_issued_ && !loader.done())
    {
        selection_issued_ = true;
        std::vector<std::string> choice = take_selection();
        if (choice.empty())
        {
            // "None": what a listener answering the target's list with
            // SelectionDecision::cancelled would do. The target's next status file confirms
            // the abort, as for the operator's own Abort.
            loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
            return;
        }
        std::vector<std::string_view> views;
        views.reserve(choice.size());
        for (const std::string& name : choice)
        {
            views.push_back(name);
        }
        if (!loader.provide_selection(now, views) && !loader.done())
        {
            // The loader is not waiting for a choice (the target's list has not arrived): the
            // choice is not lost, it is offered again at the next poll.
            post_selection(std::move(choice));
            selection_issued_ = false;
        }
    }
}

} // namespace adl::loader_core
