// The worker's jobs called directly, on this thread: what the window never asks for (an id no
// directory can be named after), and the ways an operation can fail to run at all.

#include <string>

#include <QSignalSpy>
#include <QTest>

#include "adl/app/engine_worker.hpp"
#include "bytes.hpp"
#include "faulty_host.hpp"
#include "rig.hpp"
#include "test_classes.hpp"

using namespace adl::app;

namespace {

// The target the rig's simulated one answers as.
const QString target_address = QStringLiteral("10.0.0.2");
const QString downloads = QStringLiteral("/downloads");

HostFactory host_of(adl::platform::Host& host) {
    return [&host]() -> std::expected<HostLease, QString> {
        HostLease lease;
        lease.host = &host;
        return lease;
    };
}

// What the job reported when it ended (every job ends with exactly one `finished`).
OperationSummary finished_with(QSignalSpy& spy) {
    if (spy.count() != 1) {
        return {};
    }
    return qvariant_cast<OperationSummary>(spy.at(0).at(0));
}

// The entries of the download directory, as the fake file system holds them.
QStringList entries_of(adl::fake::MemoryFileSystem& files, std::string_view directory) {
    QStringList names;
    const auto listed = adl::platform::for_each_entry(
        files, directory, [&](std::string_view name, adl::platform::FileKind) {
            names.append(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
            return true;
        });
    if (!listed) {
        names.append(QStringLiteral("<no such directory>"));
    }
    return names;
}

adl::test::SimPlan denying_plan() {
    adl::test::SimPlan plan;
    plan.init_status = adl::arinc615a::StatusCode::operation_denied;
    plan.init_text = "not now";
    return plan;
}

}  // namespace

void EngineWorkerTest::an_id_no_directory_can_be_named_after_makes_no_directory() {
    adl::test::Rig rig;
    EngineWorker worker(host_of(rig.loader_host));
    for (const char* id : {"", "a/b", "a:b", "a|b", "..\\x", "NUL\t"}) {
        QSignalSpy finished(&worker, &EngineWorker::finished);
        worker.download_operator(target_address, QString::fromLatin1(id), downloads, 59);
        const OperationSummary media = finished_with(finished);
        QVERIFY2(!media.success && media.outcome.text().contains(QStringLiteral("THW_ID_POS")),
                 qPrintable(media.outcome.text()));
        QVERIFY(!media.hint.text().isEmpty());

        QSignalSpy finished_again(&worker, &EngineWorker::finished);
        worker.download_media(target_address, QString::fromLatin1(id),
                              QStringList{QStringLiteral("A.BIN")}, downloads, 59);
        const OperationSummary operator_defined = finished_with(finished_again);
        QVERIFY(!operator_defined.success);
        QVERIFY(operator_defined.outcome.text().contains(QStringLiteral("THW_ID_POS")));
    }
    // Not even the download directory was made, and the target was never asked.
    QVERIFY(!rig.loader_host.fake_files().stat("/downloads").has_value());
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});
}

void EngineWorkerTest::a_download_the_target_denies_leaves_no_directory() {
    adl::test::Rig rig(denying_plan());
    EngineWorker worker(host_of(rig.loader_host));
    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 59);

    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QCOMPARE(summary.outcome.text(), QStringLiteral("denied by the target"));
    // Nothing came, so there is no directory to point at, and none is left behind. The download
    // directory itself was the operator's choice (made on demand) and stays.
    QVERIFY(summary.download_directory.isEmpty());
    QVERIFY2(summary.detail.text().contains(QStringLiteral("removed")),
             qPrintable(summary.detail.text()));
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"), QStringList{});
}

void EngineWorkerTest::a_download_from_a_silent_target_leaves_no_directory() {
    adl::test::SimPlan plan;
    plan.init_silent = true;
    adl::test::Rig rig(plan);
    EngineWorker worker(host_of(rig.loader_host));
    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_media(target_address, QStringLiteral("HNPFMS_L"),
                          QStringList{QStringLiteral("A.BIN")}, downloads, 59);

    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY(summary.download_directory.isEmpty());
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"), QStringList{});
}

void EngineWorkerTest::a_download_that_received_a_file_keeps_its_directory() {
    adl::test::SimPlan plan;
    plan.files = {adl::test::DownloadItem{"A.BIN", adl::test::bytes_of("aaa"), "first"}};
    adl::test::Rig rig(plan);
    EngineWorker worker(host_of(rig.loader_host));
    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_media(target_address, QStringLiteral("HNPFMS_L"),
                          QStringList{QStringLiteral("A.BIN")}, downloads, 59);

    const OperationSummary summary = finished_with(finished);
    QVERIFY2(summary.success, qPrintable(summary.outcome.text()));
    QVERIFY(summary.download_directory.contains(QStringLiteral("DNLD_DATA_HNPFMS_L_1")));
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"),
             QStringList{QStringLiteral("DNLD_DATA_HNPFMS_L_1")});
    QVERIFY(rig.loader_host.fake_files().get("/downloads/DNLD_DATA_HNPFMS_L_1/A.BIN").has_value());
}

void EngineWorkerTest::a_download_stopped_with_a_file_in_the_directory_keeps_it() {
    // The directory is not removed when anything is in it, even a file that is not the target's
    // (another program put it there, or a part file survived).
    adl::test::Rig rig(denying_plan());
    adl::test::FaultyHost host(rig.loader_host);
    auto& memory = rig.loader_host.fake_files();
    host.faulty_files().after_make_directory = [&](std::string_view path) {
        memory.put(std::string{path} + "/stray.txt", "kept");
    };
    EngineWorker worker(host_of(host));
    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 59);

    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY(summary.download_directory.contains(QStringLiteral("DNLD_DATA_HNPFMS_L_1")));
    QVERIFY(memory.get("/downloads/DNLD_DATA_HNPFMS_L_1/stray.txt").has_value());
}

void EngineWorkerTest::a_request_the_loader_refuses_is_reported_and_leaves_no_directory() {
    adl::test::Rig rig;
    EngineWorker worker(host_of(rig.loader_host));

    // A control port of 0 is not one the loader can listen on: DataLoader::start refuses.
    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.read_information(target_address, QStringLiteral("HNPFMS_L"), 0);
    OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY2(summary.outcome.text().contains(QStringLiteral("the request was refused")),
             qPrintable(summary.outcome.text()));

    QSignalSpy finished_download(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 0);
    summary = finished_with(finished_download);
    QVERIFY(!summary.success);
    QVERIFY(summary.outcome.text().contains(QStringLiteral("the request was refused")));
    QVERIFY(summary.download_directory.isEmpty());
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"), QStringList{});
}

void EngineWorkerTest::a_port_that_cannot_be_opened_is_reported_and_leaves_no_directory() {
    adl::test::Rig rig;
    adl::test::FaultyHost host(rig.loader_host);
    host.faulty_network().open_fails = adl::platform::ErrorCode::address_in_use;
    EngineWorker worker(host_of(host));

    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 59);
    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY2(
        summary.outcome.text().contains(QStringLiteral("the loader's port could not be opened")),
        qPrintable(summary.outcome.text()));
    QVERIFY(summary.outcome.text().contains(QStringLiteral("address in use")));
    QVERIFY(summary.hint.text().contains(QStringLiteral("control port")));  // the host hint
    QVERIFY(summary.download_directory.isEmpty());
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"), QStringList{});
}

void EngineWorkerTest::a_network_wait_that_fails_is_reported_and_leaves_no_directory() {
    adl::test::Rig rig;
    adl::test::FaultyHost host(rig.loader_host);
    host.faulty_network().wait_fails = adl::platform::ErrorCode::io_error;
    EngineWorker worker(host_of(host));

    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 59);
    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY2(summary.outcome.text().contains(QStringLiteral("waiting for the network failed")),
             qPrintable(summary.outcome.text()));
    QVERIFY(summary.download_directory.isEmpty());
    QCOMPARE(entries_of(rig.loader_host.fake_files(), "/downloads"), QStringList{});
}

void EngineWorkerTest::a_download_directory_that_cannot_be_made_is_reported() {
    adl::test::Rig rig;
    adl::test::FaultyHost host(rig.loader_host);
    host.faulty_files().make_directory_fails["/downloads/DNLD_DATA_HNPFMS_L_1"] =
        adl::platform::ErrorCode::permission_denied;
    EngineWorker worker(host_of(host));

    QSignalSpy finished(&worker, &EngineWorker::finished);
    worker.download_operator(target_address, QStringLiteral("HNPFMS_L"), downloads, 59);
    const OperationSummary summary = finished_with(finished);
    QVERIFY(!summary.success);
    QVERIFY2(summary.outcome.text().contains(QStringLiteral("could not be used")),
             qPrintable(summary.outcome.text()));
    QVERIFY(summary.outcome.text().contains(QStringLiteral("cannot create")));
    QVERIFY(summary.hint.text().contains(QStringLiteral("download directory")));
    QCOMPARE(rig.target.log().init_requests_at.size(), std::size_t{0});
}
