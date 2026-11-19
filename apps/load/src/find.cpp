// SPDX-License-Identifier: GPL-3.0-or-later
// find: broadcast a FIND request and report each target that answers, as it answers, until the window closes.
//
// This is the loop of adl::arinc615a::Runner::find with one thing added: the interface its socket is bound to
// can be chosen (--local-address), which the runner's own does not take. The protocol is FindClient's, which
// decides what an answer is.

#include "adl/arinc615a/find.hpp"

#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/load/exit_codes.hpp"
#include "adl/load/reason.hpp"
#include "adl/loader_core/address.hpp"
#include "commands.hpp"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace adl::load
{

namespace
{

adl::platform::Endpoint to_platform(const adl::arinc615a::Endpoint& endpoint) noexcept
{
    return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets}, endpoint.port};
}

adl::arinc615a::Endpoint from_platform(const adl::platform::Endpoint& endpoint) noexcept
{
    return adl::arinc615a::Endpoint{adl::arinc615a::Address{endpoint.address.octets}, endpoint.port};
}

FoundTarget found_of(const adl::arinc615a::FindTarget& answer)
{
    FoundTarget found;
    found.address = adl::loader_core::format_ipv4(answer.from.address.octets);
    found.hardware_id = std::string{answer.hardware_id.view()};
    found.type_name = std::string{answer.type_name.view()};
    found.position = std::string{answer.position.view()};
    found.literal_name = std::string{answer.literal_name.view()};
    found.manufacturer_code = std::string{answer.manufacturer_code.view()};
    found.thw_id_pos = adl::loader_core::thw_id_pos_of(answer.hardware_id.view(), answer.position.view());
    return found;
}

std::string answered_text(std::size_t count)
{
    return count == 0 ? "No target answered."
                      : std::to_string(count) + (count == 1 ? " target answered." : " targets answered.");
}

// `finished` for a FIND that could not run.
int could_not_run(EventWriter& events, const std::string& why)
{
    FinishedEvent event;
    event.reason = to_string(Reason::local_error);
    event.detail = "FIND could not run: " + why + ".";
    event.exit_code = exit_code_of(Reason::local_error);
    events.finished(event);
    return event.exit_code;
}

} // namespace

int run_find_command(adl::platform::Host& host, const Options& options, EventWriter& events)
{
    events.started(to_string(Command::find), std::nullopt, std::nullopt);

    // The standard has the answers come back to port 1001. A socket of another FIND on this computer may hold it
    // too, so the address is shared.
    adl::platform::UdpOptions udp;
    udp.local.address =
        options.local_address ? adl::platform::Ipv4Address{*options.local_address} : adl::platform::Ipv4Address::any();
    udp.local.port = adl::arinc615a::find_port;
    udp.allow_broadcast = true;
    udp.reuse_address = true;
    auto opened = host.network().open_udp(udp);
    if (!opened)
    {
        return could_not_run(events, std::string{"the port could not be opened ("} +
                                         adl::platform::to_string(opened.error().code) + ")");
    }
    adl::platform::UdpSocket& socket = **opened;

    adl::arinc615a::FindConfig config;
    config.window = std::chrono::seconds(options.seconds);
    adl::arinc615a::FindClient client(config);
    client.start(adl::arinc615a::to_instant(host.clock().now()));
    for (const adl::arinc615a::Address& destination : client.destinations())
    {
        // A request that cannot be sent is a lost one, which is what UDP promises and the window is for.
        static_cast<void>(socket.send_to(to_platform(adl::arinc615a::Endpoint{destination, adl::arinc615a::find_port}),
                                         client.request()));
    }

    std::vector<std::byte> buffer(adl::arinc615a::find_max_answer_size + 16);
    std::array<adl::platform::UdpSocket*, 1> waiting{&socket};
    std::array<bool, 1> ready{};
    std::size_t told = 0; // the answers written as events so far
    while (!client.done())
    {
        adl::arinc615a::Instant now = adl::arinc615a::to_instant(host.clock().now());
        const adl::arinc615a::Instant wake = client.next_wake();
        const adl::platform::Duration timeout = wake > now ? wake - now : adl::platform::Duration::zero();
        const auto waited = host.network().wait_readable(waiting, timeout, std::span<bool>{ready});
        if (!waited)
        {
            if (waited.error().code == adl::platform::ErrorCode::interrupted)
            {
                continue;
            }
            return could_not_run(events, std::string{"waiting for the network failed ("} +
                                             adl::platform::to_string(waited.error().code) + ")");
        }
        now = adl::arinc615a::to_instant(host.clock().now());
        if (*waited != 0)
        {
            if (const auto datagram = socket.receive_from(buffer, adl::platform::Duration::zero());
                datagram && !datagram->truncated)
            {
                client.on_datagram(now, from_platform(datagram->source),
                                   std::span<const std::byte>{buffer.data(), datagram->size});
            }
        }
        client.on_timer(now);
        for (; told < client.targets().size(); ++told)
        {
            events.target_found(found_of(client.targets()[told]));
        }
    }

    FinishedEvent event;
    event.completed = true;
    event.reason = to_string(Reason::completed);
    event.detail = answered_text(client.targets().size());
    event.exit_code = exit_code_of(Reason::completed);
    event.count = client.targets().size();
    events.finished(event);
    return event.exit_code;
}

} // namespace adl::load
