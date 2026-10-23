#include "fake_host.hpp"

namespace adl::fake
{

FakeHost::FakeHost(FakeWorld& world, FakeNetwork& network, platform::Ipv4Address address)
    : clock_(world), threads_(world), network_(network, address), ipc_(threads_)
{
}

} // namespace adl::fake
