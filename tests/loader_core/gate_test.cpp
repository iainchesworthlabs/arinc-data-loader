// SPDX-License-Identifier: GPL-3.0-or-later
// The operator's controls of a running operation: the abort and the deferred file choice, both written by
// one thread and read by the runner's.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/loader_core/gate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace adl::loader_core;

TEST_CASE("a request and a choice are posted and forgotten", "[loader_core][gate]")
{
    OperationGate gate;
    CHECK_FALSE(gate.abort_requested());
    CHECK_FALSE(gate.selection_posted());

    gate.request_abort();
    CHECK(gate.abort_requested());
    gate.clear();
    CHECK_FALSE(gate.abort_requested());

    gate.post_selection({"A.BIN", "B.BIN"});
    CHECK(gate.selection_posted());
    gate.post_selection({}); // the choice "none" is a posted choice too
    CHECK(gate.selection_posted());
    gate.clear();
    CHECK_FALSE(gate.selection_posted());

    // One clear forgets both: an operation that starts after it is neither aborted nor answered by what
    // was left over.
    gate.request_abort();
    gate.post_selection({"A.BIN"});
    gate.clear();
    CHECK_FALSE(gate.abort_requested());
    CHECK_FALSE(gate.selection_posted());
}

TEST_CASE("a choice the loader is not waiting for is kept for the next poll", "[loader_core][gate]")
{
    adl::arinc615a::LoaderConfig config;
    config.thw_id_pos = "T";
    std::vector<std::byte> workspace(adl::arinc615a::DataLoader::workspace_bytes(config));
    adl::arinc615a::LoaderServices services;
    adl::arinc615a::DataLoader loader(config, workspace, services);
    // An Information operation never awaits a file choice.
    REQUIRE(loader.start(adl::arinc615a::Instant{}, adl::arinc615a::InformationOperation{}));

    OperationGate gate;
    gate.post_selection({"A.BIN"});
    gate.on_poll(loader, adl::arinc615a::Instant{});
    CHECK(gate.selection_posted()); // still there, for when the list has arrived
    gate.on_poll(loader, adl::arinc615a::Instant{});
    CHECK(gate.selection_posted());
    CHECK_FALSE(loader.done());
}
