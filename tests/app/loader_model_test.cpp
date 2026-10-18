#include <expected>
#include <filesystem>
#include <memory>

#include <QDir>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "adl/app/loader_model.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "rig.hpp"
#include "test_classes.hpp"

using namespace adl::app;

namespace {

// The engine's host: the loader's side of a rig (a fake network with a simulated target on it).
// With a semaphore, the worker's thread waits for it before it gets the host, which lets a test
// do something while a job is certain to be running, however fast the (virtual) time would be.
HostFactory rig_host(adl::test::Rig& rig, QSemaphore* hold = nullptr) {
    return [&rig, hold]() -> std::expected<HostLease, QString> {
        if (hold != nullptr) {
            hold->acquire();
        }
        HostLease lease;
        lease.host = &rig.loader_host;
        return lease;
    };
}

bool log_contains(LoaderModel& model, const QString& text) {
    const auto* events = qobject_cast<EventModel*>(model.events());
    for (int row = 0; row < events->rowCount(); ++row) {
        if (events->data(events->index(row), EventModel::TextRole).toString().contains(text)) {
            return true;
        }
    }
    return false;
}

void point_at_the_target(LoaderModel& model) {
    model.setTargetAddress(QStringLiteral("10.0.0.2"));
    model.setThwIdPos(QStringLiteral("HNPFMS_L"));
}

}  // namespace

void LoaderModelTest::information_reads_the_configuration() {
    adl::test::Rig rig;
    LoaderModel model(rig_host(rig));
    point_at_the_target(model);
    QVERIFY(model.canReadInformation());
    QVERIFY(!model.busy());
    QCOMPARE(model.resultState(), 0);

    QSignalSpy busy_changed(&model, &LoaderModel::busyChanged);
    model.readInformation();
    QVERIFY(model.busy());
    QVERIFY(model.canAbort());
    QVERIFY(!model.canReadInformation());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(busy_changed.count() >= 2);

    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QVERIFY(!model.canAbort());
    QVERIFY(model.canReadInformation());

    // What the simulated target holds: one piece of hardware with two parts.
    const auto* configuration = qobject_cast<ConfigurationModel*>(model.configuration());
    QCOMPARE(configuration->rowCount(), 3);
    const auto title = [&](int row) {
        return configuration->data(configuration->index(row), ConfigurationModel::TitleRole)
            .toString();
    };
    const auto level = [&](int row) {
        return configuration->data(configuration->index(row), ConfigurationModel::LevelRole)
            .toInt();
    };
    QCOMPARE(title(0), QStringLiteral("FMS LEFT"));
    QCOMPARE(level(0), 0);
    QCOMPARE(title(1), QStringLiteral("ACM47-1234-5678"));
    QCOMPARE(level(1), 1);
    QCOMPARE(title(2), QStringLiteral("ACM47-8765-4321 A"));

    // And the log tells what happened, in order.
    QVERIFY(log_contains(model, QStringLiteral("Asking 10.0.0.2 (HNPFMS_L)")));
    QVERIFY(log_contains(model, QStringLiteral("The target answered: accepted")));
    QVERIFY(log_contains(model, QStringLiteral("Status ")));
    QVERIFY(log_contains(model, QStringLiteral("Finished: completed")));

    // The target saw exactly one request for its initialization file.
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{1});
}

void LoaderModelTest::find_lists_the_targets() {
    adl::test::SimPlan plan;
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::test::Rig rig(plan);
    LoaderModel model(rig_host(rig));

    model.findTargets();
    QVERIFY(model.busy());
    QVERIFY(!model.canAbort());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("1 target(s) answered"));
    const auto* targets = qobject_cast<TargetModel*>(model.targets());
    QCOMPARE(targets->rowCount(), 1);
    const QModelIndex row = targets->index(0);
    QCOMPARE(targets->data(row, TargetModel::AddressRole).toString(), QStringLiteral("10.0.0.2"));
    QCOMPARE(targets->data(row, TargetModel::ThwIdPosRole).toString(), QStringLiteral("HNPFMS_L"));
    QCOMPARE(targets->data(row, TargetModel::ManufacturerRole).toString(), QStringLiteral("HNP"));

    // A second FIND to one address that does not answer finds nothing, and replaces the list.
    model.findTargets(QStringLiteral("10.0.0.9"));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(targets->rowCount(), 0);
}

void LoaderModelTest::selecting_a_target_fills_the_address() {
    adl::test::SimPlan plan;
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "R", "FMS RIGHT", "HNP"};
    adl::test::Rig rig(plan);
    LoaderModel model(rig_host(rig));
    QVERIFY(!model.canReadInformation());

    model.findTargets();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QSignalSpy changed(&model, &LoaderModel::targetChanged);
    model.selectTarget(7);  // there is no such row
    QCOMPARE(changed.count(), 0);
    model.selectTarget(0);
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_R"));
    QVERIFY(model.targetValid());
    QVERIFY(model.canReadInformation());
}

void LoaderModelTest::a_target_needs_an_address_and_a_name() {
    adl::test::Rig rig;
    LoaderModel model(rig_host(rig));
    QVERIFY(!model.targetValid());

    model.setTargetAddress(QStringLiteral("10.0.0.2"));
    QVERIFY(!model.targetValid());  // no name yet
    model.setThwIdPos(QStringLiteral("HNPFMS_L"));
    QVERIFY(model.targetValid());
    model.setTargetAddress(QStringLiteral("10.0.0"));
    QVERIFY(!model.targetValid());
    QVERIFY(!model.canReadInformation());

    // Starting anyway does nothing.
    model.readInformation();
    QVERIFY(!model.busy());
    QCOMPARE(model.resultState(), 0);

    // Surrounding blanks are not part of an address or a name.
    model.setTargetAddress(QStringLiteral("  10.0.0.2 "));
    model.setThwIdPos(QStringLiteral(" HNPFMS_L"));
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_L"));
    QVERIFY(model.canReadInformation());
}

void LoaderModelTest::abort_before_the_target_accepts_ends_the_operation() {
    adl::test::Rig rig;
    QSemaphore hold;
    LoaderModel model(rig_host(rig, &hold));
    point_at_the_target(model);

    model.readInformation();
    QVERIFY(model.canAbort());
    model.abort();
    QVERIFY(!model.canAbort());  // asked once
    QVERIFY(model.activity().contains(QStringLiteral("Stopping")));
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("aborted by the operator"));
    QVERIFY(log_contains(model, QStringLiteral("Finished: aborted by the operator")));
    // An abort does not leave the model unable to start the next job.
    model.readInformation();
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
}

void LoaderModelTest::a_second_job_is_refused_while_one_runs() {
    adl::test::Rig rig;
    QSemaphore hold;
    LoaderModel model(rig_host(rig, &hold));
    point_at_the_target(model);

    model.readInformation();
    QVERIFY(model.busy());
    model.findTargets();
    model.chooseMediaSet(QUrl::fromLocalFile(QDir::tempPath()));
    model.readInformation();
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QVERIFY(!log_contains(model, QStringLiteral("Looking for")));
    QVERIFY(!log_contains(model, QStringLiteral("Reading the media set")));
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{1});
}

void LoaderModelTest::a_failing_host_ends_the_job_with_the_reason() {
    LoaderModel model([]() -> std::expected<HostLease, QString> {
        return std::unexpected(QStringLiteral("no network adapter"));
    });
    point_at_the_target(model);
    model.readInformation();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("no network adapter"));
    QVERIFY(log_contains(model, QStringLiteral("no network adapter")));

    model.findTargets();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 2);

    // A FIND to something that is not an address is refused before the host is asked for.
    model.findTargets(QStringLiteral("not an address"));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(model.resultText().contains(QStringLiteral("not an IPv4 address")));
}

void LoaderModelTest::media_set_is_read_from_a_directory() {
    auto spec = adl::samples::sample_media_set_spec();
    QVERIFY(spec.has_value());
    auto members = adl::arinc665::build_media_set(*spec);
    QVERIFY(members.has_value());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto written = adl::media_tree::write_media_set_tree(
        std::filesystem::path(directory.path().toStdU16String()), *members);
    QVERIFY(written.has_value());

    adl::test::Rig rig;
    LoaderModel model(rig_host(rig));
    QSignalSpy changed(&model, &LoaderModel::mediaSetChanged);
    model.chooseMediaSet(QUrl::fromLocalFile(directory.path()));
    QVERIFY(model.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QVERIFY2(model.mediaSetError().isEmpty(), qPrintable(model.mediaSetError()));
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(changed.count(), 1);
    QVERIFY2(model.mediaSetTitle().contains(
                 QString::fromLatin1(adl::samples::sample_media_set_part_number)),
             qPrintable(model.mediaSetTitle()));
    const auto* loads = qobject_cast<LoadModel*>(model.loads());
    QVERIFY(loads->rowCount() >= 1);
    QCOMPARE(loads->data(loads->index(0), LoadModel::PartNumberRole).toString(),
             QString::fromLatin1(adl::samples::sample_load_part_number));
    QVERIFY(model.mediaSetPath().contains(QDir(directory.path()).dirName()));
}

void LoaderModelTest::media_set_that_is_not_there_is_reported() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    adl::test::Rig rig;
    LoaderModel model(rig_host(rig));
    model.chooseMediaSet(QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing"))));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 2);
    QVERIFY(!model.mediaSetError().isEmpty());
    QVERIFY(model.mediaSetTitle().isEmpty());
    QCOMPARE(qobject_cast<LoadModel*>(model.loads())->rowCount(), 0);
    QVERIFY(log_contains(model, QStringLiteral("could not be read")));
}

void LoaderModelTest::destroying_the_model_waits_for_the_engine() {
    adl::test::Rig rig;
    QSemaphore hold;
    auto model = std::make_unique<LoaderModel>(rig_host(rig, &hold));
    point_at_the_target(*model);
    model->readInformation();
    hold.release();
    // The job is queued or running; the destructor aborts it and joins the thread. Nothing may
    // be left that calls back into the model, which AddressSanitizer would report.
    model.reset();
    QVERIFY(true);
}
