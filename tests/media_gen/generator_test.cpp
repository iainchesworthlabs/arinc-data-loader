#include "test_classes.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <QString>
#include <QUrl>
#include <QtTest>

#include "adl/app/media_set_summary.hpp"
#include "adl/gen/generator_model.hpp"
#include "adl/gen/source_file_model.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "temp_dir.hpp"

namespace arinc665 = adl::arinc665;
namespace media_tree = adl::media_tree;

namespace {

const QString set_part_number = QStringLiteral("ACM-SET-0001");

// Writes `text` as a file `name` below `directory` and hands back its path.
std::filesystem::path write_file(const std::filesystem::path& directory, std::string_view name,
                                 std::string_view text) {
    const std::filesystem::path path = directory / std::string{name};
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return path;
}

QUrl url_of(const std::filesystem::path& path) {
    return QUrl::fromLocalFile(QString::fromStdU16String(path.u16string()));
}

bool directory_exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

// The form filled in to a known good state, which the tests then vary.
void fill_form(adl::gen::GeneratorModel& model, const std::filesystem::path& output) {
    model.setLoadPartNumber(QStringLiteral("ACM47-1234-5678"));
    model.setMediaSetPartNumber(set_part_number);
    model.setTargetHardwareIds({QStringLiteral("ACME")});
    model.chooseOutputDirectory(url_of(output));
}

// The directory the generation would write for the good form above.
std::filesystem::path expected_root(const std::filesystem::path& output) {
    return output / set_part_number.toStdString();
}

bool has_file(const arinc665::MediaMember& member, std::string_view pathname,
              std::string_view name) {
    return std::any_of(member.files.begin(), member.files.end(),
                       [&](const arinc665::MediaFile& file) {
                           return file.pathname == pathname && file.name == name;
                       });
}

const arinc665::MediaMember* member_of(const std::vector<arinc665::MediaMember>& members,
                                       std::uint16_t sequence_number) {
    const auto found =
        std::find_if(members.begin(), members.end(), [&](const arinc665::MediaMember& member) {
            return member.sequence_number == sequence_number;
        });
    return found == members.end() ? nullptr : &*found;
}

}  // namespace

void GeneratorTest::header_file_name_follows_the_load_part_number() {
    adl::gen::GeneratorModel model;
    QCOMPARE(model.headerFileName(), QStringLiteral(""));  // empty until a part number is typed
    model.setLoadPartNumber(QStringLiteral("ACM47-1234-5678"));
    QCOMPARE(model.headerFileName(), QStringLiteral("ACM.LUH"));
    model.setLoadPartNumber(QStringLiteral("BOE01-0001-0001"));
    QCOMPARE(model.headerFileName(), QStringLiteral("BOE.LUH"));
    // A name the operator typed is kept when the part number changes.
    model.setHeaderFileName(QStringLiteral("CUSTOM.LUH"));
    model.setLoadPartNumber(QStringLiteral("ACM48-1234-5678"));
    QCOMPARE(model.headerFileName(), QStringLiteral("CUSTOM.LUH"));
}

void GeneratorTest::generates_a_verified_media_set() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    const auto support = write_file(sources.path(), "CONFIG.LDR", "CONFIG 12345");

    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles({url_of(data), url_of(support)});
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    QCOMPARE(files->rowCount(), 2);
    QCOMPARE(files->data(files->index(0), adl::gen::SourceFileModel::SizeRole).toString(),
             QStringLiteral("16 B"));
    QVERIFY(files->setData(files->index(1), QStringLiteral("support"),
                           adl::gen::SourceFileModel::KindRole));
    QVERIFY(model.canGenerate());

    model.generate();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 30000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY2(model.validationFindings().isEmpty(),
             qPrintable(model.validationFindings().join(QStringLiteral("; "))));
    QVERIFY(!model.outputPath().isEmpty());

    const std::filesystem::path root = expected_root(output.path());
    QVERIFY(directory_exists(root));
    auto members = media_tree::read_media_set_tree(root);
    QVERIFY(members.has_value());
    auto set = arinc665::read_media_set(*members);
    QVERIFY(set.has_value());
    QVERIFY(set->findings.empty());
    QCOMPARE(set->loads.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(set->loads[0].header_file_name), QStringLiteral("ACM.LUH"));
    QCOMPARE(QString::fromStdString(std::string{set->loads[0].header.load_part_number.view()}),
             QStringLiteral("ACM47-1234-5678"));
    QCOMPARE(
        QString::fromStdString(std::string{set->loads_list.identity.media_set_part_number.view()}),
        set_part_number);
    QCOMPARE(set->loads_list.identity.number_of_media_set_members, std::uint8_t{1});
    // The load the set holds verifies end to end (read_media_set already does, checked twice
    // here because it is the point of the generator).
    QVERIFY(arinc665::verify_load(set->loads[0]).has_value());

    // The data loader reads what the generator made, exactly as an operator hands it over.
    const adl::app::MediaSetSummary summary =
        adl::app::read_media_set_summary(QString::fromStdU16String(root.u16string()));
    QVERIFY2(summary.error.isEmpty(), qPrintable(summary.error));
    QCOMPARE(summary.part_number, set_part_number);
    QCOMPARE(summary.members, 1);
    QCOMPARE(summary.loads.size(), 1);
    QCOMPARE(summary.loads[0].part_number, QStringLiteral("ACM47-1234-5678"));
    QCOMPARE(summary.loads[0].header_file, QStringLiteral("ACM.LUH"));
    QCOMPARE(summary.loads[0].data_files, 1);
    QCOMPARE(summary.loads[0].support_files, 1);
    QVERIFY(summary.findings.isEmpty());

    // The outcome describes the form that produced it: changing the form clears it.
    model.setMemberCount(1);  // no change, so the outcome stays
    QCOMPARE(model.resultState(), 1);
    model.setBatchPartNumber(QStringLiteral("ACM4C-0100-0200"));
    QCOMPARE(model.resultState(), 0);
    QVERIFY(model.resultText().isEmpty());
    QVERIFY(model.outputPath().isEmpty());
}

void GeneratorTest::refuses_an_invalid_form() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorModel model;
    fill_form(model, output.path());

    // An invalid load part number blocks the generation and reports why.
    model.setLoadPartNumber(QStringLiteral("not a part number"));
    QVERIFY(!model.canGenerate());
    QVERIFY(!model.loadPartNumberError().isEmpty());
    model.generate();
    QCOMPARE(model.resultState(), 2);
    QVERIFY(!model.resultText().isEmpty());
    QVERIFY(!directory_exists(expected_root(output.path())));

    // A valid form again, then a file name the naming rules refuse.
    model.setLoadPartNumber(QStringLiteral("ACM47-1234-5678"));
    model.addFiles({url_of(data)});
    QVERIFY(model.canGenerate());
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    QVERIFY(files->setData(files->index(0), QStringLiteral("bad name.bin"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(!model.canGenerate());
    QVERIFY(!model.filesError().isEmpty());
    QVERIFY(
        !files->data(files->index(0), adl::gen::SourceFileModel::MessageRole).toString().isEmpty());
    model.generate();
    QCOMPARE(model.resultState(), 2);
    QVERIFY(!directory_exists(expected_root(output.path())));

    // And no data files at all.
    model.clearFiles();
    QVERIFY(!model.canGenerate());
    QVERIFY(!model.filesError().isEmpty());
    model.generate();
    QCOMPARE(model.resultState(), 2);
    QVERIFY(!directory_exists(expected_root(output.path())));
}

void GeneratorTest::refuses_duplicate_file_names() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    std::filesystem::create_directories(sources.path() / "other");
    write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    write_file(sources.path() / "other", "DATA.LUP", "OTHER OTHER DATA");

    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles(
        {url_of(sources.path() / "DATA.LUP"), url_of(sources.path() / "other" / "DATA.LUP")});
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    QCOMPARE(files->rowCount(), 2);
    QVERIFY(
        !files->data(files->index(1), adl::gen::SourceFileModel::MessageRole).toString().isEmpty());
    QVERIFY(!model.canGenerate());
    model.generate();
    QCOMPARE(model.resultState(), 2);
    QVERIFY(!directory_exists(expected_root(output.path())));

    // A distinct name (different in case only would still clash) makes the form good again.
    QVERIFY(files->setData(files->index(1), QStringLiteral("DATA2.LUP"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(model.canGenerate());
}

void GeneratorTest::places_data_files_over_the_members() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto one = write_file(sources.path(), "ONE.LUP", "ONE ONE ONE ONE!");
    const auto two = write_file(sources.path(), "TWO.LUP", "TWO TWO TWO TWO!");
    const auto three = write_file(sources.path(), "THREE.LUP", "THREE THREE THR!");
    const auto support = write_file(sources.path(), "CONFIG.LDR", "CONFIG 12345");

    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.setMemberCount(2);
    model.addFiles({url_of(one), url_of(two), url_of(three), url_of(support)});
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    QVERIFY(files->setData(files->index(3), QStringLiteral("support"),
                           adl::gen::SourceFileModel::KindRole));
    QVERIFY(model.canGenerate());

    model.generate();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 30000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY(model.validationFindings().isEmpty());

    auto members = media_tree::read_media_set_tree(expected_root(output.path()));
    QVERIFY(members.has_value());
    QCOMPARE(members->size(), std::size_t{2});
    // The load's pathname is the part root directory plus the placement's subdirectory, so a
    // data file in a subdirectory of the part root's name sits two directories down, on
    // whichever member round-robin put it.
    const std::string part_root = "\\ACM4712345678\\";
    const std::string data_directory = "\\ACM4712345678\\ACM4712345678\\";

    // The data files go round-robin, each in a directory of the part root's name: one and three
    // on the first member (below the part root directory, where the header file and the support
    // file stay), two on the second member.
    const arinc665::MediaMember* first = member_of(*members, 1);
    QVERIFY(first != nullptr);
    QVERIFY(has_file(*first, "\\", "LOADS.LUM"));
    QVERIFY(has_file(*first, "\\", "FILES.LUM"));
    QVERIFY(has_file(*first, part_root, "ACM.LUH"));
    QVERIFY(has_file(*first, part_root, "CONFIG.LDR"));
    QVERIFY(has_file(*first, data_directory, "ONE.LUP"));
    QVERIFY(!has_file(*first, data_directory, "TWO.LUP"));
    QVERIFY(has_file(*first, data_directory, "THREE.LUP"));

    const arinc665::MediaMember* second = member_of(*members, 2);
    QVERIFY(second != nullptr);
    QVERIFY(has_file(*second, "\\", "LOADS.LUM"));
    QVERIFY(has_file(*second, "\\", "FILES.LUM"));
    QVERIFY(has_file(*second, data_directory, "TWO.LUP"));
    QVERIFY(!has_file(*second, data_directory, "ONE.LUP"));
    QVERIFY(!has_file(*second, data_directory, "THREE.LUP"));

    auto set = arinc665::read_media_set(*members);
    QVERIFY(set.has_value());
    QVERIFY(set->findings.empty());
    QCOMPARE(set->loads.size(), std::size_t{1});
}

void GeneratorTest::cancel_writes_nothing() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    // Enough files that the reading lasts long enough to cancel during it.
    constexpr int file_count = 32;
    constexpr std::size_t file_bytes = 1024 * 1024;
    const std::vector<std::byte> bytes(file_bytes, std::byte{0xAB});
    QList<QUrl> urls;
    for (int i = 0; i < file_count; ++i) {
        const std::string name = "BULK" + std::to_string(i / 10) + std::to_string(i % 10) + ".LUP";
        std::filesystem::path path = sources.path() / name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        urls.append(url_of(path));
    }

    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles(urls);
    QVERIFY(model.canGenerate());

    model.generate();
    QTRY_VERIFY_WITH_TIMEOUT(model.progressPercent() > 0, 60000);
    model.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 60000);
    QCOMPARE(model.resultState(), 3);
    QCOMPARE(model.resultText(), QStringLiteral("Cancelled."));
    QVERIFY(!directory_exists(expected_root(output.path())));
    QVERIFY(model.outputPath().isEmpty());
}

void GeneratorTest::writes_the_batch_when_asked() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");

    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.setIncludeBatch(true);
    model.setBatchPartNumber(QStringLiteral("ACM4C-0100-0200"));
    model.addFiles({url_of(data)});
    QVERIFY(model.batchPartNumberError().isEmpty());
    QVERIFY(model.canGenerate());

    model.generate();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 30000);
    QCOMPARE(model.resultState(), 1);
    QVERIFY2(model.validationFindings().isEmpty(),
             qPrintable(model.validationFindings().join(QStringLiteral("; "))));

    auto members = media_tree::read_media_set_tree(expected_root(output.path()));
    QVERIFY(members.has_value());
    const arinc665::MediaMember* first = member_of(*members, 1);
    QVERIFY(first != nullptr);
    QVERIFY(has_file(*first, "\\", "BATCHES.LUM"));
    auto set = arinc665::read_media_set(*members);
    QVERIFY(set.has_value());
    QVERIFY(set->findings.empty());
    QVERIFY(set->batches_list.has_value());
    QCOMPARE(set->batches.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(set->batches[0].file_name), QStringLiteral("ACM.LUB"));
    QCOMPARE(QString::fromStdString(std::string{set->batches[0].batch.batch_part_number.view()}),
             QStringLiteral("ACM4C-0100-0200"));
    QCOMPARE(set->batches[0].batch.targets.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(set->batches[0].batch.targets[0].target_hardware_id_position),
             QStringLiteral("ACME"));
    QCOMPARE(set->batches[0].batch.targets[0].loads.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(set->batches[0].batch.targets[0].loads[0].header_file_name),
             QStringLiteral("ACM.LUH"));
}
