// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A loader that is a script, for what the loader's engine cannot be made to do: say ABORT with a code of its own,
// answer nothing for good, or never acknowledge the initialization file. It reads one initialization file, naming
// its own port in the request (the "port" option) so that the target writes its status files there, and then does
// what it was given to do with them. It is only the loader's side of the datagrams: whoever uses it (a fake network
// agent, a socket on the loopback address) brings the datagrams to react() and sends back what it returns.

#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "adl/arinc615a_mini/mini_tftp.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace adl::test
{

class LoaderScript
{
public:
    enum class Then
    {
        abort_the_first_status_file, // ABORT with `abort_code` instead of the first status file's answer, then normal
        say_nothing,                 // acknowledge the initialization file, and answer nothing after it
        acknowledge_nothing,         // take the initialization file's option acknowledgement, and no block of it
    };

    LoaderScript(Then then, std::uint16_t own_port,
                 adl::arinc615a::StatusCode abort_code = adl::arinc615a::StatusCode::aborted_by_data_loader)
        : then_(then), own_port_(own_port), abort_code_(abort_code)
    {
    }

    // The request for an initialization file, with this loader's port in it.
    [[nodiscard]] adl::arinc615a_mini::Bytes request(std::string_view file) const
    {
        adl::arinc615a::OptionBuilder options;
        static_cast<void>(options.add_port(own_port_));
        return adl::arinc615a_mini::mini_encode(
            adl::arinc615a::TftpRequest{adl::arinc615a::TftpRequest::Kind::read, file, options.list()});
    }

    // What this loader sends back for a datagram that came to it from the target, or nothing.
    [[nodiscard]] std::optional<adl::arinc615a_mini::Bytes> react(std::span<const std::byte> payload)
    {
        using namespace adl::arinc615a;
        const auto packet = decode_packet(payload);
        if (!packet)
        {
            return std::nullopt;
        }
        if (std::holds_alternative<TftpOptionAck>(*packet))
        {
            return adl::arinc615a_mini::mini_encode(TftpAck{0}); // the initialization file's options are taken
        }
        if (const auto* data = std::get_if<TftpData>(&*packet))
        {
            if (!init_file_done_)
            {
                if (then_ == Then::acknowledge_nothing)
                {
                    return std::nullopt;
                }
                init_file_done_ = true;
                return adl::arinc615a_mini::mini_encode(TftpAck{data->block});
            }
            if (then_ == Then::say_nothing)
            {
                return std::nullopt;
            }
            current_.insert(current_.end(), data->data.begin(), data->data.end());
            if (data->data.size() < 512)
            {
                status_files_.push_back(std::move(current_));
                current_.clear();
            }
            return adl::arinc615a_mini::mini_encode(TftpAck{data->block});
        }
        if (std::holds_alternative<TftpRequest>(*packet)) // a status file the target wants to write
        {
            ++status_requests_;
            if (then_ == Then::say_nothing)
            {
                return std::nullopt;
            }
            if (then_ == Then::abort_the_first_status_file && status_requests_ == 1)
            {
                std::array<std::byte, 32> buffer{};
                const auto size = encode_abort(AbortMessage{abort_code_}, buffer);
                return size ? std::optional<adl::arinc615a_mini::Bytes>{adl::arinc615a_mini::Bytes(
                                  buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*size))}
                            : std::nullopt;
            }
            return adl::arinc615a_mini::mini_encode(TftpAck{0});
        }
        return std::nullopt;
    }

    // The status files the target wrote, whole, in the order they came, and how many it asked to write.
    [[nodiscard]] const std::vector<adl::arinc615a_mini::Bytes>& status_files() const noexcept
    {
        return status_files_;
    }
    [[nodiscard]] std::size_t status_requests() const noexcept
    {
        return status_requests_;
    }

private:
    Then then_;
    std::uint16_t own_port_;
    adl::arinc615a::StatusCode abort_code_;
    bool init_file_done_ = false;
    std::size_t status_requests_ = 0;
    adl::arinc615a_mini::Bytes current_;
    std::vector<adl::arinc615a_mini::Bytes> status_files_;
};

} // namespace adl::test
