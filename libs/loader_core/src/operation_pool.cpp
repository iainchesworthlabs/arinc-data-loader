// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/operation_pool.hpp"

#include "adl/arinc615a/runner.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>
#include <variant>

namespace adl::loader_core
{

namespace
{

using adl::arinc615a::DataLoader;
using adl::arinc615a::Instant;
using adl::arinc615a::never;
using adl::platform::Owned;
using adl::platform::UdpSocket;

// How long the sockets that are beyond the wait window go without being waited on: they are looked at once for each
// pass, so a pass is this short while there are any.
constexpr adl::platform::Duration overflow_wait = std::chrono::milliseconds(2);

// How often a controller is polled while nothing else is happening: the runner's default.
constexpr adl::platform::Duration controller_poll = std::chrono::milliseconds(100);

[[nodiscard]] adl::platform::Endpoint to_platform(const adl::arinc615a::Endpoint& endpoint) noexcept
{
    return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] adl::arinc615a::Endpoint from_platform(const adl::platform::Endpoint& endpoint) noexcept
{
    return adl::arinc615a::Endpoint{adl::arinc615a::Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] bool is_download_operation(const adl::arinc615a::Operation& operation) noexcept
{
    return std::holds_alternative<adl::arinc615a::MediaDownloadOperation>(operation) ||
           std::holds_alternative<adl::arinc615a::OperatorDownloadOperation>(operation);
}

} // namespace

// One operation that has been started: its engine and everything the engine points into. It is never moved, so
// the views the engine keeps (the THW_ID_POS, the workspace) stay where they are.
struct OperationPool::Running
{
    adl::arinc615a::Address address;
    std::string thw_id_pos;
    std::string download_folder;
    RunServices services;
    bool is_download = false;
    Completion completion;
    std::vector<std::byte> workspace;
    std::unique_ptr<DataLoader> loader;
    std::array<Owned<UdpSocket>, DataLoader::max_slots> slots;
    bool abort_requested = false;
};

// An operation that could not be started, with the result it ends with: delivered by the next step().
struct OperationPool::Early
{
    OperationResult result;
    Completion completion;
};

OperationPool::OperationPool(adl::platform::Host& host, const PoolConfig& config) : host_(host), config_(config)
{
    config_.max_operations = std::max<std::size_t>(config_.max_operations, 1);
    config_.wait_window = std::clamp<std::size_t>(config_.wait_window, 1, adl::platform::max_wait_sockets);
    // One buffer for every operation: they all have the engine's default configuration.
    const adl::arinc615a::LoaderConfig defaults;
    buffer_.resize(std::max<std::size_t>(defaults.max_block_size + 4, 2048));
}

OperationPool::~OperationPool() = default;

adl::platform::Result<void> OperationPool::open()
{
    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{config_.local_address, config_.control_port};
    auto opened = host_.network().open_udp(options);
    if (!opened)
    {
        return std::unexpected(opened.error());
    }
    control_ = std::move(*opened);
    control_port_ = control_->local_endpoint().port;
    return {};
}

bool OperationPool::is_open() const noexcept
{
    return control_ != nullptr;
}

std::uint16_t OperationPool::control_port() const noexcept
{
    return control_port_;
}

std::expected<void, PoolRefusal> OperationPool::start(const PoolTarget& target,
                                                      const adl::arinc615a::Operation& operation,
                                                      const RunServices& services, Completion completion)
{
    if (!is_open())
    {
        return std::unexpected(PoolRefusal::not_open);
    }
    if (is_running(target.address))
    {
        return std::unexpected(PoolRefusal::address_busy);
    }
    if (running_.size() >= config_.max_operations)
    {
        return std::unexpected(PoolRefusal::too_many_operations);
    }

    auto operation_state = std::make_unique<Running>();
    Running& running = *operation_state;
    running.address = target.address;
    running.thw_id_pos = target.thw_id_pos;
    running.download_folder = std::string{services.download_folder};
    running.services = services;
    running.services.download_folder = running.download_folder;
    running.is_download = is_download_operation(operation);
    running.completion = std::move(completion);

    // An operation that cannot start ends as run_operation's does: it did not run, and the result says where.
    const auto cannot_start = [this, &running](adl::arinc615a::RunError error)
    {
        auto early = std::make_unique<Early>();
        early->result = make_result(host_, std::unexpected(error), running.is_download, running.services);
        early->completion = std::move(running.completion);
        early_.push_back(std::move(early));
    };
    if (target.port == 0)
    {
        // No datagram can go to port 0: refused here as the engine refuses a control port of 0.
        cannot_start(adl::arinc615a::RunError{});
        return {};
    }

    adl::arinc615a::LoaderConfig loader_config;
    loader_config.target = adl::arinc615a::Endpoint{target.address, target.port};
    loader_config.thw_id_pos = running.thw_id_pos;
    loader_config.control_port = control_port_;
    running.workspace.resize(DataLoader::workspace_bytes(loader_config));
    adl::arinc615a::LoaderServices loader_services;
    loader_services.listener = services.listener;
    loader_services.source = services.upload_source;
    loader_services.sink = services.download_sink;
    running.loader = std::make_unique<DataLoader>(loader_config, running.workspace, loader_services);

    if (const auto started = running.loader->start(adl::arinc615a::to_instant(host_.clock().now()), operation);
        !started)
    {
        cannot_start(adl::arinc615a::RunError{adl::arinc615a::RunError::Stage::start, started.error(),
                                              adl::platform::ErrorCode::other});
        return {};
    }
    running_.push_back(std::move(operation_state));
    execute(running);
    return {};
}

bool OperationPool::abort(const adl::arinc615a::Address& target)
{
    for (const auto& running : running_)
    {
        if (running->address == target)
        {
            running->abort_requested = true;
            return true;
        }
    }
    return false;
}

bool OperationPool::is_running(const adl::arinc615a::Address& target) const noexcept
{
    return std::ranges::any_of(running_, [&target](const auto& running) { return running->address == target; });
}

std::size_t OperationPool::running() const noexcept
{
    return running_.size();
}

// Carries out what the engine of `operation` asked for after a call: send this, close that. A datagram that
// cannot be sent is a lost datagram, which is what UDP promises and what the engine's time-outs are for.
void OperationPool::execute(Running& operation)
{
    for (const adl::arinc615a::Command& command : operation.loader->take_commands())
    {
        switch (command.kind)
        {
        case adl::arinc615a::Command::Kind::send_from_control:
            static_cast<void>(control_->send_to(to_platform(command.to), command.bytes));
            break;
        case adl::arinc615a::Command::Kind::send_from_slot:
        {
            Owned<UdpSocket>& socket = operation.slots[command.slot];
            if (!socket)
            {
                adl::platform::UdpOptions options; // any free port
                options.local.address = config_.local_address;
                auto opened = host_.network().open_udp(options);
                if (!opened)
                {
                    break;
                }
                socket = std::move(*opened);
            }
            static_cast<void>(socket->send_to(to_platform(command.to), command.bytes));
            break;
        }
        case adl::arinc615a::Command::Kind::release_slot:
            operation.slots[command.slot].reset();
            break;
        }
    }
}

// Where a socket that was waited on belongs: to the control port (no operation) or to a slot of one operation.
struct OperationPool::Entry
{
    UdpSocket* socket = nullptr;
    Running* operation = nullptr; // null: the control socket
    std::size_t slot = 0;
};

void OperationPool::step(adl::platform::Duration max_wait)
{
    if (!early_.empty())
    {
        // Operations that could not be started: their end is told first, from here and not from start().
        auto early = std::move(early_);
        early_.clear();
        for (auto& ended : early)
        {
            ended->completion(std::move(ended->result));
        }
        return;
    }
    if (!is_open())
    {
        return;
    }
    auto& network = host_.network();
    auto& clock = host_.clock();

    Instant now = adl::arinc615a::to_instant(clock.now());
    adl::platform::Duration timeout = max_wait;
    std::vector<Entry> entries;
    entries.push_back(Entry{control_.get(), nullptr, 0});
    for (const auto& running : running_)
    {
        if (running->abort_requested)
        {
            timeout = adl::platform::Duration::zero();
        }
        if (const Instant wake = running->loader->next_wake(); wake != never)
        {
            timeout = std::min(timeout, wake > now ? wake - now : adl::platform::Duration::zero());
        }
        if (running->services.controller != nullptr)
        {
            timeout = std::min(timeout, controller_poll);
        }
        for (std::size_t slot = 0; slot < running->slots.size(); ++slot)
        {
            if (running->slots[slot])
            {
                entries.push_back(Entry{running->slots[slot].get(), running.get(), slot});
            }
        }
    }

    // The sockets that fit one wait are waited on; the others are looked at once for each pass.
    const std::size_t waited = std::min(entries.size(), config_.wait_window);
    if (waited < entries.size())
    {
        timeout = std::min(timeout, overflow_wait);
    }
    std::array<adl::platform::UdpSocket*, adl::platform::max_wait_sockets> waiting{};
    std::array<bool, adl::platform::max_wait_sockets> ready{};
    for (std::size_t i = 0; i < waited; ++i)
    {
        waiting[i] = entries[i].socket;
    }
    const auto result = network.wait_readable(std::span<adl::platform::UdpSocket* const>{waiting}.first(waited),
                                              timeout, std::span<bool>{ready}.first(waited));
    if (!result && result.error().code != adl::platform::ErrorCode::interrupted)
    {
        fail_all(result.error().code);
        return;
    }
    now = adl::arinc615a::to_instant(clock.now());

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (i < waited && !ready[i])
        {
            continue;
        }
        // An earlier datagram of this pass may have closed (or reopened) the socket.
        UdpSocket* const socket =
            entries[i].operation == nullptr ? control_.get() : entries[i].operation->slots[entries[i].slot].get();
        if (socket == nullptr)
        {
            continue;
        }
        const auto datagram = socket->receive_from(buffer_, adl::platform::Duration::zero());
        if (!datagram)
        {
            continue;
        }
        if (datagram->truncated)
        {
            // Larger than any block: not ours to read. On the control port that is a datagram nobody can use.
            if (entries[i].operation == nullptr)
            {
                ++unrouted_;
            }
            continue;
        }
        const std::span<const std::byte> bytes{buffer_.data(), datagram->size};
        const adl::arinc615a::Endpoint from = from_platform(datagram->source);
        if (entries[i].operation != nullptr)
        {
            entries[i].operation->loader->on_transfer_datagram(now, entries[i].slot, from, bytes);
            execute(*entries[i].operation);
            continue;
        }
        const auto found = std::ranges::find(running_, from.address, [](const auto& r) { return r->address; });
        if (found == running_.end())
        {
            ++unrouted_;
            continue;
        }
        (*found)->loader->on_control_datagram(now, from, bytes);
        execute(**found);
    }

    for (const auto& running : running_)
    {
        if (running->abort_requested)
        {
            running->abort_requested = false;
            running->loader->abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
            execute(*running);
        }
        if (running->services.controller != nullptr)
        {
            running->services.controller->on_poll(*running->loader, now);
            execute(*running);
        }
        running->loader->on_timer(now);
        execute(*running);
    }
    complete_ended();
}

// The operations whose engines have nothing more to do are taken out, and then told: a completion may start
// another operation, so the pool is whole before the first one is called.
void OperationPool::complete_ended()
{
    std::vector<std::unique_ptr<Running>> ended;
    for (auto it = running_.begin(); it != running_.end();)
    {
        if ((*it)->loader->idle())
        {
            ended.push_back(std::move(*it));
            it = running_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto& operation : ended)
    {
        const std::expected<adl::arinc615a::Outcome, adl::arinc615a::RunError> outcome = operation->loader->outcome();
        OperationResult result = make_result(host_, outcome, operation->is_download, operation->services);
        operation->completion(std::move(result));
    }
}

// Waiting on the network failed: no operation can go on. Each ends as run_operation's does when it cannot wait.
void OperationPool::fail_all(adl::platform::ErrorCode code)
{
    std::vector<std::unique_ptr<Running>> ended = std::move(running_);
    running_.clear();
    for (auto& operation : ended)
    {
        const std::expected<adl::arinc615a::Outcome, adl::arinc615a::RunError> failure =
            std::unexpected(adl::arinc615a::RunError{adl::arinc615a::RunError::Stage::wait,
                                                     adl::arinc615a::Error::invalid_value, code});
        OperationResult result = make_result(host_, failure, operation->is_download, operation->services);
        operation->completion(std::move(result));
    }
}

} // namespace adl::loader_core
