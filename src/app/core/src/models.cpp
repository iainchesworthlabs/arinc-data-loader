#include "adl/app/models.hpp"

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
    if (lines_.size() >= capacity) {
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
    return parent.isValid() ? 0 : static_cast<int>(loads_.size());
}

QVariant LoadModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= loads_.size()) {
        return {};
    }
    const LoadSummary& load = loads_[index.row()];
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
        default:
            return {};
    }
}

QHash<int, QByteArray> LoadModel::roleNames() const {
    return {{Qt::DisplayRole, "display"},
            {PartNumberRole, "partNumber"},
            {HeaderFileRole, "headerFile"},
            {DetailRole, "detail"}};
}

void LoadModel::reset(QList<LoadSummary> loads) {
    beginResetModel();
    loads_ = std::move(loads);
    endResetModel();
}

}  // namespace adl::app
