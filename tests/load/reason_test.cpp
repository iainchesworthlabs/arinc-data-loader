// SPDX-License-Identifier: GPL-3.0-or-later
// The table of reasons and exit codes of docs/load-command.md, row by row: the word that goes into `reason`,
// the exit code that goes with it, and that the reasons of loader_core's run_operation are the same words.
// `aborted` is only here: nothing the simulated target does makes the program end an operation that way (a
// record written by a window can carry it), so the table is where it is held.

#include "adl/load/reason.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

using adl::load::Reason;

namespace
{

struct Row
{
    Reason reason;
    std::string_view word;
    int exit_code;
};

const std::vector<Row> table{
    {Reason::completed, "completed", 0},
    {Reason::target_refused, "target_refused", 1},
    {Reason::target_aborted, "target_aborted", 1},
    {Reason::load_failed, "load_failed", 1},
    {Reason::missing_files, "missing_files", 1},
    {Reason::protocol_error, "protocol_error", 1},
    {Reason::aborted, "aborted", 1},
    {Reason::usage, "usage", 2},
    {Reason::media_set_unreadable, "media_set_unreadable", 3},
    {Reason::media_set_changed, "media_set_changed", 3},
    {Reason::no_answer, "no_answer", 4},
    {Reason::local_error, "local_error", 5},
};

} // namespace

TEST_CASE("every reason is the contract's word with the contract's exit code", "[load][reason]")
{
    for (const Row& row : table)
    {
        CAPTURE(row.word);
        CHECK(to_string(row.reason) == row.word);
        CHECK(adl::load::exit_code_of(row.reason) == row.exit_code);
    }
    CHECK(adl::load::exit_completed == 0);
    CHECK(adl::load::exit_failed == 1);
    CHECK(adl::load::exit_usage == 2);
    CHECK(adl::load::exit_media_set == 3);
    CHECK(adl::load::exit_no_answer == 4);
    CHECK(adl::load::exit_local_error == 5);
}

TEST_CASE("the reasons of an operation that ran are the same words", "[load][reason]")
{
    using adl::loader_core::Reason;
    for (const Reason reason :
         {Reason::completed, Reason::target_refused, Reason::target_aborted, Reason::load_failed, Reason::missing_files,
          Reason::protocol_error, Reason::aborted, Reason::media_set_changed, Reason::no_answer, Reason::local_error})
    {
        CAPTURE(to_string(reason));
        CHECK(to_string(adl::load::reason_of(reason)) == to_string(reason));
    }
}
