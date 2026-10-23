#include "adl/app/engine_worker.hpp"
#include "adl/arinc615a/data_loader.hpp"
#include "test_classes.hpp"

#include <QTest>

using namespace adl::app;

// The runner's controller: the operator's abort request and the deferred file choice of an
// operator defined download, both written on the window's thread and read by the runner's.
void ControlsTest::a_request_and_a_choice_are_posted_and_forgotten()
{
    OperationGate gate;
    QVERIFY(!gate.abort_requested());
    QVERIFY(!gate.selection_posted());

    gate.request_abort();
    QVERIFY(gate.abort_requested());
    gate.clear();
    QVERIFY(!gate.abort_requested());

    gate.post_selection({QStringLiteral("A.BIN"), QStringLiteral("B.BIN")});
    QVERIFY(gate.selection_posted());
    gate.post_selection({}); // the choice "none" is a posted choice too
    QVERIFY(gate.selection_posted());
    gate.clear();
    QVERIFY(!gate.selection_posted());

    // One clear forgets both: a job that starts after it is neither aborted nor answered by
    // what was left over.
    gate.request_abort();
    gate.post_selection({QStringLiteral("A.BIN")});
    gate.clear();
    QVERIFY(!gate.abort_requested());
    QVERIFY(!gate.selection_posted());
}

// The choice of the operator is not dropped when the loader is not yet waiting for it (the call
// that gives it fails): it stays posted and is offered at the next poll.
void ControlsTest::a_choice_the_loader_is_not_waiting_for_is_kept()
{
    adl::arinc615a::LoaderConfig config;
    config.thw_id_pos = "T";
    std::vector<std::byte> workspace(adl::arinc615a::DataLoader::workspace_bytes(config));
    adl::arinc615a::LoaderServices services;
    adl::arinc615a::DataLoader loader(config, workspace, services);
    // An Information operation never awaits a file choice.
    QVERIFY(loader.start(adl::arinc615a::Instant{}, adl::arinc615a::InformationOperation{}));

    OperationGate gate;
    gate.post_selection({QStringLiteral("A.BIN")});
    gate.on_poll(loader, adl::arinc615a::Instant{});
    QVERIFY(gate.selection_posted()); // still there, for when the list has arrived
    gate.on_poll(loader, adl::arinc615a::Instant{});
    QVERIFY(gate.selection_posted());
    QVERIFY(!loader.done());
}
