// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The lists the window shows. Each is a QAbstractListModel that is replaced or appended to on
// the window's thread only, from the signals of the worker.

#include "adl/app/events.hpp"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

namespace adl::app
{

// What FIND found, one row per target.
class TargetModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
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
    // The language changed: the rows are read again in the new one.
    void retranslate();

private:
    QList<FoundTarget> targets_;
};

// The activity log: what the target said and what the loader did, oldest first.
class EventModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        SeverityRole = Qt::UserRole + 1,
        TextRole,
        TimeRole
    };

    // The most lines kept when nothing else is asked; older ones are dropped. The operator's
    // preference (logCapacity) can change it per instance.
    static constexpr int default_capacity = 2000;

    explicit EventModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int capacity() const noexcept
    {
        return capacity_;
    }
    void set_capacity(int capacity) noexcept
    {
        capacity_ = capacity < 1 ? 1 : capacity;
    }

    void append(const EventEntry& entry);
    void clear();
    // The language changed: the lines are read again in the new one.
    void retranslate();

private:
    struct Line
    {
        EventEntry entry;
        QDateTime time;
    };
    QList<Line> lines_;
    int capacity_ = default_capacity;
};

// The files of the media set that was read, one row per file, from FILES.LUM.
class FileModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        MemberRole,
        PathnameRole,
        CrcRole
    };

    explicit FileModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(QList<MediaFileSummary> files);

private:
    QList<MediaFileSummary> files_;
};

// What an Information operation found: hardware, each followed by its parts.
class ConfigurationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        LevelRole = Qt::UserRole + 1,
        TitleRole,
        DetailRole
    };

    explicit ConfigurationModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(QList<HardwareRow> rows);
    // The language changed: the rows are read again in the new one.
    void retranslate();

private:
    QList<HardwareRow> rows_;
};

// The loads of the media set that was read: what each is, whether the operator has ticked it for
// an upload, and, while an upload runs, how far the target has got with it.
class LoadModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        PartNumberRole = Qt::UserRole + 1,
        HeaderFileRole,
        DetailRole,
        CheckedRole,    // writable: the operator's tick
        RatioRole,      // percent done, or -1 when the load is not in an upload
        LoadStatusRole, // the target's word for it ("in progress"), or empty
        FailedRole,     // the target gave up on it
        ProblemRole,    // what the target said about it, when it said more
    };

    explicit LoadModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    // Only CheckedRole can be set, and not while the model is locked.
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // Replaces the loads; nothing is ticked, nothing is in progress. A load that was there
    // before, under the same header file and part number, keeps its tick (so re-reading or
    // re-checking a set does not throw the operator's choice away); a new set starts unticked.
    void reset(QList<LoadSummary> loads);

    // While locked (a job runs) the ticks cannot be changed.
    void set_locked(bool locked) noexcept
    {
        locked_ = locked;
    }
    void check_all(bool checked);
    [[nodiscard]] int checked_count() const noexcept;
    [[nodiscard]] QList<LoadSummary> checked_loads() const;

    // An upload starts: the ticked loads wait, the others are not part of it.
    void begin_upload();
    // A status file: matches each row to a load by header file and part number.
    void apply_progress(const QList<LoadProgressRow>& rows);
    // The language changed: the rows are read again in the new one.
    void retranslate();

Q_SIGNALS:
    // The number of ticked loads may have changed.
    void selection_changed();

private:
    struct Row
    {
        LoadSummary load;
        bool checked = false;
        int ratio = -1;
        Message status;
        QString problem;
        bool failed = false;
    };
    void changed(int first, int last, const QList<int>& roles);

    QList<Row> rows_;
    bool locked_ = false;
};

// The files of a download. In the media defined mode the rows are the names the operator has
// written down; in the operator defined mode they are the target's list (.LNL), each with the
// operator's tick, which is the choice sent back. While a download runs the rows carry its
// progress, matched to the download status files by name.
class DownloadModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        DescriptionRole, // the target's word for the file (.LNL), or empty
        CheckedRole,     // writable: the operator's tick (operator defined mode only)
        RatioRole,       // percent done, or -1 when the row is not in the running download
        FileStatusRole,  // the target's word for it ("in progress"), or empty
        FailedRole,      // the target gave up on it
        ProblemRole,     // what the target said about it, when it said more
    };

    explicit DownloadModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    // Only CheckedRole can be set, and not while the model is locked.
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // The operator's list of a media defined download: replaces the rows with `names`.
    void reset_media(const QStringList& names);
    // Adds a name to the operator's list; refuses an empty one and one that is there already.
    [[nodiscard]] bool add_name(const QString& name);
    void remove_row(int row);
    [[nodiscard]] QStringList names() const;

    // The target's list of an operator defined download: replaces the rows, all ticked (the
    // operator untickks what they do not want).
    void reset_operator(const QList<DownloadFileRow>& files);
    void check_all(bool checked);
    [[nodiscard]] int checked_count() const noexcept;
    [[nodiscard]] QStringList checked_names() const;

    // A download starts (or the operator has confirmed the choice): the rows that are part of
    // it wait; in the operator defined mode the rows the operator untickked are dropped. The
    // ticks are locked from here until the next list or the next media defined entry.
    void begin_download();
    // A status file: matches each row to a file by name.
    void apply_progress(const QList<FileProgressRow>& rows);
    // The language changed: the rows are read again in the new one.
    void retranslate();

    // While locked (the download runs, or the choice has been made) the ticks cannot change.
    void set_locked(bool locked) noexcept
    {
        locked_ = locked;
    }

Q_SIGNALS:
    // The number of ticked files, or the list itself, may have changed.
    void selection_changed();

private:
    struct Row
    {
        QString name;
        QString description;
        bool checked = false;
        int ratio = -1;
        Message status;
        QString problem;
        bool failed = false;
    };
    void changed(int first, int last, const QList<int>& roles);

    QList<Row> rows_;
    bool locked_ = false;
    // True while the rows are the target's list (the operator's ticks are the choice); false
    // while they are the operator's own names of a media defined download.
    bool choice_mode_ = false;
};

} // namespace adl::app
