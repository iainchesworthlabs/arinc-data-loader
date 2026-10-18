#include <QTest>

#include "adl/app/engine_worker.hpp"
#include "test_classes.hpp"

using namespace adl::app;

// The runner's controller: the operator's abort request and the deferred file choice of an
// operator defined download, both written on the window's thread and read by the runner's.
void ControlsTest::a_request_and_a_choice_are_posted_and_forgotten() {
    OperationGate gate;
    QVERIFY(!gate.abort_requested());
    QVERIFY(!gate.selection_posted());

    gate.request_abort();
    QVERIFY(gate.abort_requested());
    gate.clear();
    QVERIFY(!gate.abort_requested());

    gate.post_selection({QStringLiteral("A.BIN"), QStringLiteral("B.BIN")});
    QVERIFY(gate.selection_posted());
    gate.post_selection({});  // the choice "none" is a posted choice too
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
