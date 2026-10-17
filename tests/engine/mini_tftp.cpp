#include "mini_tftp.hpp"

#include <algorithm>
#include <variant>

#include "adl/arinc615a/limits.hpp"

namespace adl::test {

using namespace adl::arinc615a;

Bytes mini_encode(const TftpPacket& packet) {
    const auto size = encoded_size(packet);
    Bytes bytes(size.value_or(0));
    if (size) {
        static_cast<void>(encode_packet(packet, bytes));
    }
    return bytes;
}

void MiniTransfer::transmit(Bytes packet) {
    last_packet_ = std::move(packet);
    tries_ = 1;
    ++generation_;
    ++sent_;
    host_.send(port_, peer_, last_packet_);
    host_.after(tftp_timeout, [this, generation = generation_] { on_timeout(generation); });
}

void MiniTransfer::on_timeout(std::uint64_t generation) {
    if (generation != generation_ || !running()) {
        return;
    }
    if (tries_ <= tftp_retries) {
        ++tries_;
        ++generation_;
        ++sent_;
        host_.send(port_, peer_, last_packet_);
        host_.after(tftp_timeout, [this, generation = generation_] { on_timeout(generation); });
        return;
    }
    finish(MiniResult::Kind::timed_out);
}

void MiniTransfer::finish(MiniResult::Kind kind) {
    if (!running()) {
        return;
    }
    result_.kind = kind;
    ++generation_;  // no time-out of ours is still wanted
    if (!done_called_ && on_done) {
        done_called_ = true;
        on_done(*this);
    }
}

void MiniTransfer::start_client(const Endpoint& server, Request request) {
    peer_ = server;
    name_ = request.file_name;
    awaiting_first_ = true;
    asked_block_size_ = request.block_size;
    OptionBuilder options;
    if (request.block_size != 0) {
        static_cast<void>(options.add_block_size(static_cast<std::uint16_t>(request.block_size)));
    }
    if (role_ == Role::client_write) {
        content_ = std::move(request.content);
        static_cast<void>(options.add_transfer_size(content_.size()));
    } else {
        static_cast<void>(options.add_transfer_size(0));
    }
    if (request.checksum) {  // on a read it names the file, on a write it checks it
        checksum_asked_ = true;
        static_cast<void>(options.add_checksum(*request.checksum));
    }
    transmit(mini_encode(TftpRequest{role_ == Role::client_write ? TftpRequest::Kind::write
                                                                  : TftpRequest::Kind::read,
                                      name_, options.list()}));
}

void MiniTransfer::serve(const Endpoint& client, const TftpRequest& request, Bytes content,
                         std::size_t block_size_cap, bool echo_port) {
    peer_ = client;
    peer_known_ = true;
    name_ = std::string{request.file_name};
    const auto options = read_transfer_options(request.options);
    OptionBuilder reply;
    if (options && options->block_size) {
        block_size_ = std::min<std::size_t>(*options->block_size, block_size_cap);
        static_cast<void>(reply.add_block_size(static_cast<std::uint16_t>(block_size_)));
    }
    if (echo_port && options && options->port) {
        static_cast<void>(reply.add_port(*options->port));
    }
    if (role_ == Role::server_read) {
        content_ = std::move(content);
        if (options && options->transfer_size) {
            static_cast<void>(reply.add_transfer_size(content_.size()));
        }
    } else if (options && options->transfer_size) {
        announced_ = *options->transfer_size;
        static_cast<void>(reply.add_transfer_size(*options->transfer_size));
    }
    if (reply.count() != 0) {
        awaiting_first_ = true;
        transmit(mini_encode(TftpOptionAck{reply.list()}));
        return;
    }
    if (role_ == Role::server_read) {
        send_data();
    } else {
        block_ = 0;
        send_ack(0);
    }
}

void MiniTransfer::send_ack(std::uint16_t block) {
    transmit(mini_encode(TftpAck{block}));
}

void MiniTransfer::send_data() {
    const std::uint16_t block = next_block(block_);
    const std::size_t start = static_cast<std::size_t>(offset_);
    const std::size_t count = std::min(block_size_, content_.size() - start);
    block_ = block;
    offset_ += count;
    last_sent_ = count < block_size_;
    transmit(mini_encode(
        TftpData{block, std::span<const std::byte>{content_}.subspan(start, count)}));
}

void MiniTransfer::accept_error(const TftpError& error) {
    result_.error_text = std::string{error.message};
    const auto meaning = interpret_error(error);
    if (meaning) {
        if (const auto* wait = std::get_if<WaitMessage>(&*meaning)) {
            result_.wait_seconds = wait->seconds;
            finish(MiniResult::Kind::waited);
            return;
        }
        if (const auto* abort_message = std::get_if<AbortMessage>(&*meaning)) {
            result_.abort_status = abort_message->status;
            finish(MiniResult::Kind::aborted);
            return;
        }
    }
    result_.error_code = error.code;
    finish(MiniResult::Kind::error);
}

void MiniTransfer::receive(const TftpData& data) {
    if (data.block == block_ && block_ != 0) {
        send_ack(block_);  // our ACK was lost: say it again
        return;
    }
    if (data.block != next_block(block_) || data.data.size() > block_size_) {
        return;
    }
    received_.insert(received_.end(), data.data.begin(), data.data.end());
    block_ = data.block;
    awaiting_first_ = false;
    send_ack(block_);
    if (data.data.size() < block_size_) {
        finish(MiniResult::Kind::ok);
    }
}

void MiniTransfer::on_datagram(const Endpoint& from, std::span<const std::byte> payload) {
    if (from.address != peer_.address) {
        return;
    }
    if (peer_known_ && from.port != peer_.port) {
        return;
    }
    const auto packet = decode_packet(payload);
    if (!packet) {
        return;
    }
    // A finished receiver goes on answering a repeat of the last block, for the loader whose
    // ACK went missing; it is the only thing a finished transfer does.
    if (!running()) {
        if (role_ == Role::server_write || role_ == Role::client_read) {
            if (const auto* data = std::get_if<TftpData>(&*packet)) {
                if (result_.kind == MiniResult::Kind::ok && data->block == block_) {
                    send_ack(block_);
                }
            }
        }
        return;
    }
    if (const auto* error = std::get_if<TftpError>(&*packet)) {
        peer_ = from;
        peer_known_ = true;
        accept_error(*error);
        return;
    }
    switch (role_) {
        case Role::client_write: {
            if (awaiting_first_) {
                if (const auto* oack = std::get_if<TftpOptionAck>(&*packet)) {
                    peer_ = from;
                    peer_known_ = true;
                    const auto options = read_transfer_options(oack->options);
                    if (options && options->block_size) {
                        block_size_ = *options->block_size;
                    }
                    checksum_acknowledged_ = options && options->checksum.has_value();
                } else if (const auto* ack = std::get_if<TftpAck>(&*packet); ack && ack->block == 0) {
                    peer_ = from;
                    peer_known_ = true;
                    block_size_ = tftp_default_block_size;
                } else {
                    return;
                }
                awaiting_first_ = false;
                send_data();
                return;
            }
            if (const auto* ack = std::get_if<TftpAck>(&*packet); ack && ack->block == block_) {
                if (last_sent_) {
                    finish(MiniResult::Kind::ok);
                } else {
                    send_data();
                }
            }
            return;
        }
        case Role::client_read: {
            if (awaiting_first_) {
                if (const auto* oack = std::get_if<TftpOptionAck>(&*packet)) {
                    peer_ = from;
                    peer_known_ = true;
                    const auto options = read_transfer_options(oack->options);
                    if (options && options->block_size) {
                        block_size_ = *options->block_size;
                    }
                    if (options && options->transfer_size) {
                        announced_ = *options->transfer_size;
                    }
                    block_ = 0;
                    awaiting_first_ = false;
                    send_ack(0);
                    return;
                }
                if (const auto* data = std::get_if<TftpData>(&*packet); data && data->block == 1) {
                    peer_ = from;
                    peer_known_ = true;
                    block_size_ = tftp_default_block_size;
                    block_ = 0;
                    awaiting_first_ = false;
                    receive(*data);
                }
                return;
            }
            if (const auto* data = std::get_if<TftpData>(&*packet)) {
                receive(*data);
            }
            return;
        }
        case Role::server_read: {
            if (const auto* ack = std::get_if<TftpAck>(&*packet)) {
                if (awaiting_first_ && ack->block == 0) {
                    awaiting_first_ = false;
                    send_data();
                } else if (!awaiting_first_ && ack->block == block_) {
                    if (last_sent_) {
                        finish(MiniResult::Kind::ok);
                    } else {
                        send_data();
                    }
                }
            }
            return;
        }
        case Role::server_write: {
            if (const auto* data = std::get_if<TftpData>(&*packet)) {
                receive(*data);
            }
            return;
        }
    }
}

}  // namespace adl::test
