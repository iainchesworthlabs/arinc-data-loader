#pragma once

// The plain values that cross from the engine's thread to the window: every one is a copy
// (the engine hands out views that die with the call), made of Qt strings so a queued signal
// can carry it. Nothing here knows about the engine.

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace adl::app {

enum class Severity : int { info = 0, notice = 1, warning = 2, error = 3 };

// One line of the activity log.
struct EventEntry {
    Severity severity = Severity::info;
    QString text;
};

// A target that answered FIND.
struct FoundTarget {
    QString address;  // dotted decimal
    QString hardware_id;
    QString type_name;
    QString position;
    QString literal_name;
    QString manufacturer_code;

    // The <THW_ID_POS> its protocol files are named by: the hardware identifier, an underscore
    // and the position ("HNPFMS" at "L" is "HNPFMS_L"), or the identifier alone for a target
    // that gave no position.
    [[nodiscard]] QString thw_id_pos() const;
};

// One line of what an Information operation found: a piece of hardware or one of its parts.
struct HardwareRow {
    enum class Level : int { hardware = 0, part = 1 };
    Level level = Level::hardware;
    QString title;
    QString detail;
};

// How a job ended. `success` is the operation's own verdict (the target said it was
// completed); `outcome` is a short phrase for it and `detail` says more when there is more.
struct OperationSummary {
    bool success = false;
    QString outcome;
    QString detail;
    quint32 statuses_received = 0;
    quint32 statuses_lost = 0;
};

// One load of a media set.
struct LoadSummary {
    QString part_number;
    QString header_file;
    int data_files = 0;
    int support_files = 0;
    QStringList target_hardware_ids;
};

// What reading a media set from a directory found. `error` is set, and the rest empty, when
// it could not be read.
struct MediaSetSummary {
    QString path;
    QString error;
    QString part_number;
    int members = 0;
    int batches = 0;
    QList<LoadSummary> loads;
    QStringList findings;  // where the set differs from what the standard says it should be
};

// Qt needs to know these to carry them in a queued signal. Safe to call more than once.
void register_metatypes();

}  // namespace adl::app

Q_DECLARE_METATYPE(adl::app::EventEntry)
Q_DECLARE_METATYPE(adl::app::FoundTarget)
Q_DECLARE_METATYPE(adl::app::HardwareRow)
Q_DECLARE_METATYPE(adl::app::OperationSummary)
Q_DECLARE_METATYPE(adl::app::LoadSummary)
Q_DECLARE_METATYPE(adl::app::MediaSetSummary)
