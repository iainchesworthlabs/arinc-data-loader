#include "test_classes.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <QString>
#include <QUrl>
#include <QtTest>

#include "adl/app/media_set_summary.hpp"
#include "adl/arinc665/naming.hpp"
#include "adl/arinc665/part_number.hpp"
#include "adl/arinc665/portable_name.hpp"
#include "adl/gen/generator_model.hpp"
#include "adl/gen/generator_worker.hpp"
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

// A job that is valid, with one data file, for the worker to run directly (on this thread).
adl::gen::GenerateJob good_job(const std::filesystem::path& source,
                               const std::filesystem::path& output) {
    adl::gen::GenerateJob job;
    job.load_part_number = QStringLiteral("ACM47-1234-5678");
    job.media_set_part_number = set_part_number;
    job.header_file_name = QStringLiteral("ACM.LUH");
    job.target_hardware_ids = {QStringLiteral("ACME")};
    job.output_directory = QString::fromStdU16String(output.u16string());
    job.files = {adl::gen::JobFile{QString::fromStdU16String(source.u16string()),
                                   QStringLiteral("DATA.LUP"), adl::gen::SourceFileModel::data_kind,
                                   0}};
    return job;
}

// Runs the job to its end and returns how it ended (every job ends with exactly one `finished`).
adl::gen::GenerateOutcome run_job(adl::gen::GeneratorWorker& worker,
                                  const adl::gen::GenerateJob& job) {
    adl::gen::GenerateOutcome outcome;
    int finished = 0;
    const auto connection = QObject::connect(&worker, &adl::gen::GeneratorWorker::finished, &worker,
                                             [&](const adl::gen::GenerateOutcome& result) {
                                                 outcome = result;
                                                 ++finished;
                                             });
    worker.generate(job);
    QObject::disconnect(connection);
    if (finished != 1) {
        outcome.state = -1;
    }
    return outcome;
}

// The names of what is in `directory`, sorted (empty when it is empty or missing).
QStringList names_in(const std::filesystem::path& directory) {
    QStringList names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        names.append(QString::fromStdU16String(entry.path().filename().u16string()));
    }
    names.sort();
    return names;
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

void GeneratorTest::refuses_a_name_the_folder_tree_would_refuse() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles({url_of(data)});
    QVERIFY(model.canGenerate());
    auto* files = static_cast<adl::gen::SourceFileModel*>(model.files());
    const auto message = [&] {
        return files->data(files->index(0), adl::gen::SourceFileModel::MessageRole).toString();
    };

    // Names that ARINC 665's rules let through and that the media set folder tree refuses: the
    // form says so on the row instead of letting the generation fail at write time.
    const std::vector<std::string> refused = {"DATA*.LUP", "DATA?.LUP", "DATA\".LUP", "DATA<.LUP",
                                              "DATA>.LUP", "DATA|.LUP", "DATA.LUP.",  "CON.LUP",
                                              "nul",       "com1.LUP",  "LPT9"};
    for (const std::string& name : refused) {
        const QString qname = QString::fromStdString(name);
        QVERIFY2(arinc665::check_data_file_name(name).empty(), qPrintable(qname));
        QVERIFY2(!arinc665::is_portable_file_name(name), qPrintable(qname));
        QVERIFY(files->setData(files->index(0), qname, adl::gen::SourceFileModel::FileNameRole));
        QVERIFY2(message().contains(QStringLiteral("every system")), qPrintable(qname));
        QVERIFY2(!model.canGenerate(), qPrintable(qname));
        QVERIFY2(!model.filesError().isEmpty(), qPrintable(qname));
    }
    model.generate();
    QCOMPARE(model.resultState(), 2);
    QVERIFY(!directory_exists(expected_root(output.path())));

    // The same rule holds for a support file, and a name that is fine is not marked.
    QVERIFY(files->setData(files->index(0), QStringLiteral("support"),
                           adl::gen::SourceFileModel::KindRole));
    QVERIFY(files->setData(files->index(0), QStringLiteral("AUX.LDR"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(message().contains(QStringLiteral("every system")));
    QVERIFY(files->setData(files->index(0), QStringLiteral("CONFIG.LDR"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(message().isEmpty());
    QVERIFY(files->setData(files->index(0), QStringLiteral("data"),
                           adl::gen::SourceFileModel::KindRole));
    QVERIFY(files->setData(files->index(0), QStringLiteral("DATA.LUP"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(message().isEmpty());
    QVERIFY(model.canGenerate());

    // A name the standard refuses keeps its own, more specific, message.
    QVERIFY(files->setData(files->index(0), QStringLiteral("DA:TA.LUP"),
                           adl::gen::SourceFileModel::FileNameRole));
    QVERIFY(message().contains(QStringLiteral("2.2.2")));
}

void GeneratorTest::refuses_a_header_or_batch_file_name_the_folder_tree_would_refuse() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles({url_of(data)});
    QVERIFY(model.canGenerate());

    // The header file's name: the standard asks for a manufacturer's code and .LUH, and a code
    // such as CON makes a device name of it.
    for (const char* name : {"CON.LUH", "ACM*.LUH"}) {
        QVERIFY2(arinc665::check_header_file_name(name).empty(), name);
        model.setHeaderFileName(QString::fromLatin1(name));
        QVERIFY2(model.headerFileNameError().contains(QStringLiteral("every system")), name);
        QVERIFY2(!model.canGenerate(), name);
    }
    model.setHeaderFileName(QStringLiteral("ACM.LUH"));
    QVERIFY(model.headerFileNameError().isEmpty());
    QVERIFY(model.canGenerate());

    // The batch file is named for the first three characters of the batch part number.
    const auto batch = arinc665::make_load_part_number("NUL", "0100-0200");
    QVERIFY(batch.has_value());
    model.setIncludeBatch(true);
    model.setBatchPartNumber(QString::fromStdString(std::string{batch->view()}));
    QVERIFY(model.batchPartNumberError().contains(QStringLiteral("every system")));
    QVERIFY(!model.canGenerate());
    model.setBatchPartNumber(QStringLiteral("ACM4C-0100-0200"));
    QVERIFY(model.batchPartNumberError().isEmpty());
    QVERIFY(model.canGenerate());
}

void GeneratorTest::a_change_of_language_keeps_the_outcome_of_the_run() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorModel model;
    fill_form(model, output.path());
    model.addFiles({url_of(data)});
    model.generate();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 30000);
    QCOMPARE(model.resultState(), 1);
    const QString path = model.outputPath();
    const QString text = model.resultText();
    QVERIFY(!path.isEmpty());
    QVERIFY(!text.isEmpty());

    // The language is not an edit of the form: the outcome, and the Open folder button that
    // goes with the output path, stay.
    model.retranslate();
    QCOMPARE(model.resultState(), 1);
    QCOMPARE(model.outputPath(), path);
    QCOMPARE(model.resultText(), text);

    // An edit of the form still clears it.
    model.setBatchPartNumber(QStringLiteral("ACM4C-0100-0200"));
    QCOMPARE(model.resultState(), 0);
    QVERIFY(model.outputPath().isEmpty());
}

void GeneratorTest::any_check_value_follows_the_check_value_column() {
    adl::test::TempDir sources;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    const auto support = write_file(sources.path(), "CONFIG.LDR", "CONFIG 12345");
    adl::gen::SourceFileModel files;
    QVERIFY(!files.property("anyCheckValue").toBool());
    QSignalSpy changes(&files, &adl::gen::SourceFileModel::filesChanged);

    files.addFiles({url_of(data), url_of(support)});
    QVERIFY(!files.property("anyCheckValue").toBool());
    QVERIFY(files.setData(files.index(1), 2, adl::gen::SourceFileModel::CheckTypeRole));
    QVERIFY(files.property("anyCheckValue").toBool());
    QVERIFY(files.any_check_value());
    QVERIFY(changes.count() >= 2);  // a change in the column is announced
    files.remove(1);
    QVERIFY(!files.any_check_value());
    QVERIFY(files.setData(files.index(0), 5, adl::gen::SourceFileModel::CheckTypeRole));
    QVERIFY(files.any_check_value());
    QVERIFY(files.setData(files.index(0), 0, adl::gen::SourceFileModel::CheckTypeRole));
    QVERIFY(!files.any_check_value());
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

void GeneratorTest::a_cancel_before_anything_is_written_says_nothing_was_written() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    worker.set_checkpoint_hook([&](std::string_view place) {
        if (place == "write") {
            worker.request_cancel();
        }
    });
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 3);  // cancelled, not failed
    QCOMPARE(outcome.text.text(), QStringLiteral("Cancelled."));
    QCOMPARE(outcome.detail.text(), QStringLiteral("Nothing was written."));
    QCOMPARE(names_in(output.path()), QStringList{});
}

void GeneratorTest::a_cancel_after_the_tree_is_written_removes_it_and_says_so() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    bool tree_was_there = false;
    worker.set_checkpoint_hook([&](std::string_view place) {
        if (place == "written") {
            // The tree is on the disk at this point: that is what the cancel finds.
            tree_was_there = !names_in(output.path()).isEmpty();
            worker.request_cancel();
        }
    });
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QVERIFY(tree_was_there);
    QCOMPARE(outcome.state, 3);  // cancelled, not failed
    QCOMPARE(outcome.text.text(), QStringLiteral("Cancelled."));
    // It is not "nothing was written": something was, and has been taken away.
    QVERIFY2(!outcome.detail.text().contains(QStringLiteral("Nothing was written")),
             qPrintable(outcome.detail.text()));
    QVERIFY2(outcome.detail.text().contains(QStringLiteral("removed")),
             qPrintable(outcome.detail.text()));
    QVERIFY(outcome.output_path.isEmpty());
    QCOMPARE(names_in(output.path()), QStringList{});
}

void GeneratorTest::a_cancel_after_the_set_verified_removes_it_too() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    worker.set_checkpoint_hook([&](std::string_view place) {
        if (place == "verified") {
            worker.request_cancel();
        }
    });
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 3);  // cancelled, not failed
    QCOMPARE(outcome.text.text(), QStringLiteral("Cancelled."));
    QVERIFY(outcome.detail.text().contains(QStringLiteral("removed")));
    QCOMPARE(names_in(output.path()), QStringList{});
}

void GeneratorTest::a_write_that_fails_leaves_no_partial_set() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    // A name no file system takes (longer than any of them allows): the write of the set fails
    // after the member's directory is made.
    auto job = good_job(data, output.path());
    job.files[0].file_name = QString(300, QLatin1Char('A')) + QStringLiteral(".LUP");
    adl::gen::GeneratorWorker worker;
    const auto outcome = run_job(worker, job);
    QCOMPARE(outcome.state, 2);
    QVERIFY2(outcome.text.text().contains(QStringLiteral("could not be")),
             qPrintable(outcome.text.text()));
    QVERIFY(outcome.output_path.isEmpty());
    // What had been made was taken away, and the operator is told so.
    QCOMPARE(names_in(output.path()), QStringList{});
    QVERIFY2(outcome.detail.text().contains(QStringLiteral("removed")),
             qPrintable(outcome.detail.text()));
}

void GeneratorTest::a_file_that_cannot_be_allocated_ends_the_job_cleanly() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    worker.set_checkpoint_hook([](std::string_view place) {
        if (place == "allocate") {
            throw std::bad_alloc();  // what resizing the buffer of a huge file would do
        }
    });
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 2);
    QVERIFY2(outcome.text.text().contains(QStringLiteral("too large")),
             qPrintable(outcome.text.text()));
    QCOMPARE(names_in(output.path()), QStringList{});
}

void GeneratorTest::any_other_allocation_failure_ends_the_job_cleanly() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    // Out of memory after the temporary directory exists: the debris is removed on the way out.
    worker.set_checkpoint_hook([&](std::string_view place) {
        if (place == "written") {
            throw std::bad_alloc();
        }
    });
    auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 2);
    QVERIFY2(outcome.text.text().contains(QStringLiteral("out of memory")),
             qPrintable(outcome.text.text()));
    QCOMPARE(names_in(output.path()), QStringList{});

    worker.set_checkpoint_hook([](std::string_view place) {
        if (place == "load") {
            throw std::runtime_error("something unexpected");
        }
    });
    outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 2);
    QVERIFY(outcome.detail.text().contains(QStringLiteral("something unexpected")));
}

void GeneratorTest::the_set_appears_under_its_name_in_one_step() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    adl::gen::GeneratorWorker worker;
    // While the set is being checked it is not under its name yet, only in a directory beside it.
    QStringList while_checking;
    worker.set_checkpoint_hook([&](std::string_view place) {
        if (place == "written") {
            while_checking = names_in(output.path());
        }
    });
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 1);
    QCOMPARE(while_checking, QStringList{set_part_number + QStringLiteral(".writing")});
    // When it is done, it is under its name and the temporary directory is gone.
    QCOMPARE(names_in(output.path()), QStringList{set_part_number});
    QCOMPARE(outcome.output_path,
             QString::fromStdU16String(expected_root(output.path()).u16string()));
}

void GeneratorTest::a_set_that_exists_is_never_overwritten() {
    adl::test::TempDir sources;
    adl::test::TempDir output;
    const auto data = write_file(sources.path(), "DATA.LUP", "DATA DATA DATA!!");
    // A set under the name already, an empty directory under the next name, and the debris of an
    // earlier job that was killed.
    write_file(output.path(), "unrelated.txt", "mine");
    std::filesystem::create_directories(expected_root(output.path()));
    write_file(expected_root(output.path()), "keep.txt", "an earlier set");
    std::filesystem::create_directories(output.path() / (set_part_number.toStdString() + "_2"));
    const auto debris = output.path() / (set_part_number.toStdString() + ".writing");
    std::filesystem::create_directories(debris);
    write_file(debris, "half.bin", "half a file");

    adl::gen::GeneratorWorker worker;
    const auto outcome = run_job(worker, good_job(data, output.path()));
    QCOMPARE(outcome.state, 1);
    // It took the empty directory's name, and left everything else as it found it.
    const auto second = output.path() / (set_part_number.toStdString() + "_2");
    QCOMPARE(outcome.output_path, QString::fromStdU16String(second.u16string()));
    QVERIFY(std::filesystem::exists(expected_root(output.path()) / "keep.txt"));
    QVERIFY(std::filesystem::exists(debris / "half.bin"));
    QVERIFY(std::filesystem::exists(output.path() / "unrelated.txt"));
    const auto on_disk = media_tree::read_media_set_tree(second);
    QVERIFY(on_disk.has_value());
    QVERIFY(arinc665::read_media_set(*on_disk).has_value());
}
