#include "adl/gen/source_file_model.hpp"

#include <algorithm>
#include <filesystem>

#include <QFileInfo>
#include <QStringList>
#include <QUrl>

#include "adl/arinc665/naming.hpp"

namespace adl::gen {

namespace {

// A size for the window: whole bytes below a kilobyte, one decimal above.
QString human_size(qint64 bytes) {
    if (bytes < 1024) {
        return SourceFileModel::tr("%1 B").arg(bytes);
    }
    const double kilobytes = static_cast<double>(bytes) / 1024.0;
    if (kilobytes < 1024.0) {
        return SourceFileModel::tr("%1 KB").arg(kilobytes, 0, 'f', 1);
    }
    const double megabytes = kilobytes / 1024.0;
    if (megabytes < 1024.0) {
        return SourceFileModel::tr("%1 MB").arg(megabytes, 0, 'f', 1);
    }
    return SourceFileModel::tr("%1 GB").arg(megabytes / 1024.0, 0, 'f', 1);
}

// The naming rules' findings as one message, in the window's usual "clause: text" form.
QString findings_text(const adl::arinc665::Findings& findings) {
    QStringList pieces;
    for (const adl::arinc665::Finding& finding : findings) {
        pieces.append(QStringLiteral("%1: %2")
                          .arg(QString::fromUtf8(finding.clause.data(),
                                                 static_cast<qsizetype>(finding.clause.size())),
                               QString::fromUtf8(finding.message.data(),
                                                 static_cast<qsizetype>(finding.message.size()))));
    }
    return pieces.join(QStringLiteral("; "));
}

}  // namespace

SourceFileModel::SourceFileModel(QObject* parent) : QAbstractListModel(parent) {}

int SourceFileModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant SourceFileModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const Row& row = rows_.at(index.row());
    switch (role) {
        case SourcePathRole:
            return row.source_path;
        case FileNameRole:
            return row.file_name;
        case KindRole:
            return row.kind;
        case CheckTypeRole:
            return row.check_type;
        case SizeRole:
            return row.size;
        case MessageRole:
            return row.message;
        default:
            return {};
    }
}

QHash<int, QByteArray> SourceFileModel::roleNames() const {
    return {{SourcePathRole, "sourcePath"},
            {FileNameRole, "fileName"},
            {KindRole, "kind"},
            {CheckTypeRole, "checkType"},
            {SizeRole, "size"},
            {MessageRole, "message"}};
}

Qt::ItemFlags SourceFileModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (index.isValid()) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool SourceFileModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }
    Row& row = rows_[index.row()];
    bool changed_row = false;
    switch (role) {
        case FileNameRole: {
            const QString name = value.toString();
            if (name == row.file_name) {
                return true;
            }
            row.file_name = name;
            changed_row = true;
            break;
        }
        case KindRole: {
            const QString kind = value.toString();
            if (kind != data_kind && kind != support_kind) {
                return false;
            }
            if (kind == row.kind) {
                return true;
            }
            row.kind = kind;
            changed_row = true;
            break;
        }
        case CheckTypeRole: {
            bool ok = false;
            const int check_type = value.toInt(&ok);
            if (!ok || check_type < 0 || check_type > 5) {
                return false;
            }
            if (check_type == row.check_type) {
                return true;
            }
            row.check_type = check_type;
            changed_row = true;
            break;
        }
        default:
            return false;
    }
    revalidate();
    Q_EMIT filesChanged();
    return changed_row;
}

void SourceFileModel::addFiles(const QList<QUrl>& urls) {
    bool added = false;
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (path.isEmpty()) {
            continue;
        }
        Row row;
        row.source_path = path;
        row.file_name = QFileInfo(path).fileName();
        std::error_code error;
        const auto size = std::filesystem::file_size(
            std::filesystem::path{path.toStdU16String()}, error);
        row.size = error ? tr("unknown") : human_size(static_cast<qint64>(size));
        beginInsertRows({}, rowCount(), rowCount());
        rows_.append(std::move(row));
        endInsertRows();
        added = true;
    }
    if (added) {
        revalidate();
        Q_EMIT filesChanged();
    }
}

void SourceFileModel::remove(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    beginRemoveRows({}, row, row);
    rows_.removeAt(row);
    endRemoveRows();
    revalidate();
    Q_EMIT filesChanged();
}

void SourceFileModel::moveUp(int row) {
    if (row <= 0 || row >= rowCount()) {
        return;
    }
    beginMoveRows({}, row, row, {}, row - 1);
    rows_.move(row, row - 1);
    endMoveRows();
    Q_EMIT filesChanged();
}

void SourceFileModel::moveDown(int row) {
    if (row < 0 || row >= rowCount() - 1) {
        return;
    }
    beginMoveRows({}, row, row, {}, row + 2);
    rows_.move(row, row + 1);
    endMoveRows();
    Q_EMIT filesChanged();
}

void SourceFileModel::clear() {
    if (rows_.isEmpty()) {
        return;
    }
    beginResetModel();
    rows_.clear();
    endResetModel();
    Q_EMIT filesChanged();
}

QList<JobFile> SourceFileModel::files() const {
    QList<JobFile> files;
    files.reserve(static_cast<qsizetype>(rows_.size()));
    for (const Row& row : rows_) {
        JobFile file;
        file.source_path = row.source_path;
        file.file_name = row.file_name;
        file.kind = row.kind;
        file.check_type = row.check_type;
        files.append(std::move(file));
    }
    return files;
}

int SourceFileModel::data_file_count() const noexcept {
    int count = 0;
    for (const Row& row : rows_) {
        if (row.kind == data_kind) {
            ++count;
        }
    }
    return count;
}

bool SourceFileModel::all_valid() const noexcept {
    return std::all_of(rows_.begin(), rows_.end(),
                       [](const Row& row) { return row.message.isEmpty(); });
}

void SourceFileModel::revalidate() {
    const int count = rowCount();
    if (count == 0) {
        return;
    }
    // Names have to be unique case-insensitively, as they are on the desktops the trees are
    // written on, so every row is checked against every other one.
    QList<QString> seen;
    for (int i = 0; i < count; ++i) {
        Row& row = rows_[i];
        row.message.clear();
        const QString name = row.file_name;
        if (name.isEmpty()) {
            row.message = tr("The name is empty.");
        } else {
            const std::string text = name.toStdString();
            const adl::arinc665::Findings findings =
                row.kind == data_kind ? adl::arinc665::check_data_file_name(text)
                                      : adl::arinc665::check_support_file_name(text);
            if (!findings.empty()) {
                row.message = findings_text(findings);
            }
        }
        if (row.message.isEmpty()) {
            bool duplicate = false;
            for (const QString& other : seen) {
                if (name.compare(other, Qt::CaseInsensitive) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                row.message = tr("Another file has this name.");
            } else {
                seen.append(name);
            }
        }
    }
    Q_EMIT dataChanged(index(0), index(count - 1),
                       {SourcePathRole, FileNameRole, KindRole, CheckTypeRole, SizeRole,
                        MessageRole});
}

}  // namespace adl::gen
