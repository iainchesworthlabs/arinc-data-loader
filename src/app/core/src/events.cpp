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
    qRegisterMetaType<MediaSetSummary>();
}

}  // namespace adl::app
