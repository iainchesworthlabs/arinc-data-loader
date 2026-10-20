#pragma once

// The files the operator chose for the media set, one row per file, and what the window shows
// about each: the path on disk, the name it will have on the media set (editable, defaults to
// the source file's own name), whether it is a data or a support file, the check value type the
// header file will record for it, its size, and what is wrong with the row, if anything. The
// names are checked with the ARINC 665 naming rules and have to be unique, case-insensitively,
// so the model revalidates every row whenever anything changes.

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QUrl>

#include "adl/gen/generator_worker.hpp"

namespace adl::gen {

class SourceFileModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SourcePathRole = Qt::UserRole + 1,  // the path on disk, read-only
        FileNameRole,                       // the name on the media set, writable
        KindRole,                           // "data" or "support", writable
        CheckTypeRole,                      // the check value type's index, writable
        SizeRole,                           // human readable, read-only
        MessageRole,                        // the row's validation message, read-only
    };

    // The kinds a row's KindRole can hold.
    inline static const QString data_kind = QStringLiteral("data");
    inline static const QString support_kind = QStringLiteral("support");

    explicit SourceFileModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // The operator's list operations; `urls` is what a multi-select file dialog hands over.
    Q_INVOKABLE void addFiles(const QList<QUrl>& urls);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void moveUp(int row);
    Q_INVOKABLE void moveDown(int row);
    Q_INVOKABLE void clear();

    // For GeneratorModel: the rows as the worker takes them, and the model's state.
    [[nodiscard]] QList<JobFile> files() const;
    [[nodiscard]] int data_file_count() const noexcept;
    [[nodiscard]] bool all_valid() const noexcept;

    // The language changed: the rows' sizes and messages are said again in the new one.
    void retranslate();

Q_SIGNALS:
    // Anything that could change whether a generation may start.
    void filesChanged();

private:
    struct Row {
        QString source_path;
        QString file_name;
        QString kind = data_kind;
        int check_type = 0;  // CheckValueType::none
        qint64 size = -1;  // bytes, or -1 when it could not be read
        QString message;
    };

    void revalidate();
    void changed(int first, int last);

    QList<Row> rows_;
};

}  // namespace adl::gen
