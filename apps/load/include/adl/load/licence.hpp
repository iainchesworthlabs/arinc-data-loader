// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Where an operation's record gets the licence it ran under (docs/file-reference.md, "The licence in a record").
//
// The program that runs a command says how the licence is read: main gives a LicenceSource that reads this
// computer's licence files, a test gives one that returns what it wants, and a source that is not given reads
// nothing, so the record says `null`. The library does not include the licence code itself, because the programs
// that are built for a target with no file system have none (libs/licence_platform is a desktop library).
//
// A source is asked once for each operation, when it starts. It must not write anything, and an operation never
// waits for, depends on or changes its answer: a licence that cannot be read is the Community edition, and one
// that cannot be judged is nothing.

#include "adl/loader_core/record.hpp"
#include "adl/platform/host.hpp"

#include <functional>
#include <optional>
#include <utility>

namespace adl::load
{

class LicenceSource
{
public:
    // What the licence in force is, for the record; nothing when it cannot be told.
    using Reader = std::function<std::optional<adl::loader_core::RecordedLicence>(adl::platform::Host&)>;

    // A source that reads no licence.
    LicenceSource() = default;
    explicit LicenceSource(Reader reader) : reader_(std::move(reader)) {}

    // The licence in force now, or nothing when this source reads none or cannot say.
    [[nodiscard]] std::optional<adl::loader_core::RecordedLicence> stamp(adl::platform::Host& host) const
    {
        return reader_ ? reader_(host) : std::nullopt;
    }

private:
    Reader reader_;
};

} // namespace adl::load
