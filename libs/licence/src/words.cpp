// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "adl/licence/words.hpp"

namespace adl::licence
{

std::string_view state_word(State value) noexcept
{
    switch (value)
    {
    case State::none:
        return "none";
    case State::valid:
        return "valid";
    case State::expiring:
        return "expiring";
    case State::grace:
        return "grace";
    case State::expired:
        return "expired";
    case State::not_yet_valid:
        return "not yet valid";
    case State::clock_suspect:
        return "clock suspect";
    case State::invalid:
        return "invalid";
    }
    return "unknown";
}

std::string_view state_name(State value) noexcept
{
    switch (value)
    {
    case State::none:
        return "none";
    case State::valid:
        return "valid";
    case State::expiring:
        return "expiring";
    case State::grace:
        return "grace";
    case State::expired:
        return "expired";
    case State::not_yet_valid:
        return "not_yet_valid";
    case State::clock_suspect:
        return "clock_suspect";
    case State::invalid:
        return "invalid";
    }
    return "invalid";
}

std::string_view edition_name(const Status& current) noexcept
{
    if (!current.active() || !current.value)
    {
        return "community";
    }
    return current.value->edition == Edition::enterprise ? "enterprise" : "pro";
}

std::string_view reason_sentence(Reason value) noexcept
{
    switch (value)
    {
    case Reason::ok:
        return "ok";
    case Reason::no_licence:
        return "there is no licence";
    case Reason::expired:
        return "the licence has ended";
    case Reason::not_yet_valid:
        return "the licence has not started yet";
    case Reason::clock_suspect:
        return "the date is before a time that has certainly passed";
    case Reason::malformed:
        return "the file is damaged or is not a licence";
    case Reason::newer_version:
        return "the licence needs a newer version";
    case Reason::bad_signature:
        return "the signature does not verify";
    case Reason::revoked:
        return "the licence or its key was withdrawn";
    case Reason::wrong_computer:
        return "the licence does not name this computer";
    case Reason::wrong_product:
        return "the licence is for another product";
    case Reason::not_in_licence:
        return "the licence does not include the feature";
    }
    return "unknown";
}

} // namespace adl::licence
