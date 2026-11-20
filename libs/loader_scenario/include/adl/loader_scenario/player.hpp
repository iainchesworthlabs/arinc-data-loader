// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The Player carries a scenario out: it is the engine's FaultHook, and it is the only place where a fault is
// decided. It watches what the target sends (to count requests and status files) and every command the loader
// makes, and for each datagram the loader means to send it finds the first step that is in effect and matches,
// and does what the step says: drops, repeats, delays, holds back, damages or replaces the datagram, or
// silences the loader, or asks for its abort. It tells its listener each time, with the step and what was done.
//
// It allocates when it is made (the room it needs to hold datagrams) and never in a call. It is for one
// operation and is not reused; the loader that uses it holds a pointer, so it must outlive the loader.
//
// Time is the engine's: a step with a time trigger counts from the loader's first command. Everything that is
// drawn at random is drawn from one generator seeded by the scenario's seed, in the order the datagrams are
// sent, so the same scenario against the same target gives the same history.

#include "adl/arinc615a/fault_hook.hpp"
#include "adl/loader_scenario/scenario.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace adl::loader_scenario
{

// A fault that was applied, as it is reported. The views are valid during the call only.
struct FaultEvent
{
    std::size_t step = 0; // the step's place in the scenario's list, from 0
    Action action = Action::drop;
    adl::arinc615a::Instant at;
    // The datagram that was acted on, for the actions that act on one.
    std::optional<DatagramKind> datagram; // request, option_ack, ack, data or error
    std::string_view file;                // the file its transfer is for, or empty
    std::optional<std::uint16_t> block;   // a data or ack datagram's
    std::optional<adl::arinc615a::Endpoint> to;
    // The action's parameters as they were applied: how many copies, how long (milliseconds), the change in
    // length, the code or status, whichever the action has.
    std::int64_t amount = 0;
};

class FaultListener
{
public:
    virtual ~FaultListener() = default;
    virtual void on_fault(const FaultEvent& event) noexcept = 0;
};

class Player final : public adl::arinc615a::FaultHook
{
public:
    // The largest datagram that is held back or changed: the loader's largest block and its header, with room.
    // A datagram that is longer is sent as it is and the step is not applied.
    static constexpr std::size_t datagram_capacity = 8192;
    // How many datagrams can be held back at once, and be changed in one call into the loader.
    static constexpr std::size_t held_capacity = 64;
    static constexpr std::size_t scratch_capacity = 64;

    explicit Player(Scenario scenario, FaultListener* listener = nullptr);
    ~Player() override;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    // The FaultHook.
    void begin_call(adl::arinc615a::Instant now) noexcept override;
    void on_arrival(adl::arinc615a::Instant now, std::optional<std::size_t> slot, const adl::arinc615a::Endpoint& from,
                    std::span<const std::byte> bytes) noexcept override;
    [[nodiscard]] std::size_t on_command(adl::arinc615a::Instant now, const adl::arinc615a::Command& command,
                                         std::span<adl::arinc615a::Command> out) noexcept override;
    [[nodiscard]] std::size_t on_timer(adl::arinc615a::Instant now,
                                       std::span<adl::arinc615a::Command> out) noexcept override;
    [[nodiscard]] adl::arinc615a::Instant next_wake() const noexcept override;
    [[nodiscard]] std::optional<adl::arinc615a::StatusCode> take_abort(adl::arinc615a::Instant now) noexcept override;

    [[nodiscard]] const Scenario& scenario() const noexcept
    {
        return scenario_;
    }
    // How many times a step has been applied (an abort or a silence once).
    [[nodiscard]] std::size_t applied(std::size_t step) const noexcept;
    [[nodiscard]] std::size_t applied() const noexcept;
    // The steps that were never applied, in order: the datagram they waited for did not come, or a trigger was
    // not reached.
    [[nodiscard]] std::vector<std::size_t> unapplied_steps() const;
    // A datagram that is still held back, which will not be sent if the operation has ended.
    [[nodiscard]] std::size_t held() const noexcept;

private:
    struct Seen; // what a datagram is, for the matching
    struct Pending
    {
        adl::arinc615a::Endpoint peer;
        std::array<char, 256> file{};
        std::size_t length = 0;
        bool used = false;
    };
    struct Held
    {
        bool used = false;
        bool reorder = false; // sent after the next datagram, or when due
        adl::arinc615a::Command meta;
        adl::arinc615a::Instant due;
        std::size_t length = 0;
        std::uint64_t order = 0;
    };

    [[nodiscard]] Seen classify(const adl::arinc615a::Command& command) noexcept;
    void remember_request(const adl::arinc615a::Endpoint& from, std::string_view file) noexcept;
    [[nodiscard]] bool armed(const Step& step, adl::arinc615a::Instant now) const noexcept;
    void arm_operation_steps(adl::arinc615a::Instant now) noexcept;
    [[nodiscard]] bool matches(const Step& step, const Seen& seen) const noexcept;
    [[nodiscard]] std::span<std::byte> take_scratch(std::size_t bytes) noexcept;
    [[nodiscard]] std::span<std::byte> held_bytes(std::size_t index) noexcept;
    [[nodiscard]] bool hold(const adl::arinc615a::Command& command, adl::arinc615a::Instant due, bool reorder) noexcept;
    [[nodiscard]] std::size_t release(adl::arinc615a::Instant now, bool reorders, std::uint64_t before,
                                      std::span<adl::arinc615a::Command> out, std::size_t used) noexcept;
    [[nodiscard]] std::size_t apply(std::size_t index, const Seen& seen, const adl::arinc615a::Command& command,
                                    adl::arinc615a::Instant now, std::span<adl::arinc615a::Command> out,
                                    bool& done) noexcept;
    void report(std::size_t step, adl::arinc615a::Instant now, const Seen* seen, const adl::arinc615a::Command* command,
                std::int64_t amount) noexcept;
    void spend(std::size_t step) noexcept;
    [[nodiscard]] std::uint64_t next_random() noexcept;

    Scenario scenario_;
    FaultListener* listener_;
    std::vector<std::size_t> applied_;
    std::vector<bool> spent_;
    std::vector<std::byte> scratch_;
    std::size_t scratch_used_ = 0;
    std::vector<std::byte> held_storage_;
    std::array<Held, held_capacity> held_{};
    std::uint64_t held_order_ = 0;

    std::uint64_t random_state_;
    std::optional<adl::arinc615a::Instant> began_;
    std::size_t requests_ = 0;
    std::size_t status_files_ = 0;

    // Where the datagrams of each transfer are going, and which file each is for.
    static constexpr std::size_t pending_capacity = 16;
    std::array<Pending, pending_capacity> pending_{};
    std::size_t next_pending_ = 0;
    static constexpr std::size_t slot_capacity = 8;
    std::array<std::array<char, 256>, slot_capacity> slot_file_{};
    std::array<std::size_t, slot_capacity> slot_file_length_{};

    // The operation-wide faults.
    bool silent_ = false;
    adl::arinc615a::Instant silent_until_{};
    std::optional<adl::arinc615a::StatusCode> abort_;
    std::optional<std::size_t> abort_step_;
};

} // namespace adl::loader_scenario
