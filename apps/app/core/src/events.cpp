// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/events.hpp"

#include "adl/app/text.hpp"
#include "adl/loader_core/address.hpp"

namespace adl::app
{

QString FoundTarget::thw_id_pos() const
{
    return to_qstring(adl::loader_core::thw_id_pos_of(hardware_id.toStdString(), position.toStdString()));
}

void register_metatypes()
{
    qRegisterMetaType<Message>();
    qRegisterMetaType<EventEntry>();
    qRegisterMetaType<FoundTarget>();
    qRegisterMetaType<QList<FoundTarget>>();
    qRegisterMetaType<HardwareRow>();
    qRegisterMetaType<QList<HardwareRow>>();
    qRegisterMetaType<OperationSummary>();
    qRegisterMetaType<LoadSummary>();
    qRegisterMetaType<QList<LoadSummary>>();
    qRegisterMetaType<MediaFileSummary>();
    qRegisterMetaType<QList<MediaFileSummary>>();
    qRegisterMetaType<ValidationReport>();
    qRegisterMetaType<LoadProgressRow>();
    qRegisterMetaType<QList<LoadProgressRow>>();
    qRegisterMetaType<FileProgressRow>();
    qRegisterMetaType<QList<FileProgressRow>>();
    qRegisterMetaType<DownloadFileRow>();
    qRegisterMetaType<QList<DownloadFileRow>>();
    qRegisterMetaType<MediaSetSummary>();
}

} // namespace adl::app
