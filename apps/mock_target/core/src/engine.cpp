#include "adl/mock/engine.hpp"

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/header_file.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <variant>

namespace adl::mock
{

using namespace adl::arinc615a;

namespace
{

constexpr std::size_t serve_block_size_cap = 1024;

[[nodiscard]] std::string seconds_text(Instant at)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.3f", std::chrono::duration<double>(at.since_origin).count());
    return buffer;
}

[[nodiscard]] bool is_final_status(StatusCode code) noexcept
{
    return code == StatusCode::completed || code == StatusCode::aborted_by_target ||
           code == StatusCode::aborted_by_data_loader || code == StatusCode::aborted_by_operator;
}

[[nodiscard]] std::vector<HardwareItem> default_information()
{
    std::vector<HardwareItem> items;
    items.push_back(HardwareItem{
        "FMS LEFT", "SN-0001", {{"ACM47-1234-5678", "", "FMS 1 OPS"}, {"ACM47-8765-4321", "A", "FMS 1 DATA"}}});
    return items;
}

} // namespace

MockEngine::MockEngine(EngineListener* listener) : listener_(listener) {}

MockEngine::~MockEngine()
{
    *alive_ = false;
}

std::optional<std::string> MockEngine::start(TargetLink& link, Config config)
{
    link_ = &link;
    config_ = std::move(config);
    last_error_.clear();
    std::string error;
    link_->set_local_address(config_.bind_address);
    if (!link_->bind(config_.find_port, error))
    {
        last_error_ = "the FIND port could not be bound: " + error;
        return last_error_;
    }
    if (!link_->bind(config_.tftp_port, error))
    {
        last_error_ = "the TFTP port could not be bound: " + error;
        return last_error_;
    }
    note(Severity::info, "listening: FIND on port " + std::to_string(config_.find_port) + ", TFTP on port " +
                             std::to_string(config_.tftp_port));
    return std::nullopt;
}

Instant MockEngine::now() const
{
    return link_ != nullptr ? link_->now() : Instant{};
}

void MockEngine::note(Severity severity, std::string text)
{
    log_.push_back(seconds_text(now()) + " " + text);
    if (listener_ != nullptr)
    {
        listener_->log(severity, text);
    }
}

void MockEngine::report_progress()
{
    const int percent = operation_started_ ? static_cast<int>(progress_percent()) : -1;
    if (percent != reported_progress_)
    {
        reported_progress_ = percent;
        if (listener_ != nullptr)
        {
            listener_->progress_changed(percent);
        }
    }
}

std::string MockEngine::current_operation() const
{
    return operation_started_ ? operation_name(kind_) : std::string{};
}

std::string MockEngine::operation_name(Kind kind)
{
    switch (kind)
    {
    case Kind::information:
        return "Information";
    case Kind::upload:
        return "Upload";
    case Kind::download_media:
        return "Download (media defined)";
    case Kind::download_operator:
        return "Download (operator defined)";
    }
    return "unknown";
}

OperationKind MockEngine::operation_kind(Kind kind)
{
    switch (kind)
    {
    case Kind::information:
        return OperationKind::information;
    case Kind::upload:
        return OperationKind::upload;
    case Kind::download_media:
        return OperationKind::download_media;
    case Kind::download_operator:
        return OperationKind::download_operator;
    }
    return OperationKind::information;
}

// ---- MiniHost -------------------------------------------------------------------------------

void MockEngine::send(std::uint16_t from_port, const Endpoint& to, std::span<const std::byte> payload)
{
    if (silent_ || link_ == nullptr)
    {
        return;
    }
    link_->send(from_port, to, payload);
}

void MockEngine::after(adl::arinc615a::Duration delay, std::function<void()> action)
{
    if (link_ == nullptr)
    {
        return;
    }
    link_->after(delay,
                 [alive = std::weak_ptr<bool>(alive_), this, action = std::move(action)]() mutable
                 {
                     const auto live = alive.lock();
                     if (live && *live && !silent_)
                     {
                         action();
                     }
                 });
}

// ---- Datagrams ------------------------------------------------------------------------------

void MockEngine::run()
{
    std::array<std::byte, 4096> buffer{};
    while (link_ != nullptr && !link_->stop_requested())
    {
        const std::int32_t ready = link_->wait(200);
        if (ready < 0)
        {
            last_error_ = "waiting on the network failed";
            note(Severity::error, last_error_);
            return;
        }
        for (const std::uint16_t port : link_->ports())
        {
            for (;;)
            {
                Endpoint from;
                std::size_t size = 0;
                if (!link_->receive(port, from, buffer, size))
                {
                    break;
                }
                on_datagram(port, from, std::span<const std::byte>{buffer}.first(size));
            }
        }
    }
}

void MockEngine::on_datagram(std::uint16_t port, const Endpoint& from, std::span<const std::byte> payload)
{
    if (link_ == nullptr)
    {
        return;
    }
    if (port == config_.find_port && port != config_.tftp_port)
    {
        handle_find(from, payload);
        return;
    }
    if (port != config_.tftp_port)
    {
        if (const auto found = transfers_.find(port); found != transfers_.end())
        {
            found->second->on_datagram(from, payload);
        }
        return;
    }
    if (silent_)
    {
        // The target answers nothing while it is silent. A new request for an initialization
        // file wakes it for the next operation, so the demonstration recovers when the loader
        // (which has just hit its time-out) tries again.
        const auto packet = decode_packet(payload);
        if (packet)
        {
            if (const auto* request = std::get_if<TftpRequest>(&*packet);
                request != nullptr && request->kind == TftpRequest::Kind::read)
            {
                if (const auto kind = kind_of_file_name(request->file_name))
                {
                    switch (*kind)
                    {
                    case ProtocolFileKind::lci:
                    case ProtocolFileKind::lui:
                    case ProtocolFileKind::lnd:
                    case ProtocolFileKind::lno:
                        note(Severity::info, "wakes for a new operation");
                        reset_operation_state(); // clears the silence
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        if (silent_)
        {
            return;
        }
    }
    const auto packet = decode_packet(payload);
    if (!packet)
    {
        return;
    }
    if (const auto* request = std::get_if<TftpRequest>(&*packet))
    {
        handle_request(from, *request);
    }
}

void MockEngine::handle_find(const Endpoint& from, std::span<const std::byte> payload)
{
    const auto packet = decode_find(payload);
    if (!packet || !std::holds_alternative<FindRequest>(*packet))
    {
        return;
    }
    const FindAnswer answer = config_.find_answer;
    after(config_.find_delay,
          [this, from, answer]
          {
              std::array<std::byte, find_max_answer_size> buffer{};
              if (const auto size = encode_find_answer(answer, buffer))
              {
                  for (unsigned copy = 0; copy < config_.find_copies; ++copy)
                  {
                      send(config_.find_port, from, std::span<const std::byte>{buffer}.first(*size));
                  }
              }
              note(Severity::info, "answered FIND");
          });
}

void MockEngine::send_error(const Endpoint& to, TftpErrorCode code, std::string_view text)
{
    std::array<std::byte, 64> buffer{};
    if (const auto size = encode_error(TftpError{code, text}, buffer))
    {
        send(next_port_++, to, std::span<const std::byte>{buffer}.first(*size));
    }
}

adl::arinc615a_mini::MiniTransfer& MockEngine::add_transfer(MiniTransfer::Role role)
{
    const std::uint16_t port = next_port_++;
    auto transfer = std::make_unique<MiniTransfer>(static_cast<MiniHost&>(*this), port, role);
    MiniTransfer& reference = *transfer;
    transfers_[port] = std::move(transfer);
    return reference;
}

void MockEngine::remove_later(std::uint16_t port)
{
    after(std::chrono::seconds(5), [this, port] { transfers_.erase(port); });
}

void MockEngine::handle_request(const Endpoint& from, const TftpRequest& request)
{
    const std::string name{request.file_name};
    const auto kind = kind_of_file_name(name);
    const bool matches = kind && name.substr(0, name.size() - 4) == config_.thw;
    const bool read = request.kind == TftpRequest::Kind::read;
    if (matches && read)
    {
        switch (*kind)
        {
        case ProtocolFileKind::lci:
            serve_init(from, request, Kind::information);
            return;
        case ProtocolFileKind::lui:
            serve_init(from, request, Kind::upload);
            return;
        case ProtocolFileKind::lnd:
            serve_init(from, request, Kind::download_media);
            return;
        case ProtocolFileKind::lno:
            serve_init(from, request, Kind::download_operator);
            return;
        default:
            break;
        }
    }
    const bool is_list =
        matches && !read &&
        (*kind == ProtocolFileKind::lur || *kind == ProtocolFileKind::lnr || *kind == ProtocolFileKind::lna);
    if (is_list)
    {
        if (!operation_started_ || !first_status_sent_)
        {
            // 6.2.7.3: the list is written after the status file that says "accepted"; a
            // loader that writes it sooner has broken the synchronisation the standard asks
            // for.
            send_error(from, TftpErrorCode::illegal_operation, "list before the status");
            note(Severity::warning, "refuses a list that came before the first status file");
            return;
        }
        MiniTransfer& transfer = add_transfer(MiniTransfer::Role::server_write);
        transfer.on_done = [this, name](MiniTransfer& done)
        {
            if (done.result().kind == MiniResult::Kind::ok)
            {
                on_list_received(name, done.received());
            }
            remove_later(done.port());
        };
        transfer.serve(from, request, {}, serve_block_size_cap);
        return;
    }
    send_error(from, TftpErrorCode::file_not_found, "no such file");
}

// ---- The initialization file ------------------------------------------------------------------

void MockEngine::serve_init(const Endpoint& from, const TftpRequest& request, Kind kind)
{
    if (operation_started_)
    {
        return; // one operation at a time: a repeated request after the start is not answered
    }
    kind_ = kind;
    std::array<std::byte, 64> small{};
    if (config_.wait_first_seconds != 0 && (!waited_once_ || config_.wait_always))
    {
        waited_once_ = true;
        if (const auto size = encode_wait(WaitMessage{config_.wait_first_seconds}, small))
        {
            send(next_port_++, from, std::span<const std::byte>{small}.first(*size));
        }
        note(Severity::info, "answers the request with WAIT:" + std::to_string(config_.wait_first_seconds));
        return;
    }
    // Where the engine writes: the loader's control port, or the one its "port" option named
    // (the option is echoed in the OACK below, which is how the loader learns it was taken).
    loader_ = Endpoint{from.address, config_.tftp_port};
    if (const auto options = read_transfer_options(request.options); options && options->port)
    {
        loader_.port = *options->port;
    }

    InitializationFile init;
    init.status = StatusCode::accepted;
    if (config_.refusal && config_.refusal->kind == operation_kind(kind))
    {
        init.status = config_.refusal->code;
        init.description = config_.refusal->text;
    }
    Bytes file(512);
    const auto size = encode_initialization_file(init, file);
    file.resize(size.value_or(0));
    if (config_.garbage_init)
    {
        file.resize(file.size() > 2 ? file.size() - 2 : file.size()); // the length no longer agrees
    }
    if (config_.corrupt_served && !file.empty())
    {
        file[file.size() / 2] ^= std::byte{0x01};
    }
    MiniTransfer& transfer = add_transfer(MiniTransfer::Role::server_read);
    transfer.on_done = [this](MiniTransfer& done)
    {
        if (done.result().kind == MiniResult::Kind::ok)
        {
            on_init_served();
        }
        remove_later(done.port());
    };
    transfer.serve(from, request, std::move(file), serve_block_size_cap, true);
}

void MockEngine::on_init_served()
{
    const std::string name =
        config_.thw + std::string{extension(kind_ == Kind::information      ? ProtocolFileKind::lci
                                            : kind_ == Kind::upload         ? ProtocolFileKind::lui
                                            : kind_ == Kind::download_media ? ProtocolFileKind::lnd
                                                                            : ProtocolFileKind::lno)};
    note(Severity::info, "served " + name);
    const StatusCode effective = config_.refusal && config_.refusal->kind == operation_kind(kind_)
                                     ? config_.refusal->code
                                     : StatusCode::accepted;
    if (effective != StatusCode::accepted || config_.garbage_init)
    {
        note(Severity::warning, operation_name(kind_) + " refused");
        return;
    }
    operation_started_ = true;
    note(Severity::info, operation_name(kind_) + " operation started");
    if (listener_ != nullptr)
    {
        listener_->operation_changed(operation_name(kind_));
    }
    report_progress();
    after(config_.think_time, [this] { start_work(); });
}

// ---- Client side: writes and reads ------------------------------------------------------------

std::optional<adl::arinc615a::ChecksumOption> MockEngine::checksum_for(const Bytes& content) const
{
    const auto value = adl::arinc665::compute_check_value(adl::arinc665::CheckValueType::crc16, content);
    if (!value || value->bytes.size() != 2)
    {
        return std::nullopt;
    }
    ChecksumOption option;
    option.type = adl::arinc665::CheckValueType::crc16;
    option.length = 2;
    std::copy(value->bytes.begin(), value->bytes.end(), option.value.begin());
    if (config_.wrong_checksum)
    {
        option.value[0] ^= std::byte{0x01};
    }
    return option;
}

std::optional<adl::arinc615a::ChecksumOption>
MockEngine::checksum_for_file(const adl::arinc665::UserSuppliedFile& file) const
{
    ChecksumOption option;
    if (file.check_value && file.check_value->type != adl::arinc665::CheckValueType::none &&
        !file.check_value->bytes.empty())
    {
        option.type = file.check_value->type;
        option.length =
            static_cast<std::uint8_t>(std::min<std::size_t>(file.check_value->bytes.size(), option.value.size()));
        std::copy_n(file.check_value->bytes.begin(), option.length, option.value.begin());
        return option;
    }
    // The header file's own 16-bit CRC of the whole file (2.2.3.1.22).
    option.type = adl::arinc665::CheckValueType::crc16;
    option.length = 2;
    option.value[0] = static_cast<std::byte>(file.crc >> 8);
    option.value[1] = static_cast<std::byte>(file.crc & 0xFF);
    return option;
}

void MockEngine::write_file(const std::string& name, Bytes content, std::optional<ChecksumOption> checksum,
                            std::function<void(MiniResult::Kind)> then, unsigned attempt)
{
    MiniTransfer& transfer = add_transfer(MiniTransfer::Role::client_write);
    transfer.on_done = [this, name, content, checksum, then, attempt](MiniTransfer& done) mutable
    {
        const MiniResult result = done.result();
        checksum_acknowledged_ = checksum_acknowledged_ || done.checksum_acknowledged();
        remove_later(done.port());
        switch (result.kind)
        {
        case MiniResult::Kind::ok:
            then(MiniResult::Kind::ok);
            return;
        case MiniResult::Kind::waited:
            note(Severity::notice, "the loader asked to wait " + std::to_string(result.wait_seconds) + "s for " + name);
            after(std::chrono::seconds(result.wait_seconds), [this, name, content, checksum, then, attempt]() mutable
                  { write_file(name, content, checksum, then, attempt); });
            return;
        case MiniResult::Kind::aborted:
            last_abort_status_ = result.abort_status;
            then(MiniResult::Kind::aborted);
            return;
        default:
            if (attempt < tftp_retries)
            {
                write_file(name, content, checksum, then, attempt + 1);
            }
            else
            {
                then(result.kind);
            }
            return;
        }
    };
    MiniTransfer::Request request;
    request.file_name = name;
    request.content = std::move(content);
    request.block_size = config_.request_block_size;
    request.checksum = checksum;
    transfer.start_client(loader_, std::move(request));
}

void MockEngine::read_file(const std::string& name, std::optional<ChecksumOption> checksum,
                           std::function<void(MiniResult::Kind, const Bytes&)> then, unsigned attempt)
{
    MiniTransfer& transfer = add_transfer(MiniTransfer::Role::client_read);
    transfer.on_done = [this, name, checksum, then, attempt](MiniTransfer& done)
    {
        const MiniResult result = done.result();
        const Bytes received = done.received();
        remove_later(done.port());
        switch (result.kind)
        {
        case MiniResult::Kind::ok:
            then(MiniResult::Kind::ok, received);
            return;
        case MiniResult::Kind::waited:
            note(Severity::notice, "the loader asked to wait " + std::to_string(result.wait_seconds) + "s for " + name);
            after(std::chrono::seconds(result.wait_seconds),
                  [this, name, checksum, then, attempt] { read_file(name, checksum, then, attempt); });
            return;
        case MiniResult::Kind::aborted:
            last_abort_status_ = result.abort_status;
            then(MiniResult::Kind::aborted, {});
            return;
        default:
            if (attempt < tftp_retries)
            {
                read_file(name, checksum, then, attempt + 1);
            }
            else
            {
                then(result.kind, {});
            }
            return;
        }
    };
    MiniTransfer::Request request;
    request.file_name = name;
    request.block_size = config_.request_block_size;
    request.checksum = checksum;
    transfer.start_client(loader_, std::move(request));
}

// ---- Status files -----------------------------------------------------------------------------

std::uint32_t MockEngine::progress_percent() const
{
    switch (kind_)
    {
    case Kind::upload:
        return loads_.empty() ? 0 : static_cast<std::uint32_t>(loads_done_ * 100 / loads_.size());
    case Kind::download_media:
    case Kind::download_operator:
        return writes_total_ == 0 ? 0 : static_cast<std::uint32_t>(writes_done_ * 100 / writes_total_);
    case Kind::information:
        return 0;
    }
    return 0;
}

void MockEngine::post_status(StatusSpec spec)
{
    status_queue_.push_back(std::move(spec));
    pump_status();
}

void MockEngine::pump_status()
{
    if (status_busy_ || status_queue_.empty() || silent_)
    {
        return;
    }
    if (quiet_until_set_ && now() < quiet_until_)
    {
        after(quiet_until_ - now(), [this] { pump_status(); });
        return;
    }
    quiet_until_set_ = false;
    status_busy_ = true;
    first_status_sent_ = true; // the status lane has begun, whatever happens to this file
    StatusSpec spec = std::move(status_queue_.front());
    status_queue_.erase(status_queue_.begin());

    if (config_.abort_after_statuses && status_index_ == *config_.abort_after_statuses)
    {
        spec.code = StatusCode::aborted_by_target;
        spec.text = "the mock target gave up";
        status_queue_.clear();
        heartbeat_on_ = false;
        final_posted_ = true;
    }
    const bool final_status = is_final_status(spec.code);
    // Never the first status file (the one that says "accepted" synchronises the two ends,
    // and without it the loader waits for a list that never comes) and never a final one.
    if (config_.drop_status_every != 0 && status_index_ != 0 && status_index_ % config_.drop_status_every == 0 &&
        !final_status)
    {
        ++counter_; // as if a status file had been lost on its way
        const std::size_t index = status_index_++;
        note(Severity::warning, "status " + std::to_string(index) + " lost (injection)");
        status_busy_ = false;
        pump_status();
        return;
    }
    const std::uint16_t counter = counter_++;
    const std::size_t index = status_index_++;
    const Bytes bytes = build_status(spec, counter);
    const ProtocolFileKind status_kind = kind_ == Kind::information ? ProtocolFileKind::lcs
                                         : kind_ == Kind::upload    ? ProtocolFileKind::lus
                                                                    : ProtocolFileKind::lns;
    const std::string name = config_.thw + std::string{extension(status_kind)};
    write_file(name, bytes, std::nullopt,
               [this, counter, index, final_status, spec](MiniResult::Kind kind)
               {
                   status_busy_ = false;
                   if (kind == MiniResult::Kind::ok)
                   {
                       note(Severity::info, "status " + std::to_string(counter) + " delivered");
                       if (final_status)
                       {
                           finished_ = true;
                           note(Severity::info,
                                operation_name(kind_) + " operation finished: " + std::string{to_string(spec.code)});
                           reset_operation_state();
                       }
                   }
                   else if (kind == MiniResult::Kind::aborted)
                   {
                       note(Severity::warning, "status " + std::to_string(index) + " refused with ABORT");
                       on_abort_seen(last_abort_status_);
                   }
                   else
                   {
                       note(Severity::warning, "status " + std::to_string(counter) + " failed");
                       if (final_status)
                       {
                           finished_ = true;
                           reset_operation_state();
                       }
                   }
                   pump_status();
               });
}

Bytes MockEngine::build_status(const StatusSpec& spec, std::uint16_t counter)
{
    const bool progressing = spec.code == StatusCode::in_progress || spec.code == StatusCode::in_progress_with_text;
    const std::uint16_t estimated = progressing ? estimated_time_unknown : std::uint16_t{0};
    const std::uint16_t exception = progressing ? spec.exception_timer : std::uint16_t{0};
    Bytes bytes(16384);
    Result<std::size_t> size = std::unexpected(Error::invalid_value);
    switch (kind_)
    {
    case Kind::information:
        size = encode_configuration_status(ConfigurationStatus{counter, spec.code, exception, estimated, spec.text},
                                           bytes);
        break;
    case Kind::upload:
    {
        std::vector<LoadProgress> loads;
        loads.reserve(loads_.size());
        for (const UploadLoad& load : loads_)
        {
            const bool done = spec.code == StatusCode::completed;
            const std::uint8_t ratio =
                done ? std::uint8_t{100}
                     : static_cast<std::uint8_t>(load.files.empty() ? 0 : load.files_done * 100 / load.files.size());
            loads.push_back(LoadProgress{load.header_name, load.part_number, ratio,
                                         done ? StatusCode::completed : StatusCode::in_progress, ""});
        }
        size = encode_upload_status(
            UploadStatus{spec.code, spec.text, counter, exception, estimated,
                         static_cast<std::uint8_t>(spec.code == StatusCode::accepted ? 0 : progress_percent()), loads},
            bytes);
        break;
    }
    case Kind::download_media:
    case Kind::download_operator:
    {
        std::vector<FileProgress> files;
        files.reserve(requests_.size());
        for (const DownloadRequested& request : requests_)
        {
            const StatusCode item = spec.code == StatusCode::completed
                                        ? (request.failed ? StatusCode::item_failed : StatusCode::completed)
                                        : StatusCode::in_progress;
            files.push_back(FileProgress{request.name, item, ""});
        }
        size = encode_download_status(DownloadStatus{spec.code, spec.text, counter, exception, estimated,
                                                     static_cast<std::uint8_t>(progress_percent()), files},
                                      bytes);
        break;
    }
    }
    bytes.resize(size.value_or(0));
    return bytes;
}

void MockEngine::start_heartbeat()
{
    heartbeat_on_ = true;
    const std::uint64_t generation = ++heartbeat_generation_;
    after(config_.status_period, [this, generation] { heartbeat_tick(generation); });
}

void MockEngine::heartbeat_tick(std::uint64_t generation)
{
    if (generation != heartbeat_generation_ || !heartbeat_on_ || aborted_ || final_posted_)
    {
        return;
    }
    if (!status_busy_ && status_queue_.empty())
    {
        post_status(StatusSpec{StatusCode::in_progress, 0, ""});
    }
    after(config_.status_period, [this, generation] { heartbeat_tick(generation); });
}

// ---- The operations ---------------------------------------------------------------------------

Bytes MockEngine::build_configuration_list() const
{
    const std::vector<HardwareItem> items = config_.information.empty() ? default_information() : config_.information;
    // The views the encoder is given must stay put while it reads them: the parts of each
    // item live in their own vector, made before any HardwareItem views them.
    std::vector<std::vector<adl::arinc615a::HardwarePart>> part_storage;
    part_storage.reserve(items.size());
    for (const HardwareItem& item : items)
    {
        std::vector<adl::arinc615a::HardwarePart> parts;
        parts.reserve(item.parts.size());
        for (const HardwarePart& part : item.parts)
        {
            parts.push_back(adl::arinc615a::HardwarePart{part.part_number, part.amendment, part.designation});
        }
        part_storage.push_back(std::move(parts));
    }
    std::vector<adl::arinc615a::HardwareItem> view_items;
    view_items.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        view_items.push_back(
            adl::arinc615a::HardwareItem{items[i].literal_name, items[i].serial_number, part_storage[i]});
    }
    Bytes bytes(4096);
    const auto size = encode_configuration_list(ConfigurationList{view_items}, bytes);
    bytes.resize(size.value_or(0));
    return bytes;
}

Bytes MockEngine::build_download_list() const
{
    std::vector<FileListing> listing;
    listing.reserve(config_.holds.size());
    for (const HeldFile& file : config_.holds)
    {
        listing.push_back(FileListing{file.name, file.description});
    }
    Bytes bytes(16384);
    const auto size = encode_download_list(DownloadList{listing}, bytes);
    bytes.resize(size.value_or(0));
    return bytes;
}

void MockEngine::start_work()
{
    note(Severity::info, "starts work");
    const std::uint64_t generation = main_generation_;
    switch (kind_)
    {
    case Kind::information:
    {
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        if (config_.silent)
        {
            go_silent();
            return;
        }
        after(config_.think_time,
              [this, generation]
              {
                  if (generation != main_generation_)
                  {
                      return;
                  }
                  const Bytes list_bytes = build_configuration_list();
                  const std::string name = config_.thw + ".LCL";
                  write_file(name, list_bytes, checksum_for(list_bytes),
                             [this, generation, name](MiniResult::Kind kind)
                             {
                                 if (generation != main_generation_)
                                 {
                                     return;
                                 }
                                 if (kind == MiniResult::Kind::aborted)
                                 {
                                     on_abort_seen(last_abort_status_);
                                     return;
                                 }
                                 if (kind == MiniResult::Kind::ok)
                                 {
                                     note(Severity::info, "wrote " + name);
                                 }
                                 finish_operation(StatusCode::completed, "");
                             });
              });
        return;
    }
    case Kind::upload:
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        if (config_.silent)
        {
            go_silent();
        }
        return; // the load list comes next, from the loader
    case Kind::download_media:
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        if (config_.silent)
        {
            go_silent();
        }
        return;
    case Kind::download_operator:
    {
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        if (config_.silent)
        {
            go_silent();
            return;
        }
        const Bytes list_bytes = build_download_list();
        const std::string name = config_.thw + ".LNL";
        write_file(name, list_bytes, checksum_for(list_bytes),
                   [this, generation, name](MiniResult::Kind kind)
                   {
                       if (generation != main_generation_)
                       {
                           return;
                       }
                       if (kind == MiniResult::Kind::aborted)
                       {
                           on_abort_seen(last_abort_status_);
                           return;
                       }
                       if (kind == MiniResult::Kind::ok)
                       {
                           note(Severity::info, "wrote " + name);
                       }
                   });
        return;
    }
    }
}

void MockEngine::on_list_received(const std::string& name, const Bytes& bytes)
{
    const std::uint64_t generation = main_generation_;
    const std::string extension_text = name.substr(name.size() - 4);
    if (extension_text == ".LUR")
    {
        const auto request = decode_upload_request(bytes);
        if (!request)
        {
            finish_operation(StatusCode::aborted_by_target, "bad load list");
            return;
        }
        loads_.clear();
        for (const LoadReference& reference : request->loads)
        {
            loads_.push_back(UploadLoad{
                std::string{reference.header_file_name}, std::string{reference.load_part_number_name}, {}, 0});
        }
        loads_done_ = 0;
        load_index_ = 0;
        file_index_ = 0;
        note(Severity::info,
             "upload request: " + std::to_string(loads_.size()) + (loads_.size() == 1 ? " load" : " loads"));
        upload_read_header(generation);
        return;
    }
    requests_.clear();
    if (extension_text == ".LNR")
    {
        const auto request = decode_download_request(bytes);
        if (!request)
        {
            finish_operation(StatusCode::aborted_by_target, "bad file list");
            return;
        }
        for (const FileRequest& file : request->files)
        {
            requests_.push_back(DownloadRequested{std::string{file.file_name}, false});
        }
    }
    else
    {
        const auto answer = decode_download_answer(bytes);
        if (!answer)
        {
            finish_operation(StatusCode::aborted_by_target, "bad selection");
            return;
        }
        for (const std::string_view file : answer->file_names)
        {
            requests_.push_back(DownloadRequested{std::string{file}, false});
        }
    }
    writes_done_ = 0;
    writes_total_ = requests_.size();
    request_index_ = 0;
    note(Severity::info,
         "download request: " + std::to_string(requests_.size()) + (requests_.size() == 1 ? " file" : " files"));
    download_write_next(generation);
}

void MockEngine::upload_read_header(std::uint64_t generation)
{
    if (generation != main_generation_)
    {
        return;
    }
    if (load_index_ >= loads_.size())
    {
        hand_over_uploads();
        finish_operation(StatusCode::completed, "");
        return;
    }
    UploadLoad& load = loads_[load_index_];
    const std::string name = load.header_name;
    read_file(name, std::nullopt,
              [this, generation, name](MiniResult::Kind kind, const Bytes& bytes)
              {
                  if (generation != main_generation_)
                  {
                      return;
                  }
                  if (kind == MiniResult::Kind::aborted)
                  {
                      on_abort_seen(last_abort_status_);
                      return;
                  }
                  if (kind != MiniResult::Kind::ok)
                  {
                      note(Severity::warning, "could not read " + name);
                      finish_operation(StatusCode::aborted_by_target, "could not read " + name);
                      return;
                  }
                  stored_[name] = bytes;
                  note(Severity::info, "read " + name + " (" + std::to_string(bytes.size()) + " bytes)");
                  UploadLoad& current = loads_[load_index_];
                  const auto header = adl::arinc665::decode_header_file(bytes);
                  if (!header)
                  {
                      note(Severity::warning, name + " is not a valid header file");
                      finish_operation(StatusCode::aborted_by_target, "bad header file");
                      return;
                  }
                  for (const adl::arinc665::DataFile& file : header->data_files)
                  {
                      current.files.push_back(UploadFile{file.file_name, checksum_for_file(file)});
                  }
                  for (const adl::arinc665::SupportFile& file : header->support_files)
                  {
                      current.files.push_back(UploadFile{file.file_name, checksum_for_file(file)});
                  }
                  file_index_ = 0;
                  report_progress();
                  upload_read_next(generation);
              });
}

void MockEngine::upload_read_next(std::uint64_t generation)
{
    if (generation != main_generation_)
    {
        return;
    }
    while (load_index_ < loads_.size() && file_index_ >= loads_[load_index_].files.size())
    {
        ++loads_done_;
        ++load_index_;
        file_index_ = 0;
        report_progress();
        post_status(StatusSpec{StatusCode::in_progress, 0, ""});
    }
    if (load_index_ >= loads_.size())
    {
        hand_over_uploads();
        finish_operation(StatusCode::completed, "");
        return;
    }
    const std::size_t load = load_index_;
    const std::size_t file = file_index_;
    const UploadFile item = loads_[load].files[file];
    read_file(item.name, item.checksum,
              [this, generation, load, file, item](MiniResult::Kind kind, const Bytes& bytes)
              {
                  if (generation != main_generation_)
                  {
                      return;
                  }
                  if (kind == MiniResult::Kind::aborted)
                  {
                      on_abort_seen(last_abort_status_);
                      return;
                  }
                  if (kind != MiniResult::Kind::ok)
                  {
                      note(Severity::warning, "could not read " + item.name);
                      finish_operation(StatusCode::aborted_by_target, "could not read " + item.name);
                      return;
                  }
                  if (item.checksum)
                  {
                      const auto value = adl::arinc665::compute_check_value(item.checksum->type, bytes);
                      const bool matches =
                          value && value->bytes.size() == item.checksum->length &&
                          std::equal(value->bytes.begin(), value->bytes.end(), item.checksum->value.begin());
                      if (!matches)
                      {
                          note(Severity::warning, "checksum mismatch for " + item.name);
                      }
                  }
                  stored_[item.name] = bytes;
                  note(Severity::info, "read " + item.name + " (" + std::to_string(bytes.size()) + " bytes)");
                  loads_[load].files_done += 1;
                  file_index_ = file + 1;
                  report_progress();
                  post_status(StatusSpec{StatusCode::in_progress, 0, ""});
                  upload_read_next(generation);
              });
}

void MockEngine::download_write_next(std::uint64_t generation)
{
    if (generation != main_generation_)
    {
        return;
    }
    if (request_index_ >= requests_.size())
    {
        finish_operation(StatusCode::completed, "");
        return;
    }
    DownloadRequested& request = requests_[request_index_];
    const std::size_t index = request_index_;
    ++request_index_;
    const HeldFile* held = nullptr;
    for (const HeldFile& file : config_.holds)
    {
        if (file.name == request.name)
        {
            held = &file;
            break;
        }
    }
    if (held == nullptr)
    {
        request.failed = true;
        note(Severity::warning, "does not hold " + request.name);
        ++writes_done_;
        report_progress();
        post_status(StatusSpec{StatusCode::in_progress, 0, ""});
        download_write_next(generation);
        return;
    }
    const auto checksum = checksum_for(held->content);
    Bytes content = held->content;
    if (config_.corrupt_served && !content.empty())
    {
        content[content.size() / 2] ^= std::byte{0x01};
    }
    const std::string name = request.name;
    write_file(name, std::move(content), checksum,
               [this, generation, index, name](MiniResult::Kind kind)
               {
                   if (generation != main_generation_)
                   {
                       return;
                   }
                   if (kind == MiniResult::Kind::aborted)
                   {
                       on_abort_seen(last_abort_status_);
                       return;
                   }
                   if (kind == MiniResult::Kind::ok)
                   {
                       note(Severity::info, "wrote " + name);
                   }
                   else
                   {
                       requests_[index].failed = true;
                   }
                   ++writes_done_;
                   report_progress();
                   post_status(StatusSpec{StatusCode::in_progress, 0, ""});
                   download_write_next(generation);
               });
}

void MockEngine::hand_over_uploads()
{
    if (listener_ == nullptr)
    {
        return;
    }
    for (const UploadLoad& load : loads_)
    {
        UploadRecord record;
        record.load_part_number = load.part_number;
        if (const auto header = stored_.find(load.header_name); header != stored_.end())
        {
            record.files.push_back(StoredFile{load.header_name, header->second});
        }
        for (const UploadFile& file : load.files)
        {
            if (const auto found = stored_.find(file.name); found != stored_.end())
            {
                record.files.push_back(StoredFile{file.name, found->second});
            }
        }
        listener_->upload_received(record);
    }
}

// ---- Ending an operation ----------------------------------------------------------------------

void MockEngine::go_silent()
{
    silent_ = true;
    ++main_generation_;
    heartbeat_on_ = false;
    note(Severity::warning, "goes silent");
}

void MockEngine::finish_operation(StatusCode code, const std::string& text)
{
    if (final_posted_ || aborted_)
    {
        return;
    }
    final_posted_ = true;
    heartbeat_on_ = false;
    post_status(StatusSpec{code, 0, text});
}

void MockEngine::on_abort_seen(StatusCode status)
{
    if (aborted_)
    {
        return;
    }
    aborted_ = true;
    ++main_generation_;
    heartbeat_on_ = false;
    note(Severity::warning, "sees ABORT (" + std::string{to_string(status)} + ")");
    status_queue_.clear();
    final_posted_ = true;
    // 5.4.5.2: the target answers an abort with a status file that echoes its code.
    post_status(StatusSpec{status, 0, "aborted"});
}

void MockEngine::reset_operation_state()
{
    operation_started_ = false;
    finished_ = false;
    aborted_ = false;
    waited_once_ = false;
    silent_ = false;
    first_status_sent_ = false;
    status_queue_.clear();
    status_busy_ = false;
    counter_ = 0;
    status_index_ = 0;
    ++heartbeat_generation_;
    heartbeat_on_ = false;
    quiet_until_set_ = false;
    loads_.clear();
    requests_.clear();
    loads_done_ = 0;
    load_index_ = 0;
    file_index_ = 0;
    request_index_ = 0;
    writes_done_ = 0;
    writes_total_ = 0;
    ++main_generation_;
    final_posted_ = false;
    last_abort_status_ = StatusCode::aborted_by_data_loader;
    checksum_acknowledged_ = false;
    report_progress();
    if (listener_ != nullptr)
    {
        listener_->operation_changed("");
    }
}

} // namespace adl::mock
