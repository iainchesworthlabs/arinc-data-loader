#include "adl/app/host_access.hpp"

#include <utility>

#include <QObject>

namespace adl::app {

HostFactory system_host_factory() {
    return []() -> std::expected<HostLease, QString> {
        auto made = adl::platform::make_host();
        if (!made) {
            return std::unexpected(
                QObject::tr("The system services could not be started (%1).")
                    .arg(QString::fromLatin1(adl::platform::to_string(made.error().code))));
        }
        HostLease lease;
        lease.owned = std::move(*made);
        lease.host = lease.owned.get();
        return lease;
    };
}

}  // namespace adl::app
