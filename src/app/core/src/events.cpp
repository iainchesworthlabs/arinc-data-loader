#include "adl/app/events.hpp"

namespace adl::app {

QString FoundTarget::thw_id_pos() const {
    return position.isEmpty() ? hardware_id : hardware_id + QLatin1Char('_') + position;
}

void register_metatypes() {
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

}  // namespace adl::app
