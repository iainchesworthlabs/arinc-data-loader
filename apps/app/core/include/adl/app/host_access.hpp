#pragma once

// Where the worker gets the platform from. The window uses the real host; a test hands over a
// host that runs in memory, so an operation can be run against a simulated target.

#include "adl/platform/host.hpp"

#include <QString>
#include <expected>
#include <functional>
#include <memory>

namespace adl::app
{

// A host, and who keeps it alive: `owned` for one made for the job, empty for one the caller
// keeps (a test's).
struct HostLease
{
    std::unique_ptr<adl::platform::Host> owned;
    adl::platform::Host* host = nullptr;
};

// Called on the worker's thread, once per job. The error is for the operator.
using HostFactory = std::function<std::expected<HostLease, QString>()>;

// The Host of the platform this program was built for (adl::platform::make_host).
[[nodiscard]] HostFactory system_host_factory();

} // namespace adl::app
