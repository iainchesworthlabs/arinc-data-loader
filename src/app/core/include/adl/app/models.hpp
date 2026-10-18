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

// The loads of the media set that was read: what each is, whether the operator has ticked it for
// an upload, and, while an upload runs, how far the target has got with it.
class LoadModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        PartNumberRole = Qt::UserRole + 1,
        HeaderFileRole,
        DetailRole,
        CheckedRole,     // writable: the operator's tick
        RatioRole,       // percent done, or -1 when the load is not in an upload
        LoadStatusRole,  // the target's word for it ("in progress"), or empty
        FailedRole,      // the target gave up on it
        ProblemRole,     // what the target said about it, when it said more
    };

    explicit LoadModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    // Only CheckedRole can be set, and not while the model is locked.
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // Replaces the loads; nothing is ticked, nothing is in progress.
    void reset(QList<LoadSummary> loads);

    // While locked (a job runs) the ticks cannot be changed.
    void set_locked(bool locked) noexcept { locked_ = locked; }
    void check_all(bool checked);
    [[nodiscard]] int checked_count() const noexcept;
    [[nodiscard]] QList<LoadSummary> checked_loads() const;

    // An upload starts: the ticked loads wait, the others are not part of it.
    void begin_upload();
    // A status file: matches each row to a load by header file and part number.
    void apply_progress(const QList<LoadProgressRow>& rows);

Q_SIGNALS:
    // The number of ticked loads may have changed.
    void selection_changed();

private:
    struct Row {
        LoadSummary load;
        bool checked = false;
        int ratio = -1;
        QString status;
        QString problem;
        bool failed = false;
    };
    void changed(int first, int last, const QList<int>& roles);

    QList<Row> rows_;
    bool locked_ = false;
};

}  // namespace adl::app
