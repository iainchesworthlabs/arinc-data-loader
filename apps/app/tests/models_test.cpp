#include <QSignalSpy>
#include <QTest>

#include "adl/app/models.hpp"
#include "test_classes.hpp"

using namespace adl::app;

void ModelsTest::target_model_exposes_what_find_found() {
    TargetModel model;
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.at(0) == nullptr);

    FoundTarget target;
    target.address = QStringLiteral("10.0.0.2");
    target.hardware_id = QStringLiteral("HNPFMS");
    target.position = QStringLiteral("L");
    target.type_name = QStringLiteral("FMS");
    target.literal_name = QStringLiteral("FMS LEFT");
    target.manufacturer_code = QStringLiteral("HNP");
    model.reset({target});

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex row = model.index(0);
    QCOMPARE(model.data(row, TargetModel::AddressRole).toString(), QStringLiteral("10.0.0.2"));
    QCOMPARE(model.data(row, TargetModel::ThwIdPosRole).toString(), QStringLiteral("HNPFMS_L"));
    QCOMPARE(model.data(row, TargetModel::LiteralNameRole).toString(), QStringLiteral("FMS LEFT"));
    QCOMPARE(model.data(row, Qt::DisplayRole).toString(), QStringLiteral("FMS LEFT at 10.0.0.2"));
    QVERIFY(!model.data(model.index(5), TargetModel::AddressRole).isValid());
    QVERIFY(model.roleNames().values().contains("thwIdPos"));

    // A target with no literal name is shown by its identifier.
    target.literal_name.clear();
    model.reset({target});
    QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
             QStringLiteral("HNPFMS_L at 10.0.0.2"));
}

void ModelsTest::event_model_keeps_the_newest_lines() {
    EventModel model;
    QCOMPARE(model.capacity(), EventModel::default_capacity);
    for (int i = 0; i < model.capacity() + 5; ++i) {
        model.append({Severity::notice, Message::verbatim(QString::number(i))});
    }
    QCOMPARE(model.rowCount(), model.capacity());
    QCOMPARE(model.data(model.index(0), EventModel::TextRole).toString(), QStringLiteral("5"));
    QCOMPARE(model.data(model.index(model.capacity() - 1), EventModel::TextRole).toString(),
             QString::number(model.capacity() + 4));
    QCOMPARE(model.data(model.index(0), EventModel::SeverityRole).toInt(),
             static_cast<int>(Severity::notice));
    QVERIFY(model.data(model.index(0), EventModel::TimeRole).toString().size() == 8);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void ModelsTest::event_model_capacity_is_per_instance() {
    EventModel model;
    model.set_capacity(4);
    QCOMPARE(model.capacity(), 4);
    for (int i = 0; i < 6; ++i) {
        model.append({Severity::info, Message::verbatim(QString::number(i))});
    }
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(0), EventModel::TextRole).toString(), QStringLiteral("2"));
    QCOMPARE(model.data(model.index(3), EventModel::TextRole).toString(), QStringLiteral("5"));

    // Another instance keeps the default; a capacity below one is refused.
    EventModel other;
    QCOMPARE(other.capacity(), EventModel::default_capacity);
    model.set_capacity(0);
    QCOMPARE(model.capacity(), 1);
}

void ModelsTest::event_model_roles_are_the_ones_the_log_view_reads() {
    // The shared LogView (libs/ui/qml/LogView.qml) reads the severity, the text and the time by
    // number: 257, 258 and 259. The mock target's ActivityModel has to number its roles the same
    // way (apps/mock_target/tests/model_test.cpp).
    QCOMPARE(static_cast<int>(EventModel::SeverityRole), 257);
    QCOMPARE(static_cast<int>(EventModel::TextRole), 258);
    QCOMPARE(static_cast<int>(EventModel::TimeRole), 259);
    EventModel model;
    QCOMPARE(model.roleNames().value(257), QByteArrayLiteral("severity"));
    QCOMPARE(model.roleNames().value(258), QByteArrayLiteral("message"));
    QCOMPARE(model.roleNames().value(259), QByteArrayLiteral("time"));
}

void ModelsTest::configuration_model_replaces_its_rows() {
    ConfigurationModel model;
    model.reset({{HardwareRow::Level::hardware, Message::verbatim(QStringLiteral("FMS LEFT")),
                  Message::verbatim(QStringLiteral("serial"))},
                 {HardwareRow::Level::part, Message::verbatim(QStringLiteral("ACM47")),
                  Message::verbatim(QStringLiteral("FMS 1"))}});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), ConfigurationModel::LevelRole).toInt(), 0);
    QCOMPARE(model.data(model.index(1), ConfigurationModel::LevelRole).toInt(), 1);
    QCOMPARE(model.data(model.index(1), ConfigurationModel::DetailRole).toString(),
             QStringLiteral("FMS 1"));
    model.reset({});
    QCOMPARE(model.rowCount(), 0);
}

void ModelsTest::load_model_describes_a_load() {
    LoadModel model;
    LoadSummary load;
    load.part_number = QStringLiteral("ACM47-1234-5678");
    load.header_file = QStringLiteral("APP.LUH");
    load.data_files = 2;
    load.support_files = 1;
    load.target_hardware_ids = {QStringLiteral("HNPFMS"), QStringLiteral("HNPFMC")};
    model.reset({load});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), LoadModel::PartNumberRole).toString(),
             QStringLiteral("ACM47-1234-5678"));
    const QString detail = model.data(model.index(0), LoadModel::DetailRole).toString();
    QVERIFY2(detail.contains(QStringLiteral("APP.LUH")), qPrintable(detail));
    QVERIFY2(detail.contains(QStringLiteral("HNPFMS, HNPFMC")), qPrintable(detail));
}

namespace {

LoadSummary load_of(const char* part, const char* header) {
    LoadSummary load;
    load.part_number = QString::fromLatin1(part);
    load.header_file = QString::fromLatin1(header);
    return load;
}

}  // namespace

void ModelsTest::load_model_keeps_the_operators_ticks() {
    LoadModel model;
    QSignalSpy selection(&model, &LoadModel::selection_changed);
    model.reset({load_of("ACM47-0001-0001", "A.LUH"), load_of("ACM47-0002-0002", "B.LUH")});
    QCOMPARE(selection.count(), 1);  // a new set has nothing ticked
    QCOMPARE(model.checked_count(), 0);
    QVERIFY(model.checked_loads().isEmpty());
    QVERIFY(!model.data(model.index(0), LoadModel::CheckedRole).toBool());
    QVERIFY(model.flags(model.index(0)).testFlag(Qt::ItemIsEditable));

    QVERIFY(model.setData(model.index(1), true, LoadModel::CheckedRole));
    QCOMPARE(selection.count(), 2);
    QCOMPARE(model.checked_count(), 1);
    QCOMPARE(model.checked_loads().size(), 1);
    QCOMPARE(model.checked_loads().front().header_file, QStringLiteral("B.LUH"));
    QVERIFY(model.setData(model.index(1), true, LoadModel::CheckedRole));  // no change, no signal
    QCOMPARE(selection.count(), 2);
    QVERIFY(!model.setData(model.index(1), 5, LoadModel::PartNumberRole));  // only the tick
    QVERIFY(!model.setData(model.index(7), true, LoadModel::CheckedRole));

    model.check_all(true);
    QCOMPARE(model.checked_count(), 2);
    QCOMPARE(selection.count(), 3);
    model.check_all(true);  // already so
    QCOMPARE(selection.count(), 3);

    // While a job runs the ticks stay as they are.
    model.set_locked(true);
    QVERIFY(!model.flags(model.index(0)).testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.setData(model.index(0), false, LoadModel::CheckedRole));
    model.check_all(false);
    QCOMPARE(model.checked_count(), 2);
    model.set_locked(false);
    model.check_all(false);
    QCOMPARE(model.checked_count(), 0);

    model.check_all(true);
    model.reset({load_of("ACM47-0003-0003", "C.LUH")});
    QCOMPARE(model.checked_count(), 0);
}

void ModelsTest::load_model_keeps_ticks_for_loads_that_are_still_there() {
    LoadModel model;
    model.reset({load_of("ACM47-0001-0001", "A.LUH"), load_of("ACM47-0002-0002", "B.LUH")});
    QVERIFY(model.setData(model.index(1), true, LoadModel::CheckedRole));

    // The same set read again (or re-checked) keeps the tick on the load that is still there,
    // whatever the order; a load that is gone is not ticked, and neither is a new one.
    model.reset({load_of("ACM47-0002-0002", "B.LUH"), load_of("ACM47-0003-0003", "C.LUH")});
    QCOMPARE(model.checked_count(), 1);
    QVERIFY(model.data(model.index(0), LoadModel::CheckedRole).toBool());   // B.LUH, kept
    QVERIFY(!model.data(model.index(1), LoadModel::CheckedRole).toBool());  // C.LUH, new
    QVERIFY(!model.checked_loads().isEmpty());
    QCOMPARE(model.checked_loads().front().header_file, QStringLiteral("B.LUH"));
}

void ModelsTest::file_model_lists_the_files_of_the_set() {
    FileModel model;
    QCOMPARE(model.rowCount(), 0);
    MediaFileSummary file;
    file.name = QStringLiteral("DATA.LUP");
    file.pathname = QStringLiteral("\\disk001\\");
    file.member = 2;
    file.crc16 = QStringLiteral("1A2B");
    model.reset({file});
    QCOMPARE(model.rowCount(), 1);
    const QModelIndex row = model.index(0);
    QCOMPARE(model.data(row, FileModel::NameRole).toString(), QStringLiteral("DATA.LUP"));
    QCOMPARE(model.data(row, FileModel::PathnameRole).toString(), QStringLiteral("\\disk001\\"));
    QCOMPARE(model.data(row, FileModel::MemberRole).toInt(), 2);
    QCOMPARE(model.data(row, FileModel::CrcRole).toString(), QStringLiteral("1A2B"));
    QCOMPARE(model.data(row, Qt::DisplayRole).toString(), QStringLiteral("DATA.LUP"));
    QVERIFY(model.data(model.index(4), FileModel::NameRole).isNull());
    QVERIFY(model.roleNames().values().contains("crc"));
    model.reset({});
    QCOMPARE(model.rowCount(), 0);
}

void ModelsTest::load_model_shows_the_progress_of_the_ticked_loads() {
    LoadModel model;
    model.reset({load_of("ACM47-0001-0001", "A.LUH"), load_of("ACM47-0002-0002", "B.LUH")});
    QCOMPARE(model.data(model.index(0), LoadModel::RatioRole).toInt(), -1);

    QVERIFY(model.setData(model.index(1), true, LoadModel::CheckedRole));
    model.begin_upload();
    QCOMPARE(model.data(model.index(0), LoadModel::RatioRole).toInt(), -1);  // not in the upload
    QCOMPARE(model.data(model.index(1), LoadModel::RatioRole).toInt(), 0);
    QCOMPARE(model.data(model.index(1), LoadModel::LoadStatusRole).toString(),
             QStringLiteral("waiting"));

    QSignalSpy changed(&model, &LoadModel::dataChanged);
    LoadProgressRow row;
    row.header_file = QStringLiteral("B.LUH");
    row.part_number = QStringLiteral("ACM47-0002-0002");
    row.ratio = 40;
    row.status = Message::verbatim(QStringLiteral("in progress"));
    LoadProgressRow stranger = row;  // a load the model does not have
    stranger.header_file = QStringLiteral("Z.LUH");
    model.apply_progress({stranger, row});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(model.data(model.index(1), LoadModel::RatioRole).toInt(), 40);
    QCOMPARE(model.data(model.index(1), LoadModel::LoadStatusRole).toString(),
             QStringLiteral("in progress"));
    QVERIFY(!model.data(model.index(1), LoadModel::FailedRole).toBool());
    QCOMPARE(model.data(model.index(0), LoadModel::RatioRole).toInt(), -1);

    row.ratio = 10;
    row.status = Message::verbatim(QStringLiteral("failed"));
    row.description = QStringLiteral("no room");
    row.failed = true;
    model.apply_progress({row});
    QVERIFY(model.data(model.index(1), LoadModel::FailedRole).toBool());
    QCOMPARE(model.data(model.index(1), LoadModel::ProblemRole).toString(),
             QStringLiteral("no room"));

    // The next upload starts from nothing.
    model.begin_upload();
    QCOMPARE(model.data(model.index(1), LoadModel::RatioRole).toInt(), 0);
    QVERIFY(!model.data(model.index(1), LoadModel::FailedRole).toBool());
    QVERIFY(model.data(model.index(1), LoadModel::ProblemRole).toString().isEmpty());
}

void ModelsTest::download_model_holds_the_operators_names() {
    DownloadModel model;
    QSignalSpy selection(&model, &DownloadModel::selection_changed);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.names().isEmpty());

    QVERIFY(model.add_name(QStringLiteral("A.BIN")));
    QVERIFY(model.add_name(QStringLiteral("  B.BIN  ")));  // trimmed
    QVERIFY(!model.add_name(QStringLiteral("A.BIN")));     // there already
    QVERIFY(!model.add_name(QStringLiteral("   ")));       // empty
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.names(), QStringList({QStringLiteral("A.BIN"), QStringLiteral("B.BIN")}));
    QCOMPARE(selection.count(), 2);
    QCOMPARE(model.data(model.index(0), DownloadModel::NameRole).toString(),
             QStringLiteral("A.BIN"));
    QCOMPARE(model.data(model.index(0), DownloadModel::RatioRole).toInt(), -1);
    // The operator's own names are not a choice: the ticks are not writable.
    QVERIFY(!model.setData(model.index(0), true, DownloadModel::CheckedRole));

    model.remove_row(0);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.names(), QStringList{QStringLiteral("B.BIN")});
    model.remove_row(7);  // no such row
    QCOMPARE(model.rowCount(), 1);

    model.reset_media({QStringLiteral("C.BIN")});
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.names(), QStringList{QStringLiteral("C.BIN")});
}

void ModelsTest::download_model_ticks_the_targets_list_and_keeps_the_choice() {
    DownloadModel model;
    model.reset_operator(
        {{QStringLiteral("A.BIN"), QStringLiteral("first")}, {QStringLiteral("B.BIN"), QString()}});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.checked_count(), 2);  // all ticked to start with
    QCOMPARE(model.data(model.index(0), DownloadModel::DescriptionRole).toString(),
             QStringLiteral("first"));
    QVERIFY(model.data(model.index(0), DownloadModel::CheckedRole).toBool());
    QVERIFY(model.flags(model.index(0)).testFlag(Qt::ItemIsEditable));

    // The operator untickks one file: the choice is the rest.
    QVERIFY(model.setData(model.index(0), false, DownloadModel::CheckedRole));
    QCOMPARE(model.checked_count(), 1);
    QCOMPARE(model.checked_names(), QStringList{QStringLiteral("B.BIN")});
    model.check_all(true);
    QCOMPARE(model.checked_count(), 2);
    QVERIFY(model.setData(model.index(1), false, DownloadModel::CheckedRole));
    QCOMPARE(model.checked_names(), QStringList{QStringLiteral("A.BIN")});

    // The download starts with the choice made: the unchosen row is dropped, the chosen one
    // waits, and the ticks are locked.
    model.begin_download();
    model.set_locked(true);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), DownloadModel::NameRole).toString(),
             QStringLiteral("A.BIN"));
    QCOMPARE(model.data(model.index(0), DownloadModel::RatioRole).toInt(), 0);
    QCOMPARE(model.data(model.index(0), DownloadModel::FileStatusRole).toString(),
             QStringLiteral("waiting"));
    QVERIFY(!model.setData(model.index(0), false, DownloadModel::CheckedRole));

    // A status file updates the row by name; a stranger is ignored.
    FileProgressRow row;
    row.name = QStringLiteral("A.BIN");
    row.ratio = 100;
    row.status = Message::verbatim(QStringLiteral("in progress"));
    FileProgressRow stranger = row;
    stranger.name = QStringLiteral("Z.BIN");
    model.apply_progress({stranger, row});
    QCOMPARE(model.data(model.index(0), DownloadModel::RatioRole).toInt(), 100);
    QCOMPARE(model.data(model.index(0), DownloadModel::FileStatusRole).toString(),
             QStringLiteral("in progress"));
    QVERIFY(!model.data(model.index(0), DownloadModel::FailedRole).toBool());

    row.ratio = 0;
    row.status = Message::verbatim(QStringLiteral("failed"));
    row.description = QStringLiteral("no room");
    row.failed = true;
    model.apply_progress({row});
    QVERIFY(model.data(model.index(0), DownloadModel::FailedRole).toBool());
    QCOMPARE(model.data(model.index(0), DownloadModel::ProblemRole).toString(),
             QStringLiteral("no room"));
}
