#include "adl/app/host_access.hpp"

#include <QCoreApplication>
#include <utility>

namespace adl::app
{

namespace
{

// The words of a failure to start the system services; the factory returns its error as text,
// made in the language of the moment the job starts.
class HostAccess
{
    Q_DECLARE_TR_FUNCTIONS(HostAccess)
};

} // namespace

HostFactory system_host_factory()
{
    return []() -> std::expected<HostLease, QString>
    {
        auto made = adl::platform::make_host();
        if (!made)
        {
            return std::unexpected(HostAccess::tr("The system services could not be started (%1).")
                                       .arg(QString::fromLatin1(adl::platform::to_string(made.error().code))));
        }
        HostLease lease;
        lease.owned = std::move(*made);
        lease.host = lease.owned.get();
        return lease;
    };
}

} // namespace adl::app
