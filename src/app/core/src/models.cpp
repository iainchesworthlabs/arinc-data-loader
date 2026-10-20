#include "adl/app/models.hpp"

#include <algorithm>
#include <utility>

#include <adl/profiling.hpp>

namespace adl::app {

// ---- TargetModel -------------------------------------------------------------------------

int TargetModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(targets_.size());
}

QVariant TargetModel::data(const QModelIndex& index, int role) const {
    const FoundTarget* target = index.isValid() ? at(index.row()) : nullptr;
    if (target == nullptr) {
        return {};
    }
    switch (role) {
        case Qt::DisplayRole:
            return tr("%1 at %2")
                .arg(target->literal_name.isEmpty() ? target->thw_id_pos() : target->literal_name,
                     target->address);
        case AddressRole:
            return target->address;
        case HardwareIdRole:
            return target->hardware_id;
        case PositionRole:
            return target->position;
        case TypeNameRole:
            return target->type_name;
        case LiteralNameRole:
            return target->literal_name;
        case ManufacturerRole:
            return target->manufacturer_code;
        case ThwIdPosRole:
            return target->thw_id_pos();
        default:
            return {};
    }
}

QHash<int, QByteArray> TargetModel::roleNames() const {
    return {{Qt::DisplayRole, "summary"},       {AddressRole, "address"},
            {HardwareIdRole, "hardwareId"},     {PositionRole, "position"},
            {TypeNameRole, "typeName"},         {LiteralNameRole, "literalName"},
            {ManufacturerRole, "manufacturer"}, {ThwIdPosRole, "thwIdPos"}};
}

void TargetModel::retranslate() {
    if (!targets_.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(targets_.size()) - 1), {Qt::DisplayRole});
    }
}

void TargetModel::reset(QList<FoundTarget> targets) {
    beginResetModel();
    targets_ = std::move(targets);
    endResetModel();
}

const FoundTarget* TargetModel::at(int row) const {
    return row >= 0 && row < targets_.size() ? &targets_[row] : nullptr;
}

// ---- EventModel --------------------------------------------------------------------------

int EventModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(lines_.size());
}

QVariant EventModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= lines_.size()) {
        return {};
    }
    const Line& line = lines_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case TextRole:
            return line.entry.text.text();
        case SeverityRole:
            return static_cast<int>(line.entry.severity);
        case TimeRole:
            return line.time.toString(QStringLiteral("HH:mm:ss"));
        default:
            return {};
    }
}

QHash<int, QByteArray> EventModel::roleNames() const {
    return {{Qt::DisplayRole, "display"},
            {SeverityRole, "severity"},
            {TextRole, "message"},
            {TimeRole, "time"}};
}

void EventModel::append(const EventEntry& entry) {
    if (lines_.size() >= capacity_) {
        beginRemoveRows({}, 0, 0);
        lines_.removeFirst();
        endRemoveRows();
    }
    const int row = static_cast<int>(lines_.size());
    beginInsertRows({}, row, row);
    lines_.append({entry, QDateTime::currentDateTime()});
    endInsertRows();
}

void EventModel::retranslate() {
    if (!lines_.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(lines_.size()) - 1));
    }
}

void EventModel::clear() {
    beginResetModel();
    lines_.clear();
    endResetModel();
}

// ---- FileModel ---------------------------------------------------------------------------

int FileModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(files_.size());
}

QVariant FileModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= files_.size()) {
        return {};
    }
    const MediaFileSummary& file = files_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return file.name;
        case MemberRole:
            return file.member;
        case PathnameRole:
            return file.pathname;
        case CrcRole:
            return file.crc16;
        default:
            return {};
    }
}

QHash<int, QByteArray> FileModel::roleNames() const {
    return {
        {NameRole, "name"}, {MemberRole, "member"}, {PathnameRole, "pathname"}, {CrcRole, "crc"}};
}

void FileModel::reset(QList<MediaFileSummary> files) {
    beginResetModel();
    files_ = std::move(files);
    endResetModel();
}

// ---- ConfigurationModel ------------------------------------------------------------------

int ConfigurationModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ConfigurationModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const HardwareRow& row = rows_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return row.title.text();
        case LevelRole:
            return static_cast<int>(row.level);
        case DetailRole:
            return row.detail.text();
        default:
            return {};
    }
}

QHash<int, QByteArray> ConfigurationModel::roleNames() const {
    return {{Qt::DisplayRole, "display"},
            {LevelRole, "level"},
            {TitleRole, "title"},
            {DetailRole, "detail"}};
}

void ConfigurationModel::retranslate() {
    if (!rows_.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
    }
}

void ConfigurationModel::reset(QList<HardwareRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

// ---- LoadModel ---------------------------------------------------------------------------

int LoadModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant LoadModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const Row& row = rows_[index.row()];
    const LoadSummary& load = row.load;
    switch (role) {
        case Qt::DisplayRole:
        case PartNumberRole:
            return load.part_number;
        case HeaderFileRole:
            return load.header_file;
        case DetailRole: {
            // One count to a sentence: the two numbers of files are phrases of their own.
            const Message targets =
                load.target_hardware_ids.isEmpty()
                    ? Message::translated(&LoadModel::tr, QT_TR_NOOP("no target ID"))
                    : Message::verbatim(load.target_hardware_ids.join(QStringLiteral(", ")));
            return Message::translated(
                       &LoadModel::tr, QT_TR_NOOP("%1 · %2, %3; for %4"),
                       {Message::verbatim(load.header_file),
                        Message::translated(&LoadModel::tr, QT_TR_NOOP("%n data file(s)"), {},
                                            load.data_files),
                        Message::translated(&LoadModel::tr, QT_TR_NOOP("%n support file(s)"), {},
                                            load.support_files),
                        targets})
                .text();
        }
        case CheckedRole:
            return row.checked;
        case RatioRole:
            return row.ratio;
        case LoadStatusRole:
            return row.status.text();
        case FailedRole:
            return row.failed;
        case ProblemRole:
            return row.problem;
        default:
            return {};
    }
}

QHash<int, QByteArray> LoadModel::roleNames() const {
    return {{Qt::DisplayRole, "display"},   {PartNumberRole, "partNumber"},
            {HeaderFileRole, "headerFile"}, {DetailRole, "detail"},
            {CheckedRole, "checked"},       {RatioRole, "ratio"},
            {LoadStatusRole, "loadStatus"}, {FailedRole, "failed"},
            {ProblemRole, "problem"}};
}

Qt::ItemFlags LoadModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags result = QAbstractListModel::flags(index);
    if (index.isValid() && !locked_) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool LoadModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != CheckedRole || locked_ || !index.isValid() || index.row() < 0 ||
        index.row() >= rows_.size()) {
        return false;
    }
    const bool checked = value.toBool();
    if (rows_[index.row()].checked != checked) {
        rows_[index.row()].checked = checked;
        changed(index.row(), index.row(), {CheckedRole});
        Q_EMIT selection_changed();
    }
    return true;
}

void LoadModel::reset(QList<LoadSummary> loads) {
    beginResetModel();
    QList<Row> rows;
    rows.reserve(loads.size());
    for (LoadSummary& load : loads) {
        // The operator's tick follows a load that is still there (same header file and part
        // number); everything else starts unticked.
        bool checked = false;
        for (const Row& old : rows_) {
            if (old.load.header_file == load.header_file &&
                old.load.part_number == load.part_number) {
                checked = old.checked;
                break;
            }
        }
        rows.append(Row{std::move(load), checked, -1, {}, {}, false});
    }
    rows_ = std::move(rows);
    endResetModel();
    Q_EMIT selection_changed();
}

void LoadModel::retranslate() {
    if (!rows_.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
    }
}

void LoadModel::changed(int first, int last, const QList<int>& roles) {
    Q_EMIT dataChanged(index(first), index(last), roles);
}

void LoadModel::check_all(bool checked) {
    if (locked_ || rows_.isEmpty()) {
        return;
    }
    bool any = false;
    for (Row& row : rows_) {
        any = any || row.checked != checked;
        row.checked = checked;
    }
    if (any) {
        changed(0, static_cast<int>(rows_.size()) - 1, {CheckedRole});
        Q_EMIT selection_changed();
    }
}

int LoadModel::checked_count() const noexcept {
    return static_cast<int>(
        std::count_if(rows_.cbegin(), rows_.cend(), [](const Row& row) { return row.checked; }));
}

QList<LoadSummary> LoadModel::checked_loads() const {
    QList<LoadSummary> loads;
    for (const Row& row : rows_) {
        if (row.checked) {
            loads.append(row.load);
        }
    }
    return loads;
}

void LoadModel::begin_upload() {
    for (Row& row : rows_) {
        row.ratio = row.checked ? 0 : -1;
        row.status = row.checked ? Message::translated(&LoadModel::tr, QT_TR_NOOP("waiting"))
                                 : Message();
        row.problem.clear();
        row.failed = false;
    }
    if (!rows_.isEmpty()) {
        changed(0, static_cast<int>(rows_.size()) - 1,
                {RatioRole, LoadStatusRole, FailedRole, ProblemRole});
    }
}

void LoadModel::apply_progress(const QList<LoadProgressRow>& progress) {
    const adl::profiling::Zone zone{"app: load progress to model"};
    for (const LoadProgressRow& update : progress) {
        for (int i = 0; i < rows_.size(); ++i) {
            Row& row = rows_[i];
            if (row.load.header_file != update.header_file ||
                row.load.part_number != update.part_number) {
                continue;
            }
            row.ratio = update.ratio;
            row.status = update.status;
            row.problem = update.description;
            row.failed = update.failed;
            changed(i, i, {RatioRole, LoadStatusRole, FailedRole, ProblemRole});
            break;
        }
    }
}

// ---- DownloadModel -----------------------------------------------------------------------

int DownloadModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant DownloadModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const Row& row = rows_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return row.name;
        case DescriptionRole:
            return row.description;
        case CheckedRole:
            return row.checked;
        case RatioRole:
            return row.ratio;
        case FileStatusRole:
            return row.status.text();
        case FailedRole:
            return row.failed;
        case ProblemRole:
            return row.problem;
        default:
            return {};
    }
}

QHash<int, QByteArray> DownloadModel::roleNames() const {
    return {{NameRole, "name"},      {DescriptionRole, "description"}, {CheckedRole, "checked"},
            {RatioRole, "ratio"},    {FileStatusRole, "fileStatus"},   {FailedRole, "failed"},
            {ProblemRole, "problem"}};
}

Qt::ItemFlags DownloadModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags result = QAbstractListModel::flags(index);
    // Only the ticks of the target's list are writable, and not while the model is locked.
    if (index.isValid() && choice_mode_ && !locked_) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool DownloadModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != CheckedRole || locked_ || !choice_mode_ || !index.isValid() || index.row() < 0 ||
        index.row() >= rows_.size()) {
        return false;
    }
    const bool checked = value.toBool();
    if (rows_[index.row()].checked != checked) {
        rows_[index.row()].checked = checked;
        changed(index.row(), index.row(), {CheckedRole});
        Q_EMIT selection_changed();
    }
    return true;
}

void DownloadModel::reset_media(const QStringList& names) {
    beginResetModel();
    QList<Row> rows;
    rows.reserve(names.size());
    for (const QString& name : names) {
        rows.append(Row{name, {}, false, -1, {}, {}, false});
    }
    rows_ = std::move(rows);
    choice_mode_ = false;
    endResetModel();
    Q_EMIT selection_changed();
}

bool DownloadModel::add_name(const QString& name) {
    const QString trimmed = name.trimmed();
    if (locked_ || choice_mode_ || trimmed.isEmpty()) {
        return false;
    }
    for (const Row& row : rows_) {
        if (row.name == trimmed) {
            return false;
        }
    }
    const int row = static_cast<int>(rows_.size());
    beginInsertRows({}, row, row);
    rows_.append(Row{trimmed, {}, false, -1, {}, {}, false});
    endInsertRows();
    Q_EMIT selection_changed();
    return true;
}

void DownloadModel::remove_row(int row) {
    if (locked_ || choice_mode_ || row < 0 || row >= rows_.size()) {
        return;
    }
    beginRemoveRows({}, row, row);
    rows_.removeAt(row);
    endRemoveRows();
    Q_EMIT selection_changed();
}

QStringList DownloadModel::names() const {
    QStringList names;
    names.reserve(rows_.size());
    for (const Row& row : rows_) {
        names.append(row.name);
    }
    return names;
}

void DownloadModel::reset_operator(const QList<DownloadFileRow>& files) {
    beginResetModel();
    QList<Row> rows;
    rows.reserve(files.size());
    for (const DownloadFileRow& file : files) {
        // All ticked: the target offered the files, and the operator untickks what they do
        // not want.
        rows.append(Row{file.name, file.description, true, -1, {}, {}, false});
    }
    rows_ = std::move(rows);
    choice_mode_ = true;
    endResetModel();
    Q_EMIT selection_changed();
}

void DownloadModel::check_all(bool checked) {
    if (locked_ || !choice_mode_ || rows_.isEmpty()) {
        return;
    }
    bool any = false;
    for (Row& row : rows_) {
        any = any || row.checked != checked;
        row.checked = checked;
    }
    if (any) {
        changed(0, static_cast<int>(rows_.size()) - 1, {CheckedRole});
        Q_EMIT selection_changed();
    }
}

int DownloadModel::checked_count() const noexcept {
    return static_cast<int>(
        std::count_if(rows_.cbegin(), rows_.cend(), [](const Row& row) { return row.checked; }));
}

QStringList DownloadModel::checked_names() const {
    QStringList names;
    for (const Row& row : rows_) {
        if (row.checked) {
            names.append(row.name);
        }
    }
    return names;
}

void DownloadModel::begin_download() {
    beginResetModel();
    for (Row& row : rows_) {
        if (!choice_mode_ || row.checked) {
            row.ratio = 0;
            row.status = Message::translated(&DownloadModel::tr, QT_TR_NOOP("waiting"));
            row.problem.clear();
            row.failed = false;
        }
    }
    if (choice_mode_) {
        // The operator's choice is what was sent: the untickked files are not part of it.
        rows_.removeIf([](const Row& row) { return !row.checked; });
    }
    endResetModel();
    Q_EMIT selection_changed();  // the list itself may have shrunk to the chosen files
}

void DownloadModel::apply_progress(const QList<FileProgressRow>& progress) {
    const adl::profiling::Zone zone{"app: download progress to model"};
    for (const FileProgressRow& update : progress) {
        for (int i = 0; i < rows_.size(); ++i) {
            Row& row = rows_[i];
            if (row.name != update.name) {
                continue;
            }
            row.ratio = update.ratio;
            row.status = update.status;
            row.problem = update.description;
            row.failed = update.failed;
            changed(i, i, {RatioRole, FileStatusRole, FailedRole, ProblemRole});
            break;
        }
    }
}

void DownloadModel::retranslate() {
    if (!rows_.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
    }
}

void DownloadModel::changed(int first, int last, const QList<int>& roles) {
    Q_EMIT dataChanged(index(first), index(last), roles);
}

}  // namespace adl::app
