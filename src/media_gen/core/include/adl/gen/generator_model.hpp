#pragma once

// What the window talks to: the state of the media set generator as properties, and the few
// things the operator can do as invokable functions. It lives on the window's thread, owns the
// thread the worker runs on, and turns the worker's signals into the outcome the window shows.
// One generation runs at a time; while one runs, `busy` is true and another is refused. The form
// is validated as it changes: every field carries its own error text, and `canGenerate` says
// whether everything needed is there and valid.

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "adl/gen/generator_worker.hpp"
#include "adl/gen/source_file_model.hpp"

namespace adl::gen {

class GeneratorModel : public QObject {
    Q_OBJECT

    // The form.
    Q_PROPERTY(QString loadPartNumber READ loadPartNumber WRITE setLoadPartNumber NOTIFY
                   loadPartNumberChanged)
    Q_PROPERTY(QString mediaSetPartNumber READ mediaSetPartNumber WRITE setMediaSetPartNumber NOTIFY
                   mediaSetPartNumberChanged)
    Q_PROPERTY(QString batchPartNumber READ batchPartNumber WRITE setBatchPartNumber NOTIFY
                   batchPartNumberChanged)
    Q_PROPERTY(bool includeBatch READ includeBatch WRITE setIncludeBatch NOTIFY includeBatchChanged)
    Q_PROPERTY(QString headerFileName READ headerFileName WRITE setHeaderFileName NOTIFY
                   headerFileNameChanged)
    Q_PROPERTY(
        int loadTypeIndex READ loadTypeIndex WRITE setLoadTypeIndex NOTIFY loadTypeIndexChanged)
    Q_PROPERTY(QStringList targetHardwareIds READ targetHardwareIds WRITE setTargetHardwareIds
                   NOTIFY targetHardwareIdsChanged)
    Q_PROPERTY(int memberCount READ memberCount WRITE setMemberCount NOTIFY memberCountChanged)
    Q_PROPERTY(int fileCheckTypeIndex READ fileCheckTypeIndex WRITE setFileCheckTypeIndex NOTIFY
                   fileCheckTypeIndexChanged)
    Q_PROPERTY(int filesListCheckTypeIndex READ filesListCheckTypeIndex WRITE
                   setFilesListCheckTypeIndex NOTIFY filesListCheckTypeIndexChanged)
    Q_PROPERTY(int loadCheckTypeIndex READ loadCheckTypeIndex WRITE setLoadCheckTypeIndex NOTIFY
                   loadCheckTypeIndexChanged)
    Q_PROPERTY(QString outputDirectory READ outputDirectory NOTIFY outputDirectoryChanged)

    // What is wrong with the form, empty field by field when nothing is.
    Q_PROPERTY(
        QString loadPartNumberError READ loadPartNumberError NOTIFY loadPartNumberErrorChanged)
    Q_PROPERTY(QString mediaSetPartNumberError READ mediaSetPartNumberError NOTIFY
                   mediaSetPartNumberErrorChanged)
    Q_PROPERTY(
        QString batchPartNumberError READ batchPartNumberError NOTIFY batchPartNumberErrorChanged)
    Q_PROPERTY(
        QString headerFileNameError READ headerFileNameError NOTIFY headerFileNameErrorChanged)
    Q_PROPERTY(QString targetHardwareIdsError READ targetHardwareIdsError NOTIFY
                   targetHardwareIdsErrorChanged)
    Q_PROPERTY(
        QString outputDirectoryError READ outputDirectoryError NOTIFY outputDirectoryErrorChanged)
    Q_PROPERTY(QString filesError READ filesError NOTIFY filesErrorChanged)

    // A generation may start: the form is valid, there is at least one data file and an output
    // directory, and nothing is running.
    Q_PROPERTY(bool canGenerate READ canGenerate NOTIFY canGenerateChanged)

    // While a generation runs.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY progressChanged)

    // The outcome of the last generation: 0 none yet, 1 the set was written and verified, 2 not.
    Q_PROPERTY(int resultState READ resultState NOTIFY resultChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultChanged)
    Q_PROPERTY(QString resultDetail READ resultDetail NOTIFY resultChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY resultChanged)
    Q_PROPERTY(QStringList validationFindings READ validationFindings NOTIFY resultChanged)

    // The list of files the operator chose.
    Q_PROPERTY(QObject* files READ files CONSTANT)

public:
    explicit GeneratorModel(QObject* parent = nullptr);
    ~GeneratorModel() override;

    [[nodiscard]] QString loadPartNumber() const { return load_part_number_; }
    void setLoadPartNumber(const QString& part_number);
    [[nodiscard]] QString mediaSetPartNumber() const { return media_set_part_number_; }
    void setMediaSetPartNumber(const QString& part_number);
    [[nodiscard]] QString batchPartNumber() const { return batch_part_number_; }
    void setBatchPartNumber(const QString& part_number);
    [[nodiscard]] bool includeBatch() const noexcept { return include_batch_; }
    void setIncludeBatch(bool include);
    [[nodiscard]] QString headerFileName() const { return header_file_name_; }
    void setHeaderFileName(const QString& name);
    [[nodiscard]] int loadTypeIndex() const noexcept { return load_type_index_; }
    void setLoadTypeIndex(int index);
    [[nodiscard]] QStringList targetHardwareIds() const { return target_hardware_ids_; }
    void setTargetHardwareIds(const QStringList& ids);
    [[nodiscard]] int memberCount() const noexcept { return member_count_; }
    void setMemberCount(int members);
    [[nodiscard]] int fileCheckTypeIndex() const noexcept { return file_check_type_index_; }
    void setFileCheckTypeIndex(int index);
    [[nodiscard]] int filesListCheckTypeIndex() const noexcept {
        return files_list_check_type_index_;
    }
    void setFilesListCheckTypeIndex(int index);
    [[nodiscard]] int loadCheckTypeIndex() const noexcept { return load_check_type_index_; }
    void setLoadCheckTypeIndex(int index);
    [[nodiscard]] QString outputDirectory() const { return output_directory_; }

    [[nodiscard]] QString loadPartNumberError() const { return load_part_number_error_; }
    [[nodiscard]] QString mediaSetPartNumberError() const { return media_set_part_number_error_; }
    [[nodiscard]] QString batchPartNumberError() const { return batch_part_number_error_; }
    [[nodiscard]] QString headerFileNameError() const { return header_file_name_error_; }
    [[nodiscard]] QString targetHardwareIdsError() const { return target_hardware_ids_error_; }
    [[nodiscard]] QString outputDirectoryError() const { return output_directory_error_; }
    [[nodiscard]] QString filesError() const { return files_error_; }

    [[nodiscard]] bool canGenerate() const noexcept { return can_generate_; }
    [[nodiscard]] bool busy() const noexcept { return busy_; }
    [[nodiscard]] int progressPercent() const noexcept { return progress_percent_; }

    [[nodiscard]] int resultState() const noexcept { return result_state_; }
    [[nodiscard]] QString resultText() const { return result_text_; }
    [[nodiscard]] QString resultDetail() const { return result_detail_; }
    [[nodiscard]] QString outputPath() const { return output_path_; }
    [[nodiscard]] QStringList validationFindings() const { return validation_findings_; }

    [[nodiscard]] QObject* files() { return &files_; }

    // The options of the window's combos, in the order their indices take.
    [[nodiscard]] Q_INVOKABLE QStringList loadTypeOptions() const;
    [[nodiscard]] Q_INVOKABLE QStringList checkValueOptions() const;

public Q_SLOTS:
    void chooseOutputDirectory(const QUrl& directory);
    void generate();
    // Asks the generation that is running to stop. Nothing is written by a cancelled job.
    void cancel();

    // The operator's list operations, on the SourceFileModel.
    void addFiles(const QList<QUrl>& urls);
    void removeFile(int row);
    void moveFileUp(int row);
    void moveFileDown(int row);
    void clearFiles();

    void addTargetHardwareId(const QString& id);
    void removeTargetHardwareId(int row);

Q_SIGNALS:
    void loadPartNumberChanged();
    void mediaSetPartNumberChanged();
    void batchPartNumberChanged();
    void includeBatchChanged();
    void headerFileNameChanged();
    void loadTypeIndexChanged();
    void targetHardwareIdsChanged();
    void memberCountChanged();
    void fileCheckTypeIndexChanged();
    void filesListCheckTypeIndexChanged();
    void loadCheckTypeIndexChanged();
    void outputDirectoryChanged();

    void loadPartNumberErrorChanged();
    void mediaSetPartNumberErrorChanged();
    void batchPartNumberErrorChanged();
    void headerFileNameErrorChanged();
    void targetHardwareIdsErrorChanged();
    void outputDirectoryErrorChanged();
    void filesErrorChanged();

    void canGenerateChanged();
    void busyChanged();
    void progressChanged();
    void resultChanged();

private:
    void refresh_validation();
    void on_finished(const GenerateOutcome& outcome);
    void set_result(int state, const QString& text, const QString& detail);
    void set_progress(int percent);

    GeneratorWorker* worker_ = nullptr;
    QThread thread_;

    SourceFileModel files_;

    QString load_part_number_;
    QString media_set_part_number_;
    QString batch_part_number_;
    bool include_batch_ = false;
    QString header_file_name_;
    bool header_file_edited_ = false;
    int load_type_index_ = -1;
    QStringList target_hardware_ids_;
    int member_count_ = 1;
    int file_check_type_index_ = 0;
    int files_list_check_type_index_ = 0;
    int load_check_type_index_ = 0;
    QString output_directory_;

    QString load_part_number_error_;
    QString media_set_part_number_error_;
    QString batch_part_number_error_;
    QString header_file_name_error_;
    QString target_hardware_ids_error_;
    QString output_directory_error_;
    QString files_error_;
    bool can_generate_ = false;

    bool busy_ = false;
    int progress_percent_ = -1;

    int result_state_ = 0;
    QString result_text_;
    QString result_detail_;
    QString output_path_;
    QStringList validation_findings_;
};

}  // namespace adl::gen
