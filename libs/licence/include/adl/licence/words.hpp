// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The plain words for a licence's state and for the reason behind it, as the command line prints them.
// The windows have their own, translated, sentences.

#include "adl/licence/licence.hpp"

#include <string_view>

namespace adl::licence
{

// "none", "valid", "expiring", "grace", "expired", "not yet valid", "clock suspect" or "invalid".
[[nodiscard]] std::string_view state_word(State value) noexcept;

// The name of a state for a program to read: none, valid, expiring, grace, expired, not_yet_valid,
// clock_suspect or invalid. One word each, the same in every product that shares this library.
[[nodiscard]] std::string_view state_name(State value) noexcept;

// The edition a status amounts to, for a program to read: "community", "pro" or "enterprise". A licence
// that is not in force is "community".
[[nodiscard]] std::string_view edition_name(const Status& current) noexcept;

// One lower-case clause that says why a licence is in its state, such as "the licence has ended"; "ok"
// for Reason::ok.
[[nodiscard]] std::string_view reason_sentence(Reason value) noexcept;

} // namespace adl::licence
