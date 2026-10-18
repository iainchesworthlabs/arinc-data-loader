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
