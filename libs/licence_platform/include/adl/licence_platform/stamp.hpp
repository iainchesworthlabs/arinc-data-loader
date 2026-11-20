// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a program that writes a record of what it did says of its licence: the edition it ran as, the state of the
// licence behind it and the licence's number. It is the licence in force now, judged as the windows judge it (the
// store of adl/licence/store.hpp, the same folders, `ADL_SETTINGS_DIR` moving them), and it is read-only: it
// reads the licence files and the clock file and writes nothing, not even the latest time seen.
//
// A stamp is a fact written down. Nothing is switched off by a licence, so nothing in a program asks for one
// before it works, and a licence that cannot be read is the Community edition.

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/store.hpp"
#include "adl/platform/host.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence_platform
{

struct Stamp
{
    // "community", "pro" or "enterprise": the edition in force. A licence that is not in force is "community".
    std::string edition;
    // "none", "valid", "expiring", "grace", "expired", "not_yet_valid", "clock_suspect" or "invalid".
    std::string state;
    // The licence number of the licence in force; nothing when none is. The licensee is never part of it.
    std::optional<std::string> number;
};

// The stamp of the licence files in `folders` at the time `now`, for the computer named `computer_name`, judged
// against `keys`. `build_time` is a time the program knows has passed (TimePoint::min() when it knows none).
[[nodiscard]] Stamp stamp_for(licence::store::Files& files, const licence::store::Folders& folders,
                              licence::TimePoint now, std::string_view computer_name,
                              const std::vector<licence::signed_file::TrustedKey>& keys,
                              licence::TimePoint build_time = licence::TimePoint::min());

// The stamp of this computer: the host's file system and clock, the licence folders of the platform (moved by
// `settings_directory`, the value of ADL_SETTINGS_DIR as UTF-8 text, or empty when it is not set) and the name of
// the computer. Nothing when the host has no wall clock, because then no licence can be judged.
[[nodiscard]] std::optional<Stamp> stamp_of(platform::Host& host, std::string_view settings_directory,
                                            const std::vector<licence::signed_file::TrustedKey>& keys);

} // namespace adl::licence_platform
