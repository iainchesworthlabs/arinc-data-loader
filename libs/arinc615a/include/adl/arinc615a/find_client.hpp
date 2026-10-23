#pragma once

// FIND, the data loader's side (Attachment 3): the loader broadcasts one request on UDP port
// 1001 and registers the valid answers that come back in the next three seconds; an invalid answer
// is ignored, and there are no errors (3-2). Like the other state machines it is given the
// time and the datagrams and holds no socket: the runner sends request() to each of the
// destinations (the limited broadcast address unless told otherwise; over AFDX, which has no
// broadcast, a unicast address for each target, Appendix E) from a socket bound to port
// 1001, and feeds the answers back.

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/error.hpp"
#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/time.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace adl::arinc615a
{

// A text of at most N characters, held in the object (a FIND answer's views die with its
// datagram).
template <std::size_t N>
class FixedText
{
public:
    void set(std::string_view text) noexcept
    {
        size_ = text.size() < N ? text.size() : N;
        for (std::size_t i = 0; i < size_; ++i)
        {
            chars_[i] = text[i];
        }
    }
    [[nodiscard]] std::string_view view() const noexcept
    {
        return {chars_.data(), size_};
    }
    friend bool operator==(const FixedText& a, const FixedText& b) noexcept
    {
        return a.view() == b.view();
    }

private:
    std::array<char, N> chars_{};
    std::size_t size_ = 0;
};

// A target that answered.
struct FindTarget
{
    Endpoint from; // where the answer came from: the target's address
    FixedText<find_max_hardware_id> hardware_id;
    FixedText<find_max_type_name> type_name;
    FixedText<find_max_position> position;
    FixedText<find_max_literal_name> literal_name;
    FixedText<find_max_manufacturer_code> manufacturer_code;

    friend bool operator==(const FindTarget&, const FindTarget&) noexcept = default;
};

struct FindConfig
{
    Duration window = find_window;
    // Where the request goes. Empty means the limited broadcast address, 255.255.255.255.
    std::span<const Address> destinations;
};

class FindClient
{
public:
    static constexpr std::size_t max_targets = 64;

    explicit FindClient(const FindConfig& config) noexcept : config_(config) {}

    // Opens the window. After it the runner sends request() to destinations().
    void start(Instant now) noexcept;

    [[nodiscard]] std::span<const std::byte> request() const noexcept
    {
        return std::span<const std::byte>{request_}.first(request_size_);
    }
    // Every address the request is to be sent to, port 1001.
    [[nodiscard]] std::span<const Address> destinations() const noexcept
    {
        return config_.destinations.empty() ? std::span<const Address>{&broadcast_, 1} : config_.destinations;
    }

    // An answer (or anything else) arrived on the FIND socket. What is not a valid answer, or
    // arrives after the window, is ignored; a repeat of an answer already registered is too.
    void on_datagram(Instant now, const Endpoint& from, std::span<const std::byte> bytes) noexcept;
    void on_timer(Instant now) noexcept;

    [[nodiscard]] Instant next_wake() const noexcept
    {
        return done_ ? never : end_;
    }
    [[nodiscard]] bool done() const noexcept
    {
        return done_;
    }

    [[nodiscard]] std::span<const FindTarget> targets() const noexcept
    {
        return std::span<const FindTarget>{targets_}.first(count_);
    }
    // Datagrams ignored as not valid answers, as repeats, or for want of room.
    [[nodiscard]] std::size_t ignored() const noexcept
    {
        return ignored_;
    }

private:
    FindConfig config_;
    Address broadcast_ = Address::broadcast();
    std::array<std::byte, 8> request_{};
    std::size_t request_size_ = 0;
    Instant end_ = never;
    bool done_ = true;
    std::array<FindTarget, max_targets> targets_{};
    std::size_t count_ = 0;
    std::size_t ignored_ = 0;
};

} // namespace adl::arinc615a
