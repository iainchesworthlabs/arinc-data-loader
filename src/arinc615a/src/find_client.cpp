#include "adl/arinc615a/find_client.hpp"

#include <algorithm>
#include <variant>

namespace adl::arinc615a {

void FindClient::start(Instant now) noexcept {
    count_ = 0;
    ignored_ = 0;
    request_size_ = encode_find_request(request_).value_or(0);
    end_ = now + config_.window;
    done_ = false;
}

void FindClient::on_datagram(Instant now, const Endpoint& from,
                             std::span<const std::byte> bytes) noexcept {
    if (done_) {
        return;
    }
    if (now > end_) {
        done_ = true;  // too late: the registration is closed (3-2)
        return;
    }
    const auto packet = decode_find(bytes);
    const auto* answer = packet ? std::get_if<FindAnswer>(&*packet) : nullptr;
    // Our own request, looped back, is a request and not an answer. An answer with no hardware
    // identifier names nothing to load.
    if (answer == nullptr || answer->target_hardware_id.empty()) {
        ++ignored_;
        return;
    }
    FindTarget target;
    target.from = from;
    target.hardware_id.set(answer->target_hardware_id);
    target.type_name.set(answer->target_type_name);
    target.position.set(answer->target_position);
    target.literal_name.set(answer->literal_name);
    target.manufacturer_code.set(answer->manufacturer_code);
    // A concentrator may answer more than once with different identities (3-5), so only a
    // repeat of the same answer from the same place is a duplicate.
    for (const FindTarget& known : targets()) {
        if (known.from.address == target.from.address && known.hardware_id == target.hardware_id &&
            known.type_name == target.type_name && known.position == target.position &&
            known.literal_name == target.literal_name &&
            known.manufacturer_code == target.manufacturer_code) {
            ++ignored_;
            return;
        }
    }
    if (count_ == targets_.size()) {
        ++ignored_;
        return;
    }
    targets_[count_++] = target;
}

void FindClient::on_timer(Instant now) noexcept {
    if (!done_ && now >= end_) {
        done_ = true;
    }
}

}  // namespace adl::arinc615a
