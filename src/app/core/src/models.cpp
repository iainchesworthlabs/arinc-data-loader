#include "adl/app/models.hpp"

#include <algorithm>
#include <utility>

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
            return line.entry.text;
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
            return row.title;
        case LevelRole:
            return static_cast<int>(row.level);
        case DetailRole:
            return row.detail;
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
        case DetailRole:
            return tr("%1, %n data file(s), %2 support file(s); for %3", "", load.data_files)
                .arg(load.header_file)
                .arg(load.support_files)
                .arg(load.target_hardware_ids.join(QStringLiteral(", ")));
        case CheckedRole:
            return row.checked;
        case RatioRole:
            return row.ratio;
        case LoadStatusRole:
            return row.status;
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
        row.status = row.checked ? tr("waiting") : QString();
        row.problem.clear();
        row.failed = false;
    }
    if (!rows_.isEmpty()) {
        changed(0, static_cast<int>(rows_.size()) - 1,
                {RatioRole, LoadStatusRole, FailedRole, ProblemRole});
    }
}

void LoadModel::apply_progress(const QList<LoadProgressRow>& progress) {
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

}  // namespace adl::app
