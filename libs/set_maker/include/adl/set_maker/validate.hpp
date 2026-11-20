// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The rules a plan has to follow before a media set is made from it: the standard's naming and part
// number rules (the check_*() functions of adl::arinc665, which return findings for what the standard
// recommends against) and the folder tree's stricter rule for a name, and the rules that only a whole plan
// has (names that differ in case only, a load with no data file, a batch that names a load the set does not
// hold). They are the rules the generator's window holds its form to, so the window and the command
// refuse the same sets (media_gen.parity in the tests says so).
//
// A problem is a place in the plan, written the way the description file's keys are
// ("loads[1].files[0].name"), and a text with no full stop at its end.

#include "adl/set_maker/plan.hpp"

#include <string>
#include <vector>

namespace adl::set_maker
{

struct Problem
{
    std::string where;
    std::string text;

    friend bool operator==(const Problem&, const Problem&) = default;
};

// Every problem of the plan, in the order of the plan. Empty when the plan can be made into a set. It does
// not look at the files on disk.
[[nodiscard]] std::vector<Problem> validate(const SetPlan& plan);

// What validate() says about one name (a file's, a header file's or a batch file's): the findings of the
// standard's rule as "clause: message" joined with "; ", or that the name will not work on every system.
// Empty when the name is good. `kind` picks the rule: a data file, a support file, a header file or a batch
// file.
enum class NameKind
{
    data_file,
    support_file,
    header_file,
    batch_file,
};

[[nodiscard]] std::string check_name(NameKind kind, std::string_view name);

} // namespace adl::set_maker
