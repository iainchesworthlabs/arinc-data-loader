#pragma once

// The lists the window shows. Each is a QAbstractListModel that is replaced or appended to on
// the window's thread only, from the signals of the worker.

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

#include "adl/app/events.hpp"

namespace adl::app {

// What FIND found, one row per target.
class TargetModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        AddressRole = Qt::UserRole + 1,
        HardwareIdRole,
        PositionRole,
        TypeNameRole,
        LiteralNameRole,
        ManufacturerRole,
        ThwIdPosRole,
    };

    explicit TargetModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(QList<FoundTarget> targets);
    [[nodiscard]] const FoundTarget* at(int row) const;

private:
    QList<FoundTarget> targets_;
};

// The activity log: what the target said and what the loader did, oldest first.
class EventModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role { SeverityRole = Qt::UserRole + 1, TextRole, TimeRole };

    // The most lines kept; older ones are dropped.
    static constexpr int capacity = 2000;

    explicit EventModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void append(const EventEntry& entry);
    void clear();

private:
    struct Line {
        EventEntry entry;
        QDateTime time;
    };
    QList<Line> lines_;
};

// What an Information operation found: hardware, each followed by its parts.
class ConfigurationModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role { LevelRole = Qt::UserRole + 1, TitleRole, DetailRole };

    explicit ConfigurationModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(QList<HardwareRow> rows);

private:
    QList<HardwareRow> rows_;
};

// The loads of the media set that was read.
class LoadModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role { PartNumberRole = Qt::UserRole + 1, HeaderFileRole, DetailRole };

    explicit LoadModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(QList<LoadSummary> loads);

private:
    QList<LoadSummary> loads_;
};

}  // namespace adl::app
