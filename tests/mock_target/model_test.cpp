// The model, headless: MockTargetModel on its worker thread against the loader's engine over
// the fake platform. Starting and stopping, the log lines and progress arriving as signals, and
// a whole Upload whose files the worker writes to the storage directory.

#include "test_classes.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include "adl/arinc615a/directory_files.hpp"
#include "adl/mock/model.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"
#include "temp_dir.hpp"

using namespace adl::mock;
using namespace adl::test;
using namespace adl::arinc615a;

namespace {

// The model with the fake link at 10.0.0.2, and the loader's host at 10.0.0.1 on the same
// network, for tests that run a whole operation through the model.
struct ModelFixture {
    MockRig rig;
    MockTargetModel model;

    ModelFixture()
        : model([this]() -> std::expected<std::unique_ptr<TargetLink>, QString> {
              return std::unique_ptr<TargetLink>(
                  std::make_unique<FakeTargetLink>(rig.world, rig.network, rig.target_address));
          }) {}

    // Schedules the loader's operation as a world event: the model's worker thread advances the
    // world, so the operation runs on the worker's thread against the model's engine.
    [[nodiscard]] std::shared_ptr<PendingRun> schedule_run(const Operation& operation,
                                                           LoaderServices services) {
        return rig.schedule_run(operation, services);
    }
};

[[nodiscard]] Bytes read_disk_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> text{std::istreambuf_iterator<char>(in),
                                 std::istreambuf_iterator<char>()};
    Bytes bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(),
                   [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
    return bytes;
}

[[nodiscard]] bool log_contains(ActivityModel& log, const QString& part) {
    for (int row = 0; row < log.rowCount(); ++row) {
        const QString text = log.data(log.index(row), ActivityModel::TextRole).toString();
        if (text.contains(part)) {
            return true;
        }
    }
    return false;
}

}  // namespace

void MockModelTest::start_stop_runs_the_engine() {
    ModelFixture fixture;
    QVERIFY(!fixture.model.running());

    QSignalSpy log_spy(&fixture.model, &MockTargetModel::logLine);
    fixture.model.start();
    QVERIFY(fixture.model.running());
    QTRY_VERIFY_WITH_TIMEOUT(log_spy.count() > 0, 5000);

    bool listening = false;
    for (int i = 0; i < log_spy.count(); ++i) {
        const auto entry = qvariant_cast<LogEntry>(log_spy.at(i).at(0));
        if (entry.text.contains(QStringLiteral("listening"))) {
            listening = true;
        }
    }
    QVERIFY(listening);

    fixture.model.stop();
    QVERIFY(!fixture.model.running());

    // A second session starts again from nothing.
    fixture.model.start();
    QVERIFY(fixture.model.running());
    fixture.model.stop();
    QVERIFY(!fixture.model.running());
}

void MockModelTest::an_upload_through_the_model_stores_the_files() {
    ModelFixture fixture;
    TempDir storage;
    fixture.model.setStorageDirectory(QString::fromStdU16String(storage.path().u16string()));

    const adl::arinc665::Load load = make_load(5000, 3);
    put_load_files(fixture.rig.loader_host, load);
    DirectoryFileSource source(fixture.rig.loader_host.files(), "/load");
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    LoaderServices services;
    services.source = &source;
    services.listener = &recorder;
    const auto pending = fixture.schedule_run(UploadOperation{UploadRequest{references}}, services);

    QSignalSpy stored_spy(&fixture.model, &MockTargetModel::storedFilesChanged);
    fixture.model.start();
    QVERIFY(fixture.model.running());

    // The operation runs (on the worker's thread, against the model's engine) to its end.
    QTRY_VERIFY_WITH_TIMEOUT(pending->finished.load(std::memory_order_acquire), 20000);
    const auto outcome = pending->outcome;
    QVERIFY(outcome);
    QCOMPARE(static_cast<int>(outcome->code), static_cast<int>(OutcomeCode::completed));

    // The worker wrote the files to the storage directory and reported them.
    QTRY_VERIFY_WITH_TIMEOUT(stored_spy.count() > 0, 20000);
    QTRY_VERIFY_WITH_TIMEOUT(
        qobject_cast<StoredFilesModel*>(fixture.model.storedFiles())->rowCount() ==
            static_cast<int>(1 + load.data_files.size() + load.support_files.size()),
        20000);

    fixture.model.stop();
    QVERIFY(!fixture.model.running());

    // On disk, under <storage>/<load part number>/<file name>, byte for byte.
    const std::filesystem::path part = storage.path() / adl::samples::sample_load_part_number;
    QVERIFY(read_disk_file(part / load.header_file_name) == load.header_bytes);
    for (const auto& file : load.data_files) {
        QVERIFY(read_disk_file(part / file.name) == file.bytes);
    }
    for (const auto& file : load.support_files) {
        QVERIFY(read_disk_file(part / file.name) == file.bytes);
    }

    // The activity log tells the story.
    auto* log = qobject_cast<ActivityModel*>(fixture.model.activity());
    QVERIFY(log_contains(*log, QStringLiteral("Upload operation started")));
    QVERIFY(log_contains(*log, QStringLiteral("Upload operation finished: completed")));
}

void MockModelTest::a_failing_link_reports_the_error() {
    MockTargetModel model([]() -> std::expected<std::unique_ptr<TargetLink>, QString> {
        return std::unexpected(QStringLiteral("the network is down"));
    });
    QSignalSpy error_spy(&model, &MockTargetModel::errorOccurred);
    model.start();
    QVERIFY(model.running());
    QTRY_VERIFY_WITH_TIMEOUT(error_spy.count() > 0, 5000);
    QVERIFY(!model.running());
    QCOMPARE(model.errorText(), QStringLiteral("the network is down"));
}
