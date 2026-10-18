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
    for (int i = 0; i < EventModel::capacity + 5; ++i) {
        model.append({Severity::notice, QString::number(i)});
    }
    QCOMPARE(model.rowCount(), EventModel::capacity);
    QCOMPARE(model.data(model.index(0), EventModel::TextRole).toString(), QStringLiteral("5"));
    QCOMPARE(model.data(model.index(EventModel::capacity - 1), EventModel::TextRole).toString(),
             QString::number(EventModel::capacity + 4));
    QCOMPARE(model.data(model.index(0), EventModel::SeverityRole).toInt(),
             static_cast<int>(Severity::notice));
    QVERIFY(model.data(model.index(0), EventModel::TimeRole).toString().size() == 8);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void ModelsTest::configuration_model_replaces_its_rows() {
    ConfigurationModel model;
    model.reset(
        {{HardwareRow::Level::hardware, QStringLiteral("FMS LEFT"), QStringLiteral("serial")},
         {HardwareRow::Level::part, QStringLiteral("ACM47"), QStringLiteral("FMS 1")}});
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
    row.status = QStringLiteral("in progress");
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
    row.status = QStringLiteral("failed");
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
