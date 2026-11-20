// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_scenario/player.hpp"

#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/tftp.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <variant>

namespace adl::loader_scenario
{
namespace
{

using adl::arinc615a::Command;
using adl::arinc615a::Endpoint;
using adl::arinc615a::Instant;
using adl::arinc615a::never;

constexpr std::int64_t default_reorder_hold_ms = 1000;

[[nodiscard]] adl::arinc615a::Duration milliseconds_of(std::int64_t value) noexcept
{
    return std::chrono::duration_cast<adl::arinc615a::Duration>(std::chrono::milliseconds{value});
}

[[nodiscard]] char lower(char c) noexcept
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool equal_without_case(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, [](char x, char y) { return lower(x) == lower(y); });
}

[[nodiscard]] bool is_status_file(std::string_view name) noexcept
{
    const auto kind = adl::arinc615a::kind_of_file_name(name);
    return kind && (*kind == adl::arinc615a::ProtocolFileKind::lcs || *kind == adl::arinc615a::ProtocolFileKind::lus ||
                    *kind == adl::arinc615a::ProtocolFileKind::lns);
}

// The hexadecimal digit that differs from `c` in its lowest bit, in the case `c` is in.
[[nodiscard]] char flip_hex_digit(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return static_cast<char>('0' + ((c - '0') ^ 1));
    }
    if (c >= 'A' && c <= 'F')
    {
        return static_cast<char>('A' + ((c - 'A') ^ 1));
    }
    if (c >= 'a' && c <= 'f')
    {
        return static_cast<char>('a' + ((c - 'a') ^ 1));
    }
    return c == '0' ? '1' : '0';
}

// SplitMix64 (Steele, Lea and Flood, 2014): integer arithmetic only, so every system gives the same numbers.
[[nodiscard]] std::uint64_t splitmix64(std::uint64_t& state) noexcept
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
}

} // namespace

struct Player::Seen
{
    DatagramKind kind = DatagramKind::any;
    std::optional<std::uint16_t> block;
    bool reply = false;
    std::string_view file;
    bool has_check_value = false;
    std::size_t check_offset = 0; // the first digit of the value, from the first byte of the datagram
};

Player::Player(Scenario scenario, OperationName operation, FaultListener* listener)
    : scenario_(std::move(scenario)), operation_(operation), listener_(listener), applied_(scenario_.steps.size(), 0),
      spent_(scenario_.steps.size(), false), stream_(scenario_.steps.size(), 0),
      scratch_(scratch_capacity * datagram_capacity), held_storage_(held_capacity * datagram_capacity)
{
    // One stream for each step, so that what one step draws never depends on what another did: the seed starts a
    // first generator, and its nth output seeds the generator of the nth step.
    std::uint64_t first = scenario_.seed.value_or(0);
    for (std::uint64_t& stream : stream_)
    {
        stream = splitmix64(first);
    }
}

Player::~Player() = default;

// ---- The generators --------------------------------------------------------------------------------------------

std::uint64_t Player::next_random(std::size_t index) noexcept
{
    return splitmix64(stream_[index]);
}

// A number in [low, high] by rejection, so that every number is as likely as any other.
std::uint64_t Player::draw(std::size_t index, std::uint64_t low, std::uint64_t high) noexcept
{
    const std::uint64_t range = high - low + 1;
    if (range == 0)
    {
        return next_random(index);
    }
    const std::uint64_t remainder = (0 - range) % range; // 2^64 mod range
    for (;;)
    {
        const std::uint64_t output = next_random(index);
        if (remainder == 0 || output < 0 - remainder)
        {
            return low + output % range;
        }
    }
}

bool Player::chance(std::size_t index) noexcept
{
    const auto& per_mille = scenario_.steps[index].per_mille;
    return !per_mille || draw(index, 1, 1000) <= *per_mille;
}

// ---- Bookkeeping -----------------------------------------------------------------------------------------------

std::size_t Player::applied(std::size_t step) const noexcept
{
    return step >= 1 && step <= applied_.size() ? applied_[step - 1] : 0;
}

std::size_t Player::applied() const noexcept
{
    std::size_t total = 0;
    for (const std::size_t count : applied_)
    {
        total += count;
    }
    return total;
}

std::vector<std::size_t> Player::unapplied_steps() const
{
    std::vector<std::size_t> steps;
    for (std::size_t i = 0; i < applied_.size(); ++i)
    {
        if (applied_[i] == 0 && active(scenario_.steps[i]))
        {
            steps.push_back(i + 1);
        }
    }
    return steps;
}

std::size_t Player::held() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(held_, [](const Held& entry) { return entry.used; }));
}

std::span<std::byte> Player::take_scratch(std::size_t bytes) noexcept
{
    if (bytes > datagram_capacity || scratch_used_ >= scratch_capacity)
    {
        return {};
    }
    const std::span<std::byte> room{scratch_.data() + scratch_used_ * datagram_capacity, datagram_capacity};
    ++scratch_used_;
    return room.first(bytes);
}

std::span<std::byte> Player::held_bytes(std::size_t index) noexcept
{
    return std::span<std::byte>{held_storage_.data() + index * datagram_capacity, datagram_capacity};
}

void Player::report(std::size_t index, Instant now, const Seen* seen, const Command* command,
                    std::int64_t amount) noexcept
{
    const Step& step = scenario_.steps[index];
    ++applied_[index];
    if (!acts_on_datagrams(step.fault.action))
    {
        spent_[index] = true;
    }
    if (listener_ == nullptr)
    {
        return;
    }
    FaultEvent event;
    event.step = index + 1;
    event.action = step.fault.action;
    event.trigger = step.when.kind;
    event.at = step.when.value;
    if (const std::uint64_t reached = occurrences(step, now); reached > 1 && step.when.every != 0)
    {
        event.at += (reached - 1) * step.when.every;
    }
    event.operation = operation_;
    event.time = now;
    event.amount = amount;
    if (seen != nullptr)
    {
        event.datagram = seen->kind;
        event.file = seen->file;
        event.block = seen->block;
    }
    if (command != nullptr)
    {
        event.to = command->to;
    }
    listener_->on_fault(event);
}

// ---- What the target sends -------------------------------------------------------------------------------------

void Player::remember_request(const Endpoint& from, std::string_view file) noexcept
{
    // The place the request's answer will be going to: the first datagram the loader sends there is the reply.
    Pending* slot = nullptr;
    for (Pending& pending : pending_)
    {
        if (pending.used && pending.peer == from)
        {
            slot = &pending;
            break;
        }
    }
    if (slot == nullptr)
    {
        slot = &pending_[next_pending_];
        next_pending_ = (next_pending_ + 1) % pending_capacity;
    }
    slot->used = true;
    slot->peer = from;
    slot->length = std::min(file.size(), slot->file.size());
    std::memcpy(slot->file.data(), file.data(), slot->length);
}

void Player::begin_call(Instant /*now*/) noexcept
{
    scratch_used_ = 0;
}

void Player::on_arrival(Instant now, std::optional<std::size_t> slot, const Endpoint& from,
                        std::span<const std::byte> bytes) noexcept
{
    if (!began_)
    {
        began_ = now;
    }
    if (!slot)
    {
        if (const auto packet = adl::arinc615a::decode_packet(bytes))
        {
            if (const auto* request = std::get_if<adl::arinc615a::TftpRequest>(&*packet))
            {
                ++requests_;
                if (request->kind == adl::arinc615a::TftpRequest::Kind::write && is_status_file(request->file_name))
                {
                    ++status_files_;
                }
                remember_request(from, request->file_name);
            }
        }
    }
    arm_operation_steps(now);
}

// ---- When a step is in effect ----------------------------------------------------------------------------------

bool Player::active(const Step& step) const noexcept
{
    return step.when.operation == OperationName::any || operation_ == OperationName::any ||
           step.when.operation == operation_;
}

// How many times the step's trigger has occurred: the first time it is reached, and again each `every`, up to
// `times` in all.
std::uint64_t Player::occurrences(const Step& step, Instant now) const noexcept
{
    const When& when = step.when;
    std::uint64_t reached = 0;
    switch (when.kind)
    {
    case TriggerKind::request:
        reached = requests_;
        break;
    case TriggerKind::status_file:
        reached = status_files_;
        break;
    case TriggerKind::after_ms:
        if (!began_ || now < *began_)
        {
            return 0;
        }
        reached =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - *began_).count());
        break;
    }
    if (reached < when.value || (when.kind != TriggerKind::after_ms && when.value == 0))
    {
        return 0;
    }
    std::uint64_t count = 1;
    if (when.every != 0)
    {
        count += (reached - when.value) / when.every;
    }
    if (when.times != 0)
    {
        count = std::min(count, when.times);
    }
    return count;
}

// Whether a step that acts on datagrams can act on the next one that matches.
bool Player::eligible(std::size_t index, Instant now) const noexcept
{
    const Step& step = scenario_.steps[index];
    if (!acts_on_datagrams(step.fault.action) || !active(step))
    {
        return false;
    }
    const std::uint64_t count = occurrences(step, now);
    return count != 0 && (step.all || applied_[index] < count * step.count);
}

// The steps that are done to the operation and not to a datagram take effect the moment they are in effect.
void Player::arm_operation_steps(Instant now) noexcept
{
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        const Step& step = scenario_.steps[i];
        if (acts_on_datagrams(step.fault.action) || spent_[i] || !active(step) || occurrences(step, now) == 0)
        {
            continue;
        }
        if (!chance(i))
        {
            spent_[i] = true; // the draw said no, for the one occurrence there is
            continue;
        }
        if (step.fault.action == Action::silent)
        {
            silent_ = true;
            silent_until_ = step.fault.milliseconds > 0 ? now + milliseconds_of(step.fault.milliseconds) : never;
            report(i, now, nullptr, nullptr, step.fault.milliseconds);
        }
        else
        {
            abort_ = step.fault.status == 0x1004 ? adl::arinc615a::StatusCode::aborted_by_data_loader
                                                 : adl::arinc615a::StatusCode::aborted_by_operator;
            report(i, now, nullptr, nullptr, static_cast<std::int64_t>(step.fault.status));
        }
    }
}

std::optional<adl::arinc615a::StatusCode> Player::take_abort(Instant now) noexcept
{
    arm_operation_steps(now);
    const auto status = abort_;
    abort_.reset();
    return status;
}

Instant Player::next_wake() const noexcept
{
    Instant wake = never;
    for (const Held& entry : held_)
    {
        if (entry.used)
        {
            wake = adl::arinc615a::earlier_of(wake, entry.due);
        }
    }
    if (began_)
    {
        for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
        {
            const Step& step = scenario_.steps[i];
            if (!spent_[i] && !acts_on_datagrams(step.fault.action) && active(step) &&
                step.when.kind == TriggerKind::after_ms)
            {
                wake = adl::arinc615a::earlier_of(
                    wake, *began_ + milliseconds_of(static_cast<std::int64_t>(step.when.value)));
            }
        }
        if (silent_ && silent_until_ != never)
        {
            wake = adl::arinc615a::earlier_of(wake, silent_until_);
        }
    }
    return wake;
}

// ---- What the loader sends -------------------------------------------------------------------------------------

Player::Seen Player::classify(const Command& command) noexcept
{
    Seen seen;
    std::string_view named; // the file a request names, for the transfer that carries it
    if (const auto packet = adl::arinc615a::decode_packet(command.bytes))
    {
        if (const auto* request = std::get_if<adl::arinc615a::TftpRequest>(&*packet))
        {
            seen.kind = DatagramKind::request;
            named = request->file_name;
        }
        else if (const auto* data = std::get_if<adl::arinc615a::TftpData>(&*packet))
        {
            seen.kind = DatagramKind::data;
            seen.block = data->block;
        }
        else if (const auto* ack = std::get_if<adl::arinc615a::TftpAck>(&*packet))
        {
            seen.kind = DatagramKind::ack;
            seen.block = ack->block;
        }
        else if (std::holds_alternative<adl::arinc615a::TftpError>(*packet))
        {
            seen.kind = DatagramKind::error;
        }
        else if (const auto* oack = std::get_if<adl::arinc615a::TftpOptionAck>(&*packet))
        {
            seen.kind = DatagramKind::option_ack;
            for (const adl::arinc615a::TftpOption option : oack->options)
            {
                constexpr std::string_view prefix = "checksum_";
                if (option.name.size() > prefix.size() &&
                    equal_without_case(option.name.substr(0, prefix.size()), prefix) && !option.value.empty())
                {
                    const auto* begin = reinterpret_cast<const char*>(command.bytes.data());
                    seen.has_check_value = true;
                    seen.check_offset = static_cast<std::size_t>(option.value.data() - begin);
                    break;
                }
            }
        }
    }

    // Which file it is for, and whether it is the answer to a request.
    const bool from_slot = command.kind == Command::Kind::send_from_slot && command.slot < slot_capacity;
    for (Pending& pending : pending_)
    {
        if (pending.used && pending.peer == command.to)
        {
            seen.reply = true;
            pending.used = false;
            if (from_slot)
            {
                slot_file_[command.slot] = pending.file;
                slot_file_length_[command.slot] = pending.length;
            }
            else
            {
                // A reply from the control socket (an error, WAIT, ABORT) has no transfer to keep the name in.
                seen.file = std::string_view{pending.file.data(), pending.length};
            }
            break;
        }
    }
    if (from_slot)
    {
        if (!named.empty())
        {
            const std::size_t length = std::min(named.size(), slot_file_[command.slot].size());
            std::memcpy(slot_file_[command.slot].data(), named.data(), length);
            slot_file_length_[command.slot] = length;
        }
        seen.file = std::string_view{slot_file_[command.slot].data(), slot_file_length_[command.slot]};
    }
    return seen;
}

bool Player::matches(const Step& step, const Seen& seen) const noexcept
{
    const Selector& on = step.on;
    if (step.fault.action == Action::check_value)
    {
        if (!seen.has_check_value)
        {
            return false;
        }
    }
    else if (on.datagram == DatagramKind::reply)
    {
        if (!seen.reply)
        {
            return false;
        }
    }
    else if (on.datagram != DatagramKind::any && seen.kind != on.datagram)
    {
        return false;
    }
    if (on.block && (!seen.block || *seen.block != *on.block))
    {
        return false;
    }
    if (!on.file.empty())
    {
        if (on.file.front() == '.')
        {
            if (seen.file.size() <= on.file.size() ||
                !equal_without_case(seen.file.substr(seen.file.size() - on.file.size()), on.file))
            {
                return false;
            }
        }
        else if (!equal_without_case(seen.file, on.file))
        {
            return false;
        }
    }
    return true;
}

bool Player::hold(const Command& command, Instant due, bool reorder) noexcept
{
    if (command.bytes.size() > datagram_capacity)
    {
        return false;
    }
    for (std::size_t i = 0; i < held_.size(); ++i)
    {
        Held& entry = held_[i];
        if (!entry.used)
        {
            entry.used = true;
            entry.reorder = reorder;
            entry.meta = command;
            entry.due = due;
            entry.length = command.bytes.size();
            entry.order = ++held_order_;
            const std::span<std::byte> room = held_bytes(i);
            std::copy(command.bytes.begin(), command.bytes.end(), room.begin());
            return true;
        }
    }
    return false;
}

// Moves the held datagrams that are due, and, when `reorders`, those that were to follow the datagram just sent
// (the ones held at `before`, a count of what had been held when the call began), into `out` after the `used`
// commands already there, oldest first.
std::size_t Player::release(Instant now, bool reorders, std::uint64_t before, std::span<Command> out,
                            std::size_t used) noexcept
{
    for (;;)
    {
        if (used >= out.size())
        {
            break;
        }
        std::size_t chosen = held_.size();
        for (std::size_t i = 0; i < held_.size(); ++i)
        {
            const Held& entry = held_[i];
            const bool eligible_now =
                entry.used && (entry.due <= now || (reorders && entry.reorder && entry.order <= before));
            if (eligible_now && (chosen == held_.size() || entry.order < held_[chosen].order))
            {
                chosen = i;
            }
        }
        if (chosen == held_.size())
        {
            break;
        }
        Held& entry = held_[chosen];
        const std::span<std::byte> room = take_scratch(entry.length);
        if (room.empty() && entry.length != 0)
        {
            break; // no room to hand it over in: it goes with the next call
        }
        const std::span<const std::byte> stored = held_bytes(chosen).first(entry.length);
        std::copy(stored.begin(), stored.end(), room.begin());
        Command command = entry.meta;
        command.bytes = room;
        entry.used = false;
        if (!silent_)
        {
            out[used++] = command;
        }
    }
    return used;
}

std::size_t Player::on_timer(Instant now, std::span<Command> out) noexcept
{
    arm_operation_steps(now);
    if (silent_ && now >= silent_until_)
    {
        silent_ = false;
    }
    return release(now, false, 0, out, 0);
}

std::size_t Player::on_command(Instant now, const Command& command, std::span<Command> out) noexcept
{
    if (!began_)
    {
        began_ = now;
    }
    arm_operation_steps(now);
    if (out.empty())
    {
        return 0;
    }
    if (command.kind == Command::Kind::release_slot)
    {
        if (command.slot < slot_capacity)
        {
            slot_file_length_[command.slot] = 0;
        }
        // What was held for a transfer that has ended cannot be sent from its socket any more.
        for (Held& entry : held_)
        {
            if (entry.used && entry.meta.kind == Command::Kind::send_from_slot && entry.meta.slot == command.slot)
            {
                entry.used = false;
            }
        }
        out[0] = command;
        return 1;
    }

    const Seen seen = classify(command);
    if (silent_ && now >= silent_until_)
    {
        silent_ = false;
    }
    if (silent_)
    {
        return 0;
    }

    std::size_t count = 0;
    bool handled = false;
    const std::uint64_t held_before = held_order_;
    for (std::size_t i = 0; i < scenario_.steps.size() && !handled; ++i)
    {
        if (!eligible(i, now) || !matches(scenario_.steps[i], seen) || !chance(i))
        {
            continue;
        }
        count = apply(i, seen, command, now, out, handled);
    }
    if (!handled)
    {
        out[count++] = command;
    }
    // What was to go after this datagram goes now (not the datagram this call may just have held).
    return release(now, true, held_before, out, count);
}

std::size_t Player::apply(std::size_t index, const Seen& seen, const Command& command, Instant now,
                          std::span<Command> out, bool& handled) noexcept
{
    const Fault& fault = scenario_.steps[index].fault;
    Command changed = command;
    handled = true;
    switch (fault.action)
    {
    case Action::drop:
        report(index, now, &seen, &command, 0);
        return 0;
    case Action::duplicate:
    {
        const std::size_t copies = std::clamp<std::size_t>(fault.copies, 1, out.size());
        for (std::size_t i = 0; i < copies; ++i)
        {
            out[i] = command;
        }
        report(index, now, &seen, &command, static_cast<std::int64_t>(copies));
        return copies;
    }
    case Action::delay:
    case Action::reorder:
    {
        const bool reorder = fault.action == Action::reorder;
        std::int64_t wait = fault.milliseconds;
        if (!reorder && fault.min_milliseconds < fault.max_milliseconds)
        {
            wait = static_cast<std::int64_t>(draw(index, static_cast<std::uint64_t>(fault.min_milliseconds),
                                                  static_cast<std::uint64_t>(fault.max_milliseconds)));
        }
        else if (reorder && wait <= 0)
        {
            wait = default_reorder_hold_ms;
        }
        if (!hold(command, now + milliseconds_of(wait), reorder))
        {
            break;
        }
        report(index, now, &seen, &command, wait);
        return 0;
    }
    case Action::corrupt:
    case Action::check_value:
    {
        const std::span<std::byte> copy = take_scratch(command.bytes.size());
        if (copy.empty())
        {
            break;
        }
        std::copy(command.bytes.begin(), command.bytes.end(), copy.begin());
        if (fault.action == Action::corrupt)
        {
            const std::size_t position = std::min<std::size_t>(fault.offset, copy.size() - 1);
            copy[position] ^= static_cast<std::byte>(fault.xor_mask);
        }
        else
        {
            char* digit = reinterpret_cast<char*>(copy.data()) + seen.check_offset;
            *digit = flip_hex_digit(*digit);
        }
        changed.bytes = copy;
        out[0] = changed;
        report(index, now, &seen, &command,
               static_cast<std::int64_t>(fault.action == Action::corrupt ? fault.offset : 0));
        return 1;
    }
    case Action::length:
    {
        const std::int64_t wanted =
            std::max<std::int64_t>(0, static_cast<std::int64_t>(command.bytes.size()) + fault.delta);
        if (fault.delta <= 0)
        {
            changed.bytes = command.bytes.first(static_cast<std::size_t>(wanted));
        }
        else
        {
            const std::span<std::byte> copy = take_scratch(static_cast<std::size_t>(wanted));
            if (copy.empty())
            {
                break;
            }
            std::ranges::fill(copy, std::byte{0});
            std::copy(command.bytes.begin(), command.bytes.end(), copy.begin());
            changed.bytes = copy;
        }
        out[0] = changed;
        report(index, now, &seen, &command, fault.delta);
        return 1;
    }
    case Action::error:
    case Action::wait:
    case Action::abort_message:
    {
        std::array<char, 80> text{};
        std::size_t length = 0;
        auto code = adl::arinc615a::TftpErrorCode::not_defined;
        std::int64_t amount = 0;
        if (fault.action == Action::error)
        {
            code = static_cast<adl::arinc615a::TftpErrorCode>(fault.code);
            length = std::min(fault.text.size(), text.size());
            std::memcpy(text.data(), fault.text.data(), length);
            amount = fault.code;
        }
        else if (fault.action == Action::wait)
        {
            constexpr std::string_view prefix = "WAIT:";
            std::memcpy(text.data(), prefix.data(), prefix.size());
            length = prefix.size();
            std::array<char, 8> digits{};
            std::size_t digit_count = 0;
            unsigned value = fault.seconds;
            do
            {
                digits[digit_count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && digit_count < digits.size());
            while (digit_count != 0)
            {
                text[length++] = digits[--digit_count];
            }
            amount = fault.seconds;
        }
        else
        {
            constexpr std::string_view prefix = "ABORT:";
            std::memcpy(text.data(), prefix.data(), prefix.size());
            length = prefix.size();
            constexpr std::string_view hex = "0123456789ABCDEF";
            for (int shift = 12; shift >= 0; shift -= 4)
            {
                text[length++] = hex[(fault.status >> static_cast<unsigned>(shift)) & 0xFU];
            }
            amount = fault.status;
        }
        const std::span<std::byte> copy = take_scratch(datagram_capacity);
        if (copy.empty())
        {
            break;
        }
        const auto size =
            adl::arinc615a::encode_error(adl::arinc615a::TftpError{code, std::string_view{text.data(), length}}, copy);
        if (!size)
        {
            break;
        }
        changed.bytes = copy.first(*size);
        out[0] = changed;
        report(index, now, &seen, &command, amount);
        return 1;
    }
    case Action::abort:
    case Action::silent:
        break;
    }
    // The step could not be applied (a datagram too large to hold, no room): it goes as it was.
    handled = false;
    return 0;
}

} // namespace adl::loader_scenario
