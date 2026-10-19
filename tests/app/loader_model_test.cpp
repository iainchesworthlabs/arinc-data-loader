#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <QDir>
#include <QSemaphore>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "adl/app/loader_model.hpp"
#include "adl/app/media_set_summary.hpp"
#include "adl/app/settings.hpp"
#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "bytes.hpp"
#include "rig.hpp"
#include "test_classes.hpp"
#include "upload_fixture.hpp"

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

// The operator's preferences in a file of their own, so no test reads (or writes) the settings
// of another one.
std::unique_ptr<AppSettings> settings_in(QTemporaryDir& directory,
                                         const QString& name = QStringLiteral("settings.ini")) {
    return std::make_unique<AppSettings>(
        std::make_unique<QSettings>(directory.filePath(name), QSettings::IniFormat));
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

// Reads the fixture's media set through the model and waits for it (the read, then the check
// that follows it while validateOnOpen is on).
void read_the_media_set(LoaderModel& model, const adl::test::UploadFixture& fixture) {
    model.chooseMediaSet(fixture.url());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY2(model.mediaSetError().isEmpty(), qPrintable(model.mediaSetError()));
    QCOMPARE(model.validationState(), 2);
}

// Waits until the model is idle and the check that was running has been recorded.
void wait_for_the_check(LoaderModel& model, int expected_state) {
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.validationState(), expected_state);
}

void tick(LoaderModel& model, int row, bool ticked = true) {
    auto* loads = qobject_cast<LoadModel*>(model.loads());
    QVERIFY(loads->setData(loads->index(row), ticked, LoadModel::CheckedRole));
}

QVariant load_field(LoaderModel& model, int row, int role) {
    auto* loads = qobject_cast<LoadModel*>(model.loads());
    return loads->data(loads->index(row), role);
}

void point_at_the_target(LoaderModel& model) {
    model.setTargetAddress(QStringLiteral("10.0.0.2"));
    model.setThwIdPos(QStringLiteral("HNPFMS_L"));
}

// The bytes of the file called `name` somewhere below `root`, and its path (empty when there is
// no such file).
std::vector<std::byte> read_file(const std::filesystem::path& root, const std::string& name,
                                 std::filesystem::path* where = nullptr) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename().string() == name) {
            if (where != nullptr) {
                *where = entry.path();
            }
            std::ifstream in(entry.path(), std::ios::binary);
            const std::vector<char> text{std::istreambuf_iterator<char>(in),
                                         std::istreambuf_iterator<char>()};
            std::vector<std::byte> bytes(text.size());
            std::memcpy(bytes.data(), text.data(), text.size());
            return bytes;
        }
    }
    return {};
}

// The 16-bit CRC of the bytes, as FILES.LUM writes it (four hexadecimal digits).
QString crc16_hex_of(std::span<const std::byte> bytes) {
    const auto value =
        adl::arinc665::compute_check_value(adl::arinc665::CheckValueType::crc16, bytes);
    const std::uint16_t crc =
        static_cast<std::uint16_t>((std::to_integer<unsigned>(value->bytes[0]) << 8) |
                                   std::to_integer<unsigned>(value->bytes[1]));
    return QStringLiteral("%1").arg(crc, 4, 16, QLatin1Char('0')).toUpper();
}

// The plan of a download: three files the simulated target can give.
adl::test::SimPlan download_plan() {
    adl::test::SimPlan plan;
    plan.files = {
        adl::test::DownloadItem{"A.BIN", adl::test::bytes_of("aaa"), "first file"},
        adl::test::DownloadItem{"B.BIN", adl::test::bytes_of("bbbb"), "second file"},
        adl::test::DownloadItem{"C.BIN", adl::test::repeated(std::byte{0x2A}, 6), ""},
    };
    return plan;
}

// The directory the downloads go to, as the operator's preference holds it.
QString download_dir_of(const adl::test::TempDir& directory) {
    return QString::fromStdU16String(directory.path().u16string());
}

// The bytes of `name` in the DNLD_DATA_HNPFMS_L_<number> directory under `directory`, as the
// fake host's file system holds them (nothing, when there is no such file).
std::optional<std::vector<std::byte>> downloaded(adl::test::Rig& rig,
                                                 const std::filesystem::path& directory,
                                                 const std::string& name, unsigned number = 1) {
    const std::u8string generic = directory.generic_u8string();
    const std::string path =
        std::string{reinterpret_cast<const char*>(generic.data()), generic.size()} +
        "/DNLD_DATA_HNPFMS_L_" + std::to_string(number) + "/" + name;
    return rig.loader_host.fake_files().get(path);
}

bool holds(adl::test::Rig& rig, const std::filesystem::path& directory, const std::string& name,
           const std::vector<std::byte>& bytes, unsigned number = 1) {
    const std::optional<std::vector<std::byte>> found = downloaded(rig, directory, name, number);
    return found.has_value() && *found == bytes;
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
    QCOMPARE(model.resultText(), QStringLiteral("1 target answered"));
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
    // A file of its own: the target of a last operation is restored from the settings, and this
    // test is about a window with no target yet.
    QTemporaryDir settings_dir;
    LoaderModel model(rig_host(rig), settings_in(settings_dir));
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
    // As above: no stored last target, so the window starts with the fields empty.
    QTemporaryDir settings_dir;
    LoaderModel model(rig_host(rig), settings_in(settings_dir));
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
    // The failure surfaces for the window's dialog, with what the operator can do about it: the
    // target never confirmed the stop.
    QCOMPARE(model.lastErrorText(), QStringLiteral("aborted by the operator"));
    QVERIFY(model.lastErrorDetail().contains(QStringLiteral("did not confirm")));
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("may still be loading")));
    model.dismissError();
    QVERIFY(model.lastErrorText().isEmpty());
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
    // The set was read and, with validateOnOpen on, checked: two mediaSetChanged, and the check
    // says it is valid.
    QCOMPARE(changed.count(), 2);
    QCOMPARE(model.validationState(), 2);
    QVERIFY(model.validationError().isEmpty());
    QVERIFY(model.validationDetail().contains(QStringLiteral("check values verified")));
    QVERIFY2(model.mediaSetTitle().contains(
                 QString::fromLatin1(adl::samples::sample_media_set_part_number)),
             qPrintable(model.mediaSetTitle()));
    QCOMPARE(model.mediaSetPartNumber(),
             QString::fromLatin1(adl::samples::sample_media_set_part_number));
    QCOMPARE(model.mediaSetMemberCount(), 2);
    QCOMPARE(model.mediaSetBatchCount(), 1);
    const auto* loads = qobject_cast<LoadModel*>(model.loads());
    QVERIFY(loads->rowCount() >= 1);
    QCOMPARE(loads->data(loads->index(0), LoadModel::PartNumberRole).toString(),
             QString::fromLatin1(adl::samples::sample_load_part_number));
    QVERIFY(model.mediaSetPath().contains(QDir(directory.path()).dirName()));

    // The files of the set, from FILES.LUM, with their CRCs.
    const auto* files = qobject_cast<FileModel*>(model.files());
    QVERIFY(files->rowCount() >= 6);
    bool saw_header = false;
    for (int row = 0; row < files->rowCount(); ++row) {
        const QModelIndex index = files->index(row);
        QVERIFY(!files->data(index, FileModel::NameRole).toString().isEmpty());
        QVERIFY(files->data(index, FileModel::MemberRole).toInt() >= 1);
        QVERIFY(!files->data(index, FileModel::PathnameRole).toString().isEmpty());
        const QString crc = files->data(index, FileModel::CrcRole).toString();
        QCOMPARE(crc.size(), 4);
        for (const QChar digit : crc) {
            QVERIFY((digit >= QLatin1Char('0') && digit <= QLatin1Char('9')) ||
                    (digit >= QLatin1Char('A') && digit <= QLatin1Char('F')));
        }
        saw_header = saw_header || files->data(index, FileModel::NameRole).toString() ==
                                       QStringLiteral("ACM47.LUH");
    }
    QVERIFY(saw_header);
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
    QVERIFY(!model.canUpload());
    // A set that cannot be read is invalid, with the reason, and stays refused.
    QCOMPARE(model.validationState(), 3);
    QCOMPARE(model.validationError(), model.mediaSetError());
    QVERIFY(model.lastErrorText().isEmpty() == false);
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("Choose the directory")));
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

void LoaderModelTest::upload_sends_the_ticked_loads() {
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));
    point_at_the_target(model);
    read_the_media_set(model, fixture);
    QCOMPARE(qobject_cast<LoadModel*>(model.loads())->rowCount(), 1);
    QCOMPARE(model.progress(), -1);

    QSignalSpy can_upload(&model, &LoaderModel::canUploadChanged);
    tick(model, 0);
    QCOMPARE(model.selectedLoadCount(), 1);
    QVERIFY(model.canUpload());
    QCOMPARE(can_upload.count(), 1);

    std::vector<int> progress;
    QObject::connect(&model, &LoaderModel::progressChanged, &model,
                     [&] { progress.push_back(model.progress()); });
    model.uploadSelected();
    QVERIFY(model.busy());
    QVERIFY(model.canAbort());
    QVERIFY(!model.canUpload());
    QVERIFY(model.activity().contains(QStringLiteral("Uploading")));
    // The ticks stay as they were while it runs.
    auto* loads = qobject_cast<LoadModel*>(model.loads());
    QVERIFY(!loads->setData(loads->index(0), false, LoadModel::CheckedRole));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    const auto& reads = fixture.load.item.reads;
    const QString served = reads.size() == 1
                               ? QStringLiteral("The target read 1 file")
                               : QStringLiteral("The target read %1 files").arg(reads.size());
    QVERIFY2(model.resultDetail().contains(served), qPrintable(model.resultDetail()));
    QVERIFY(model.canUpload());  // and it can be done again
    QVERIFY(!model.canAbort());

    // The target read the header and every file of the load, the right ones, and nothing else.
    std::vector<std::string> names;
    for (const auto& read : reads) {
        names.push_back(read.name);
    }
    QVERIFY(rig.target.log().files_read == names);
    QVERIFY(rig.target.log().files_corrupt.empty());
    QVERIFY(rig.target.log().final_status == adl::arinc615a::StatusCode::completed);

    // Progress came from the target's status files, and only ever went forward to the whole.
    QVERIFY(!progress.empty());
    QCOMPARE(progress.back(), 100);
    QCOMPARE(model.progress(), 100);
    QVERIFY(std::is_sorted(progress.begin(), progress.end()));
    QCOMPARE(load_field(model, 0, LoadModel::RatioRole).toInt(), 100);
    QCOMPARE(load_field(model, 0, LoadModel::LoadStatusRole).toString(),
             QStringLiteral("completed"));
    QVERIFY(!load_field(model, 0, LoadModel::FailedRole).toBool());

    QVERIFY(log_contains(model, QStringLiteral("Uploading 1 load to 10.0.0.2 (HNPFMS_L)")));
    QVERIFY(log_contains(
        model, QStringLiteral("The target read %1").arg(QString::fromStdString(names.front()))));
    QVERIFY(log_contains(model, QStringLiteral("% of the list")));
    QVERIFY(log_contains(model, QStringLiteral("Finished: completed")));
}

void LoaderModelTest::an_upload_needs_a_target_a_media_set_and_a_tick() {
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));

    // Nothing to upload yet, and asking for it changes nothing.
    QVERIFY(!model.canUpload());
    model.uploadSelected();
    QVERIFY(!model.busy());
    QCOMPARE(model.resultState(), 0);

    read_the_media_set(model, fixture);
    point_at_the_target(model);
    QVERIFY(!model.canUpload());  // no load ticked
    model.uploadSelected();
    QVERIFY(!model.busy());

    model.selectAllLoads(true);
    QCOMPARE(model.selectedLoadCount(), 1);
    QVERIFY(model.canUpload());
    model.setTargetAddress(QStringLiteral("10.0.0"));
    QVERIFY(!model.canUpload());  // not an address
    model.setTargetAddress(QStringLiteral("10.0.0.2"));
    QVERIFY(model.canUpload());
    model.selectAllLoads(false);
    QVERIFY(!model.canUpload());
    QCOMPARE(model.selectedLoadCount(), 0);

    // A media set that cannot be read leaves nothing to upload (the error and the failed check
    // block it); the loads of the last readable set stay, ticks and all, so the operator can
    // point the loader back at the right directory without re-ticking.
    model.selectAllLoads(true);
    QVERIFY(model.canUpload());
    model.chooseMediaSet(QUrl::fromLocalFile(
        QString::fromStdU16String((fixture.directory.path() / "missing").u16string())));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(!model.canUpload());
    QCOMPARE(model.selectedLoadCount(), 1);
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});
}

void LoaderModelTest::a_load_the_target_cannot_read_ends_the_upload() {
    adl::test::UploadFixture fixture;
    adl::test::SimPlan plan = fixture.plan();
    plan.loads[0].reads.push_back(
        adl::test::ReadItem{"MISSING.BIN", adl::test::Bytes(3), std::nullopt});
    adl::test::Rig rig(plan);
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));
    point_at_the_target(model);
    read_the_media_set(model, fixture);
    model.selectAllLoads(true);

    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // The target asked for a file the media set does not have, was refused, and gave up.
    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("aborted by the target"));
    QVERIFY(rig.target.log().read_failures == std::vector<std::string>{"MISSING.BIN"});
    QVERIFY(log_contains(model, QStringLiteral("Finished: aborted by the target")));
    QVERIFY(model.canUpload());  // the operator can try again
}

void LoaderModelTest::abort_before_the_target_accepts_ends_the_upload() {
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    QSemaphore hold;
    LoaderModel model(rig_host(rig, &hold));
    point_at_the_target(model);
    read_the_media_set(model, fixture);
    model.selectAllLoads(true);

    model.uploadSelected();
    QVERIFY(model.canAbort());
    model.abort();
    QVERIFY(!model.canAbort());
    QVERIFY(model.activity().contains(QStringLiteral("Stopping")));
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("aborted by the operator"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("The target read 0 files")));
    QVERIFY(rig.target.log().files_read.empty());

    // The next upload is not aborted by the last one's request.
    model.uploadSelected();
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
}

void LoaderModelTest::selecting_a_target_exposes_its_properties() {
    adl::test::SimPlan plan;
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "R", "FMS RIGHT", "HNP"};
    adl::test::Rig rig(plan);
    LoaderModel model(rig_host(rig));
    QCOMPARE(model.selectedTargetIndex(), -1);
    QVERIFY(model.selectedTargetHardwareId().isEmpty());

    model.findTargets();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    model.selectTarget(0);
    QCOMPARE(model.selectedTargetIndex(), 0);
    QCOMPARE(model.selectedTargetHardwareId(), QStringLiteral("HNPFMS"));
    QCOMPARE(model.selectedTargetPosition(), QStringLiteral("R"));
    QCOMPARE(model.selectedTargetTypeName(), QStringLiteral("FMS"));
    QCOMPARE(model.selectedTargetLiteralName(), QStringLiteral("FMS RIGHT"));
    QCOMPARE(model.selectedTargetManufacturer(), QStringLiteral("HNP"));
    QCOMPARE(model.selectedTargetAddress(), QStringLiteral("10.0.0.2"));

    // A FIND that finds nobody replaces the list, and there is nothing chosen any more.
    model.findTargets(QStringLiteral("10.0.0.9"));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.selectedTargetIndex(), -1);
    QVERIFY(model.selectedTargetHardwareId().isEmpty());
}

void LoaderModelTest::clearing_a_target_field_restores_its_default() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setDefaultTargetAddress(QStringLiteral("10.0.0.2"));
    settings->setDefaultThwIdPos(QStringLiteral("HNPFMS_L"));
    adl::test::Rig rig;
    LoaderModel model(rig_host(rig), std::move(settings));

    // Prefilled at start-up.
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_L"));
    QVERIFY(model.targetValid());
    // Typing something else works; clearing a field brings its default back.
    model.setTargetAddress(QStringLiteral("10.0.0.9"));
    model.setTargetAddress(QStringLiteral("   "));
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    model.setThwIdPos(QStringLiteral("   "));
    QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_L"));

    // Without a default, clearing clears; naming a default fills a field that is empty.
    QTemporaryDir settings_dir2;
    auto settings2 = settings_in(settings_dir2);
    adl::test::Rig rig2;
    LoaderModel model2(rig_host(rig2), std::move(settings2));
    QVERIFY(model2.targetAddress().isEmpty());
    model2.setTargetAddress(QStringLiteral("   "));
    QVERIFY(model2.targetAddress().isEmpty());
    model2.setDefaultTargetAddress(QStringLiteral("10.1.1.1"));
    QCOMPARE(model2.targetAddress(), QStringLiteral("10.1.1.1"));
}

void LoaderModelTest::the_first_run_screen_is_shown_until_it_is_passed() {
    QTemporaryDir settings_dir;
    {
        adl::test::Rig rig;
        LoaderModel model(rig_host(rig), settings_in(settings_dir));
        QVERIFY(model.firstRun());
        model.setDefaultTargetAddress(QStringLiteral("10.0.0.2"));
        model.setDefaultThwIdPos(QStringLiteral("HNPFMS_L"));
        model.completeFirstRun();
        QVERIFY(!model.firstRun());
        // What the first run set is what the window starts with, so the first operation does not
        // retype it.
        QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
        QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_L"));
        QVERIFY(model.targetValid());
    }

    // The next launch goes straight to the workbench.
    adl::test::Rig rig;
    LoaderModel again(rig_host(rig), settings_in(settings_dir));
    QVERIFY(!again.firstRun());
    QCOMPARE(again.targetAddress(), QStringLiteral("10.0.0.2"));
}

void LoaderModelTest::the_target_of_the_last_operation_comes_back() {
    QTemporaryDir settings_dir;
    {
        adl::test::Rig rig;
        LoaderModel model(rig_host(rig), settings_in(settings_dir));
        // The defaults are one thing; the target an operation actually used is another.
        model.setDefaultTargetAddress(QStringLiteral("10.0.0.1"));
        model.setDefaultThwIdPos(QStringLiteral("DEFAULT_L"));
        point_at_the_target(model);
        model.readInformation();
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
        QCOMPARE(model.resultState(), 1);
    }

    // A new window starts against the last operation's target, not a default.
    adl::test::Rig rig;
    LoaderModel again(rig_host(rig), settings_in(settings_dir));
    QCOMPARE(again.targetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(again.thwIdPos(), QStringLiteral("HNPFMS_L"));
}

void LoaderModelTest::a_failure_surfaces_for_the_dialog() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    LoaderModel model(
        []() -> std::expected<HostLease, QString> {
            return std::unexpected(QStringLiteral("no network adapter"));
        },
        std::move(settings));
    point_at_the_target(model);
    model.readInformation();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // One failure, one error for the dialog, with the hint the application knows for it.
    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.lastErrorText(), QStringLiteral("no network adapter"));
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("network")));
    model.dismissError();
    QVERIFY(model.lastErrorText().isEmpty());

    // The next failure surfaces again.
    model.findTargets();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.lastErrorText(), QStringLiteral("no network adapter"));

    // A media set that cannot be read has its own hint.
    model.chooseMediaSet(QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/no-such-set")));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 2);
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("Choose the directory")));

    // And so has an address that is not one.
    model.findTargets(QStringLiteral("not an address"));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(model.lastErrorText().contains(QStringLiteral("not an IPv4 address")));
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("0 to 255")));
}

void LoaderModelTest::the_summary_lists_the_files_with_their_crcs() {
    auto spec = adl::samples::sample_media_set_spec();
    QVERIFY(spec.has_value());
    auto members = adl::arinc665::build_media_set(*spec);
    QVERIFY(members.has_value());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto written = adl::media_tree::write_media_set_tree(
        std::filesystem::path(directory.path().toStdU16String()), *members);
    QVERIFY(written.has_value());

    const MediaSetSummary summary = read_media_set_summary(directory.path());
    QVERIFY2(summary.error.isEmpty(), qPrintable(summary.error));
    QVERIFY(!summary.files.isEmpty());

    // Every file FILES.LUM lists is on the member it names, and its CRC is the CRC of the
    // bytes on the disk.
    const std::filesystem::path root{directory.path().toStdU16String()};
    for (const MediaFileSummary& file : summary.files) {
        const auto dir = adl::media_tree::native_directory(
            root / adl::media_tree::member_directory_name(static_cast<std::uint16_t>(file.member)),
            file.pathname.toStdString());
        QVERIFY2(dir.has_value(), (file.name + " in " + file.pathname).toStdString().c_str());
        const std::vector<std::byte> bytes = read_file(*dir, file.name.toStdString());
        QVERIFY2(!bytes.empty(), qPrintable(file.name));
        QCOMPARE(crc16_hex_of(bytes), file.crc16);
    }
}

void LoaderModelTest::a_set_is_uploadable_only_after_the_check_passes() {
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));
    point_at_the_target(model);

    // Choosing the set leaves it unchecked, and nothing can be uploaded.
    model.chooseMediaSet(fixture.url());
    QCOMPARE(model.validationState(), 0);
    QVERIFY(!model.canUpload());
    wait_for_the_check(model, 2);
    QVERIFY(!model.canUpload());  // nothing ticked

    // The set that passed the check can be uploaded once a load is ticked.
    tick(model, 0);
    QVERIFY(model.canUpload());
    QCOMPARE(model.validationState(), 2);
}

void LoaderModelTest::a_corrupted_set_fails_the_check_and_stays_refused() {
    // One byte of a file changed: the read (which is the check) sees the CRC and says so.
    {
        adl::test::UploadFixture fixture;
        adl::test::Rig rig(fixture.plan());
        QVERIFY(fixture.write(rig));
        std::filesystem::path where;
        const std::vector<std::byte> data = read_file(fixture.directory.path(), "APP.LUP", &where);
        QVERIFY(!data.empty());
        std::vector<std::byte> corrupt = data;
        corrupt[0] ^= std::byte{0x01};
        std::ofstream out(where, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(corrupt.data()),
                  static_cast<std::streamsize>(corrupt.size()));

        LoaderModel model(rig_host(rig));
        point_at_the_target(model);
        model.chooseMediaSet(fixture.url());
        wait_for_the_check(model, 3);
        QVERIFY(!model.validationError().isEmpty());
        QCOMPARE(model.validationError(), model.mediaSetError());
        QVERIFY(!model.canUpload());
        model.uploadSelected();
        QVERIFY(!model.busy());  // refused
    }

    // A file taken away is a read error too.
    {
        adl::test::UploadFixture fixture;
        adl::test::Rig rig(fixture.plan());
        QVERIFY(fixture.write(rig));
        std::filesystem::path where;
        QVERIFY(!read_file(fixture.directory.path(), "APP.LUP", &where).empty());
        std::filesystem::remove(where);

        LoaderModel model(rig_host(rig));
        point_at_the_target(model);
        model.chooseMediaSet(fixture.url());
        wait_for_the_check(model, 3);
        QVERIFY(!model.validationError().isEmpty());
        QVERIFY(!model.canUpload());
        model.uploadSelected();
        QVERIFY(!model.busy());
    }
}

void LoaderModelTest::check_media_set_revalidates_files_changed_on_disk() {
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));
    read_the_media_set(model, fixture);
    point_at_the_target(model);
    model.selectAllLoads(true);
    QVERIFY(model.canUpload());

    // A file changes on the disk after the check passed. The next check reads it afresh and
    // fails, the reason is named, and the upload stays refused.
    std::filesystem::path where;
    const std::vector<std::byte> data = read_file(fixture.directory.path(), "APP.LUP", &where);
    QVERIFY(!data.empty());
    std::vector<std::byte> corrupt = data;
    corrupt[0] ^= std::byte{0x01};
    {
        std::ofstream out(where, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(corrupt.data()),
                  static_cast<std::streamsize>(corrupt.size()));
    }
    model.checkMediaSet();
    wait_for_the_check(model, 3);
    QVERIFY(!model.validationError().isEmpty());
    QCOMPARE(model.selectedLoadCount(), 1);  // the tick followed the load
    QVERIFY(!model.canUpload());
    model.uploadSelected();
    QVERIFY(!model.busy());

    // The file is put back: the check passes again, and the upload goes ahead (through the
    // index the check rebuilt).
    {
        std::ofstream out(where, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    model.checkMediaSet();
    wait_for_the_check(model, 2);
    QVERIFY(model.canUpload());
    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
}

void LoaderModelTest::findings_warn_but_do_not_block_the_upload() {
    // A media set part number with an I in it: the standard recommends against it, and the read
    // reports it as a finding (a warning), not as an error.
    adl::test::UploadFixture fixture;
    fixture.media_set_part_number = "ACM-SET-000I";
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig));
    read_the_media_set(model, fixture);

    QVERIFY(!model.mediaSetFindings().isEmpty());
    QCOMPARE(model.validationFindings(), model.mediaSetFindings());
    QVERIFY2(model.validationDetail().contains(QStringLiteral("warning")),
             qPrintable(model.validationDetail()));
    point_at_the_target(model);
    model.selectAllLoads(true);
    QVERIFY(model.canUpload());

    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QCOMPARE(rig.target.log().files_read.size(), fixture.load.item.reads.size());
}

void LoaderModelTest::with_validate_on_open_off_the_set_waits_for_a_check() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setValidateOnOpen(false);
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig), std::move(settings));

    model.chooseMediaSet(fixture.url());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(model.mediaSetError().isEmpty());
    QCOMPARE(model.validationState(), 0);  // read, but not checked
    point_at_the_target(model);
    model.selectAllLoads(true);
    QVERIFY(!model.canUpload());
    model.uploadSelected();
    QVERIFY(!model.busy());  // refused: the check has not said it is valid

    // The operator asks for the check, it passes, and the upload goes ahead.
    model.checkMediaSet();
    wait_for_the_check(model, 2);
    QVERIFY(model.canUpload());
    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
}

void LoaderModelTest::the_control_port_flows_into_the_loader() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setControlPort(1615);
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig), std::move(settings));
    read_the_media_set(model, fixture);
    point_at_the_target(model);
    model.selectAllLoads(true);

    // The port is sent to the target in the port option, the target honours it, and the whole
    // upload still completes.
    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QCOMPARE(rig.target.log().loader_port_used, std::uint16_t{1615});
}

void LoaderModelTest::auto_find_runs_before_an_upload_when_no_target_is_selected() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setAutoFindBeforeOperation(true);
    adl::test::UploadFixture fixture;
    adl::test::SimPlan plan = fixture.plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::test::Rig rig(plan);
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig), std::move(settings));
    read_the_media_set(model, fixture);
    model.selectAllLoads(true);

    // No target chosen: the upload may still be started, and the FIND runs first.
    QVERIFY(!model.targetValid());
    QVERIFY(model.canUpload());
    model.uploadSelected();
    QVERIFY(model.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // Exactly one target answered: it was taken, and the upload went ahead on its own.
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(model.thwIdPos(), QStringLiteral("HNPFMS_L"));
    QCOMPARE(model.selectedTargetIndex(), 0);
    QVERIFY(log_contains(model, QStringLiteral("No target is selected")));
    QCOMPARE(rig.target.log().files_read.size(), fixture.load.item.reads.size());
}

void LoaderModelTest::auto_find_with_several_targets_starts_nothing() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setAutoFindBeforeOperation(true);
    adl::test::UploadFixture fixture;
    adl::test::SimPlan plan = fixture.plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::test::Rig rig(plan);
    // A second target on the same (fake) network, with a different answer: the FIND returns two
    // distinct targets.
    adl::test::SimPlan second_plan;
    second_plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMC", "FMC", "R", "FMC RIGHT", "HNP"};
    adl::test::SimTarget second_target(rig.world, rig.network,
                                       adl::platform::Ipv4Address{{10, 0, 0, 3}}, second_plan);
    QVERIFY(fixture.write(rig));
    LoaderModel model(rig_host(rig), std::move(settings));
    read_the_media_set(model, fixture);
    model.selectAllLoads(true);

    // Several targets answered: the FIND result is shown, nothing else starts.
    model.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("2 targets answered"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("Choose the target")));
    QVERIFY(!model.targetValid());
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});

    // No target at all: the same.
    QTemporaryDir settings_dir2;
    auto settings2 = settings_in(settings_dir2);
    settings2->setAutoFindBeforeOperation(true);
    adl::test::UploadFixture fixture2;
    adl::test::Rig rig2(fixture2.plan());
    QVERIFY(fixture2.write(rig2));
    LoaderModel model2(rig_host(rig2), std::move(settings2));
    read_the_media_set(model2, fixture2);
    model2.selectAllLoads(true);
    model2.uploadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model2.busy(), 20000);
    QCOMPARE(model2.resultState(), 1);
    QCOMPARE(model2.resultText(), QStringLiteral("0 targets answered"));
    QVERIFY(!model2.targetValid());
    QCOMPARE(rig2.target.log().init_requests_at.size(), std::size_t{0});
}

void LoaderModelTest::the_last_media_set_is_restored_after_startup() {
    QTemporaryDir settings_dir;
    adl::test::UploadFixture fixture;
    adl::test::Rig rig(fixture.plan());
    QVERIFY(fixture.write(rig));
    auto settings = settings_in(settings_dir);
    settings->setLastMediaSetDirectory(
        QString::fromStdU16String(fixture.directory.path().u16string()));

    // The window's event loop would deliver the timer; the test's does. The set is read (and
    // checked) without being asked for.
    LoaderModel model(rig_host(rig), std::move(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!model.mediaSetPath().isEmpty(), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QVERIFY(model.mediaSetError().isEmpty());
    QCOMPARE(model.validationState(), 2);
    QCOMPARE(qobject_cast<LoadModel*>(model.loads())->rowCount(), 1);
}

void LoaderModelTest::a_missing_last_media_set_is_skipped_quietly() {
    QTemporaryDir settings_dir;
    adl::test::UploadFixture fixture;  // only for its directory
    adl::test::Rig rig;
    auto settings = settings_in(settings_dir);
    settings->setLastMediaSetDirectory(
        QString::fromStdU16String((fixture.directory.path() / "missing").u16string()));

    LoaderModel model(rig_host(rig), std::move(settings));
    QTest::qWait(100);
    QVERIFY(!model.busy());
    QVERIFY(model.mediaSetPath().isEmpty());
    QCOMPARE(model.resultState(), 0);  // no job ran, no dialog, nothing
    QVERIFY(model.lastErrorText().isEmpty());
}

void LoaderModelTest::the_log_capacity_preference_is_applied() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setLogCapacity(150);
    adl::test::Rig rig;
    LoaderModel model(rig_host(rig), std::move(settings));
    auto* events = qobject_cast<EventModel*>(model.events());
    QCOMPARE(events->capacity(), 150);

    model.setLogCapacity(200);
    QCOMPARE(events->capacity(), 200);
    QCOMPARE(model.logCapacity(), 200);

    // And the new length is stored for the next session.
    const auto read_back = settings_in(settings_dir);
    QCOMPARE(read_back->logCapacity(), 200);
}

void LoaderModelTest::a_media_defined_download_writes_the_files_into_the_numbered_directory() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    // The download directory itself is a directory that does not exist yet: it is made on
    // demand.
    model.setDownloadDirectory(
        QString::fromStdU16String((downloads.path() / "not there yet").u16string()));
    point_at_the_target(model);
    QVERIFY(!model.canDownload());  // no file named
    model.addDownloadFile(QStringLiteral("A.BIN"));
    model.addDownloadFile(QStringLiteral("C.BIN"));
    model.addDownloadFile(QStringLiteral("A.BIN"));  // there already: refused
    model.addDownloadFile(QStringLiteral("   "));    // empty: refused
    QCOMPARE(qobject_cast<DownloadModel*>(model.downloads())->rowCount(), 2);
    QVERIFY(model.canDownload());

    std::vector<int> progress;
    QObject::connect(&model, &LoaderModel::progressChanged, &model,
                     [&] { progress.push_back(model.progress()); });
    model.downloadSelected();
    QVERIFY(model.busy());
    QVERIFY(model.canAbort());
    QVERIFY(!model.canDownload());
    QVERIFY(model.activity().contains(QStringLiteral("Downloading")));
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QVERIFY2(model.resultDetail().contains(QStringLiteral("Files received: 2 of 2 requested.")),
             qPrintable(model.resultDetail()));

    // The files landed in DNLD_DATA_HNPFMS_L_1, under the directory that was made for them, and
    // nowhere else; the target was asked for exactly the two names.
    QVERIFY2(holds(rig, downloads.path() / "not there yet", "A.BIN", plan.files[0].content),
             "A.BIN is not in the download directory");
    QVERIFY2(holds(rig, downloads.path() / "not there yet", "C.BIN", plan.files[2].content),
             "C.BIN is not in the download directory");
    QVERIFY(!downloaded(rig, downloads.path() / "not there yet", "B.BIN").has_value());
    QCOMPARE(rig.target.log().requested_files, (std::vector<std::string>{"A.BIN", "C.BIN"}));
    QCOMPARE(rig.target.log().files_written, (std::vector<std::string>{"A.BIN", "C.BIN"}));

    // The directory is exposed for the window, and the per-file progress rows made it through
    // the model, both files received.
    QVERIFY2(model.lastDownloadDirectory().contains(QStringLiteral("DNLD_DATA_HNPFMS_L_1")),
             qPrintable(model.lastDownloadDirectory()));
    const auto* files = qobject_cast<DownloadModel*>(model.downloads());
    QCOMPARE(files->rowCount(), 2);
    QCOMPARE(files->data(files->index(0), DownloadModel::RatioRole).toInt(), 100);
    QCOMPARE(files->data(files->index(1), DownloadModel::RatioRole).toInt(), 100);
    QCOMPARE(files->data(files->index(0), DownloadModel::FileStatusRole).toString(),
             QStringLiteral("in progress"));
    QVERIFY(!files->data(files->index(0), DownloadModel::FailedRole).toBool());
    QVERIFY(!progress.empty());
    QCOMPARE(progress.back(), 100);
    QVERIFY(std::is_sorted(progress.begin(), progress.end()));

    QVERIFY(log_contains(model, QStringLiteral("Downloading 2 files from 10.0.0.2 (HNPFMS_L)")));
    QVERIFY(log_contains(model, QStringLiteral("Received A.BIN")));
    QVERIFY(log_contains(model, QStringLiteral("% of the list")));
    QVERIFY(log_contains(model, QStringLiteral("Finished: completed")));
}

void LoaderModelTest::each_download_gets_the_next_numbered_directory() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.addDownloadFile(QStringLiteral("A.BIN"));

    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content, 1));

    // The next download does not reuse DNLD_DATA_HNPFMS_L_1: the number moves on, and the first
    // directory keeps its file.
    rig.target.reset_operation();
    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content, 1));
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content, 2));
    QVERIFY(!downloaded(rig, downloads.path(), "A.BIN", 3).has_value());
    QVERIFY(model.lastDownloadDirectory().contains(QStringLiteral("DNLD_DATA_HNPFMS_L_2")));
}

void LoaderModelTest::an_operator_defined_download_takes_the_choice_through_the_gate() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.setDownloadMode(1);
    QVERIFY(model.canDownload());  // a target and a directory are all it needs

    QStringList listed;
    QObject::connect(&model, &LoaderModel::downloadChoicePendingChanged, &model, [&] {
        if (!model.downloadChoicePending()) {
            return;
        }
        auto* files = qobject_cast<DownloadModel*>(model.downloads());
        listed = files->names();
        // Take B.BIN and C.BIN only: the choice is what the gate hands to the loader.
        QVERIFY(files->setData(files->index(0), false, DownloadModel::CheckedRole));
        model.confirmDownloadSelection();
        // The choice is made once: the window is not asking again, and the ticks are locked
        // for the rest of the operation.
        QVERIFY(!model.downloadChoicePending());
        QVERIFY(!files->setData(files->index(0), false, DownloadModel::CheckedRole));
    });
    model.downloadSelected();
    QVERIFY(model.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("Files received: 2 of 2 requested.")));
    // The target's list reached the model, all three files, with the description the target gave.
    QCOMPARE(listed, QStringList({QStringLiteral("A.BIN"), QStringLiteral("B.BIN"),
                                  QStringLiteral("C.BIN")}));
    // Only the chosen files were sent back (.LNA) and written; the choice went through the gate.
    QCOMPARE(rig.target.log().selected_files, (std::vector<std::string>{"B.BIN", "C.BIN"}));
    QCOMPARE(rig.target.log().files_written, (std::vector<std::string>{"B.BIN", "C.BIN"}));
    QVERIFY2(holds(rig, downloads.path(), "B.BIN", plan.files[1].content),
             "B.BIN is not in the download directory");
    QVERIFY2(holds(rig, downloads.path(), "C.BIN", plan.files[2].content),
             "C.BIN is not in the download directory");
    QVERIFY(!downloaded(rig, downloads.path(), "A.BIN").has_value());
    // The window keeps the chosen files, with their progress.
    QCOMPARE(qobject_cast<DownloadModel*>(model.downloads())->rowCount(), 2);
    QVERIFY(log_contains(model, QStringLiteral("The target listed 3 files it can download.")));
}

void LoaderModelTest::choosing_none_of_the_targets_files_cancels_the_download() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.setDownloadMode(1);

    QObject::connect(&model, &LoaderModel::downloadChoicePendingChanged, &model, [&] {
        if (model.downloadChoicePending()) {
            model.chooseNoDownloadFiles();
        }
    });
    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // "None" is what a listener answering SelectionDecision::cancelled would do: the operation
    // is aborted, and the target was told.
    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("aborted by the operator"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("Files received: 0 of 0 requested.")));
    QVERIFY(rig.target.log().selected_files.empty());
    QCOMPARE(rig.target.log().aborts_seen.size(), std::size_t{1});
    QVERIFY(log_contains(model, QStringLiteral("Finished: aborted by the operator")));

    // The cancellation of one job is not carried into the next: a media defined download runs
    // to its end. (The cancelled download made DNLD_DATA_HNPFMS_L_1 when it started, so the
    // next one is numbered 2: every download reserves its directory.)
    rig.target.reset_operation();
    model.setDownloadMode(0);
    model.addDownloadFile(QStringLiteral("A.BIN"));
    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content, 2));
}

void LoaderModelTest::a_download_with_a_missing_file_ends_as_a_failure() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.addDownloadFile(QStringLiteral("A.BIN"));
    model.addDownloadFile(QStringLiteral("MISSING.BIN"));

    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // The target completed, but a file the operator asked for never arrived: the application
    // reports the download as failed, with the numbers and a hint.
    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("completed, 1 file missing"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("Files received: 1 of 2 requested.")));
    QCOMPARE(model.lastErrorText(), QStringLiteral("completed, 1 file missing"));
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("did not give back every file")));
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content));
    QVERIFY(log_contains(model, QStringLiteral("requested files missing")));
}

void LoaderModelTest::abort_of_a_download_stops_it() {
    const adl::test::SimPlan plan = download_plan();
    adl::test::Rig rig(plan);
    QSemaphore hold;
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig, &hold));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.addDownloadFile(QStringLiteral("A.BIN"));

    model.downloadSelected();
    QVERIFY(model.canAbort());
    model.abort();
    QVERIFY(!model.canAbort());
    QVERIFY(model.activity().contains(QStringLiteral("Stopping")));
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultState(), 2);
    QCOMPARE(model.resultText(), QStringLiteral("aborted by the operator"));
    QVERIFY(model.resultDetail().contains(QStringLiteral("Files received: 0 of 1 requested.")));
    QVERIFY(log_contains(model, QStringLiteral("Finished: aborted by the operator")));

    // The next download is not aborted by the last one's request.
    rig.target.reset_operation();
    model.downloadSelected();
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content, 2));
}

void LoaderModelTest::a_download_needs_a_target_a_directory_and_files() {
    adl::test::Rig rig(download_plan());
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));

    // Nothing set up: refused, and asking for it changes nothing.
    QVERIFY(!model.canDownload());
    model.downloadSelected();
    QVERIFY(!model.busy());
    QCOMPARE(model.resultState(), 0);

    point_at_the_target(model);
    QVERIFY(!model.canDownload());  // no file named, and the directory was cleared
    model.setDownloadDirectory(QStringLiteral("   "));
    model.addDownloadFile(QStringLiteral("A.BIN"));
    QVERIFY(!model.canDownload());  // no directory
    model.setDownloadDirectory(download_dir_of(downloads));
    QVERIFY(model.canDownload());

    // The operator defined mode needs no names: a target and a directory are all it takes.
    model.setDownloadMode(1);
    QVERIFY(model.canDownload());
    // Switching back starts the media defined list afresh; the name is added again and the
    // target is what the download still needs.
    model.setDownloadMode(0);
    QVERIFY(!model.canDownload());
    model.addDownloadFile(QStringLiteral("A.BIN"));
    QVERIFY(model.canDownload());
    model.setTargetAddress(QStringLiteral("10.0.0"));
    QVERIFY(!model.canDownload());  // not an address
    model.setTargetAddress(QStringLiteral("10.0.0.2"));
    model.removeDownloadFile(0);
    QVERIFY(!model.canDownload());  // the name was removed

    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});
}

void LoaderModelTest::a_download_refuses_a_name_that_is_not_a_safe_file_name() {
    adl::test::Rig rig(download_plan());
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.addDownloadFile(QStringLiteral("A.BIN"));
    model.addDownloadFile(QStringLiteral("../UP.BIN"));

    model.downloadSelected();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // Refused before the operation started: the target never saw a request.
    QCOMPARE(model.resultState(), 2);
    QVERIFY(model.resultText().contains(QStringLiteral("not a safe file name")));
    QVERIFY(model.lastErrorHint().contains(QStringLiteral("without a path")));
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});
}

void LoaderModelTest::a_second_job_is_refused_while_a_download_runs() {
    adl::test::Rig rig(download_plan());
    QSemaphore hold;
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig, &hold));
    model.setDownloadDirectory(download_dir_of(downloads));
    point_at_the_target(model);
    model.addDownloadFile(QStringLiteral("A.BIN"));

    model.downloadSelected();
    QVERIFY(model.busy());
    model.findTargets();
    model.readInformation();
    model.uploadSelected();
    model.downloadSelected();
    model.addDownloadFile(QStringLiteral("B.BIN"));
    QCOMPARE(qobject_cast<DownloadModel*>(model.downloads())->rowCount(), 1);
    hold.release();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QVERIFY(!log_contains(model, QStringLiteral("Looking for")));
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{1});
}

void LoaderModelTest::auto_find_runs_before_a_download_when_no_target_is_selected() {
    QTemporaryDir settings_dir;
    auto settings = settings_in(settings_dir);
    settings->setAutoFindBeforeOperation(true);
    adl::test::SimPlan plan = download_plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::test::Rig rig(plan);
    adl::test::TempDir downloads;
    LoaderModel model(rig_host(rig), std::move(settings));
    model.setDownloadDirectory(download_dir_of(downloads));
    model.addDownloadFile(QStringLiteral("A.BIN"));

    // No target chosen: the download may still be started, and the FIND runs first.
    QVERIFY(!model.targetValid());
    QVERIFY(model.canDownload());
    model.downloadSelected();
    QVERIFY(model.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 20000);

    // Exactly one target answered: it was taken, and the download went ahead on its own.
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.resultText(), QStringLiteral("completed"));
    QCOMPARE(model.targetAddress(), QStringLiteral("10.0.0.2"));
    QVERIFY(log_contains(model, QStringLiteral("No target is selected")));
    QVERIFY(holds(rig, downloads.path(), "A.BIN", plan.files[0].content));
}
