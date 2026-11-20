// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_scenario/scenario.hpp"

namespace adl::loader_scenario
{

std::string_view to_string(Action action) noexcept
{
    switch (action)
    {
    case Action::drop:
        return "drop";
    case Action::duplicate:
        return "duplicate";
    case Action::delay:
        return "delay";
    case Action::reorder:
        return "reorder";
    case Action::corrupt:
        return "corrupt";
    case Action::check_value:
        return "check_value";
    case Action::length:
        return "length";
    case Action::error:
        return "error";
    case Action::wait:
        return "wait";
    case Action::abort_message:
        return "abort_message";
    case Action::abort:
        return "abort";
    case Action::silence:
        return "silence";
    }
    return "drop";
}

std::optional<Action> action_from_string(std::string_view text) noexcept
{
    for (std::size_t i = 0; i < action_count; ++i)
    {
        const auto action = static_cast<Action>(i);
        if (text == to_string(action))
        {
            return action;
        }
    }
    return std::nullopt;
}

std::string_view to_string(DatagramKind kind) noexcept
{
    switch (kind)
    {
    case DatagramKind::any:
        return "any";
    case DatagramKind::request:
        return "request";
    case DatagramKind::option_ack:
        return "option_ack";
    case DatagramKind::ack:
        return "ack";
    case DatagramKind::data:
        return "data";
    case DatagramKind::error:
        return "error";
    case DatagramKind::reply:
        return "reply";
    }
    return "any";
}

std::optional<DatagramKind> datagram_kind_from_string(std::string_view text) noexcept
{
    for (const DatagramKind kind : {DatagramKind::any, DatagramKind::request, DatagramKind::option_ack,
                                    DatagramKind::ack, DatagramKind::data, DatagramKind::error, DatagramKind::reply})
    {
        if (text == to_string(kind))
        {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace adl::loader_scenario
