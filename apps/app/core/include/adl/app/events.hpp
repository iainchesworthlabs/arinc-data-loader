#pragma once

// The plain values that cross from the engine's thread to the window: every one is a copy
// (the engine hands out views that die with the call), made of Qt strings so a queued signal
// can carry it. Nothing here knows about the engine. What the application says in its own words
// is a Message, translated when it is read so a language switch reaches what is already shown;
// what comes from the target or the libraries (names, identifiers, the target's descriptions) is
// a QString and stays as it was given.

#include "adl/app/message.hpp"

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace adl::app
{

enum class Severity : int
{
    info = 0,
    notice = 1,
    warning = 2,
    error = 3
};

// One line of the activity log.
struct EventEntry
{
    Severity severity = Severity::info;
    Message text;
};

// A target that answered FIND.
struct FoundTarget
{
    QString address; // dotted decimal
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
struct HardwareRow
{
    enum class Level : int
    {
        hardware = 0,
        part = 1
    };
    Level level = Level::hardware;
    Message title;
    Message detail;
};

// How a job ended. `success` is the operation's own verdict (the target said it was
// completed); `outcome` is a short phrase for it, `detail` says more when there is more, and
// `hint` is what the operator can do about a failure the application can recognise (a media
// set that cannot be read, a host that cannot be started, an address that is not one, an
// abort the target did not confirm). Empty when there is nothing honest to say.
struct OperationSummary
{
    bool success = false;
    Message outcome;
    Message detail;
    Message hint;
    quint32 statuses_received = 0;
    quint32 statuses_lost = 0;
    // A download: the files the operation asked for, the files that arrived, and the directory
    // they were written into (the DNLD_DATA_<THW_ID_POS>_<number> directory of 5.4.4.3).
    quint32 files_requested = 0;
    quint32 files_received = 0;
    QString download_directory;
    // An upload that was stopped because the media set on the disk is no longer what was
    // checked: the set has to be checked again before it can be uploaded.
    bool media_set_changed = false;
};

// One load of a media set.
struct LoadSummary
{
    QString part_number;
    QString header_file;
    int data_files = 0;
    int support_files = 0;
    QStringList target_hardware_ids;
};

// One file of a media set, as FILES.LUM lists it.
struct MediaFileSummary
{
    QString name;
    QString pathname; // from the root of the member, as FILES.LUM writes it ("\" for the root)
    int member = 0;   // the member the file is on
    QString crc16;    // the file's 16-bit CRC, four hexadecimal digits
};

// What reading a media set from a directory found. `error` is set, and the rest empty, when
// it could not be read.
struct MediaSetSummary
{
    QString path;
    QString error;
    QString part_number;
    int members = 0;
    int batches = 0;
    QList<LoadSummary> loads;
    QList<MediaFileSummary> files; // every file of the set but FILES.LUM, from FILES.LUM
    QStringList findings;          // where the set differs from what the standard says it should be
};

// How checking a media set went. A set that could not be read has `error` set and is not
// valid; the findings are deviations from the standard's recommendations, which are warnings
// and do not make a set invalid (a set that reads is structurally valid: the read verifies
// every file and its check values, so a read error is what makes it invalid, and nothing
// else).
struct ValidationReport
{
    QString path;
    QString error;
    int loads = 0;
    int files = 0;
    QStringList findings;
};

// How one load of an upload is going, from an upload status file (6.4.5).
struct LoadProgressRow
{
    QString header_file;
    QString part_number;
    int ratio = 0; // percent, 0 to 100
    Message status;
    QString description;
    bool failed = false; // the target gave up on this load
};

// How one file of a download is going, from a download status file (6.4.7) and from which
// files have arrived. The protocol gives no figure per file, so the ratio is 100 once the
// file has arrived and 0 while it has not; the status is the target's word for it.
struct FileProgressRow
{
    QString name;
    int ratio = 0; // percent: 100 when the file has arrived, 0 while the target is at it
    Message status;
    QString description;
    bool failed = false; // the target gave up on this file
};

// One file of the target's list of an operator defined download (.LNL, 6.4.8).
struct DownloadFileRow
{
    QString name;
    QString description; // empty when the target gave none
};

// Qt needs to know these to carry them in a queued signal. Safe to call more than once.
void register_metatypes();

} // namespace adl::app

Q_DECLARE_METATYPE(adl::app::EventEntry)
Q_DECLARE_METATYPE(adl::app::FoundTarget)
Q_DECLARE_METATYPE(adl::app::HardwareRow)
Q_DECLARE_METATYPE(adl::app::OperationSummary)
Q_DECLARE_METATYPE(adl::app::LoadSummary)
Q_DECLARE_METATYPE(adl::app::MediaFileSummary)
Q_DECLARE_METATYPE(adl::app::MediaSetSummary)
Q_DECLARE_METATYPE(adl::app::ValidationReport)
Q_DECLARE_METATYPE(adl::app::LoadProgressRow)
Q_DECLARE_METATYPE(adl::app::FileProgressRow)
Q_DECLARE_METATYPE(adl::app::DownloadFileRow)
