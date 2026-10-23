#include "sim_target.hpp"

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc665/check_value.hpp"

#include <algorithm>
#include <cstdio>
#include <variant>

namespace adl::test
{

using namespace adl::arinc615a;

namespace
{

[[nodiscard]] Endpoint convert(const adl::platform::Endpoint& endpoint)
{
    return Endpoint{Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] adl::platform::Endpoint convert(const Endpoint& endpoint)
{
    return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] std::string seconds_text(Instant at)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.3f", std::chrono::duration<double>(at.since_origin).count());
    return buffer;
}

} // namespace

SimTarget::SimTarget(adl::fake::FakeWorld& world, adl::fake::FakeNetwork& network, adl::platform::Ipv4Address address,
                     SimPlan plan)
    : world_(world), network_(network), address_(address), plan_(std::move(plan))
{
    network_.attach_agent(address_, *this);
}

SimTarget::~SimTarget()
{
    *alive_ = false;
    network_.detach_agent(address_);
}

Instant SimTarget::now() const
{
    return Instant{world_.now().since_origin};
}

void SimTarget::note(const std::string& text)
{
    log_.events.push_back(seconds_text(now()) + " " + text);
}

void SimTarget::go_silent()
{
    silent_ = true;
    ++main_generation_;
    heartbeat_on_ = false;
    note("goes silent");
}

void SimTarget::send(std::uint16_t from_port, const Endpoint& to, std::span<const std::byte> payload)
{
    if (silent_)
    {
        return;
    }
    network_.send(adl::platform::Endpoint{address_, from_port}, convert(to), payload);
}

void SimTarget::after(Duration delay, std::function<void()> action)
{
    world_.schedule_after(delay,
                          [alive = std::weak_ptr<bool>(alive_), this, action = std::move(action)]
                          {
                              const auto live = alive.lock();
                              if (live && *live && !silent_)
                              {
                                  action();
                              }
                          });
}

MiniTransfer& SimTarget::add_transfer(MiniTransfer::Role role)
{
    const std::uint16_t port = next_port_++;
    auto transfer = std::make_unique<MiniTransfer>(static_cast<MiniHost&>(*this), port, role);
    MiniTransfer& reference = *transfer;
    transfers_[port] = std::move(transfer);
    return reference;
}

void SimTarget::remove_later(std::uint16_t port)
{
    after(std::chrono::seconds(5), [this, port] { transfers_.erase(port); });
}

// ---- Datagrams -----------------------------------------------------------------------------

void SimTarget::on_datagram(adl::platform::Instant /*at*/, const adl::platform::Endpoint& from,
                            const adl::platform::Endpoint& to, std::span<const std::byte> payload)
{
    if (silent_)
    {
        return;
    }
    const Endpoint source = convert(from);
    if (to.port == find_port)
    {
        handle_find(source, payload);
        return;
    }
    if (to.port == tftp_control_port)
    {
        const auto packet = decode_packet(payload);
        if (packet)
        {
            if (const auto* request = std::get_if<TftpRequest>(&*packet))
            {
                handle_request(source, *request);
            }
        }
        return;
    }
    if (const auto found = transfers_.find(to.port); found != transfers_.end())
    {
        found->second->on_datagram(source, payload);
    }
}

void SimTarget::handle_find(const Endpoint& from, std::span<const std::byte> payload)
{
    const auto packet = decode_find(payload);
    if (!packet || !std::holds_alternative<FindRequest>(*packet) || !plan_.find_answer)
    {
        return;
    }
    const FindAnswer answer = *plan_.find_answer;
    after(plan_.find_delay,
          [this, from, answer]
          {
              std::array<std::byte, find_max_answer_size> buffer{};
              if (const auto size = encode_find_answer(answer, buffer))
              {
                  for (unsigned copy = 0; copy < plan_.find_answer_copies; ++copy)
                  {
                      send(find_port, from, std::span<const std::byte>{buffer}.first(*size));
                  }
              }
          });
}

void SimTarget::handle_request(const Endpoint& from, const TftpRequest& request)
{
    const std::string name{request.file_name};
    const auto kind = kind_of_file_name(name);
    const bool matches = kind && name.substr(0, name.size() - 4) == plan_.thw;
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
    if (is_list && (!operation_started_ || log_.status_started_at.empty()))
    {
        // 6.2.7.3: the list is written after the status file that says "accepted"; a loader that
        // writes it sooner has broken the synchronisation the standard asks for.
        std::array<std::byte, 64> buffer{};
        if (const auto size =
                encode_error(TftpError{TftpErrorCode::illegal_operation, "list before the status"}, buffer))
        {
            send(next_port_++, from, std::span<const std::byte>{buffer}.first(*size));
        }
        note("refuses a list that came before the first status file");
        return;
    }
    if (is_list)
    {
        MiniTransfer& transfer = add_transfer(MiniTransfer::Role::server_write);
        transfer.on_done = [this, name](MiniTransfer& done)
        {
            if (done.result().kind == MiniResult::Kind::ok)
            {
                on_list_received(name, done.received());
            }
            remove_later(done.port());
        };
        transfer.serve(from, request, {}, plan_.serve_block_size_cap);
        return;
    }
    // Anything else: file not found.
    std::array<std::byte, 64> buffer{};
    if (const auto size = encode_error(TftpError{TftpErrorCode::file_not_found, "no such file"}, buffer))
    {
        send(next_port_++, from, std::span<const std::byte>{buffer}.first(*size));
    }
}

// ---- The initialization file -----------------------------------------------------------------

void SimTarget::serve_init(const Endpoint& from, const TftpRequest& request, Kind kind)
{
    log_.init_requests_at.push_back(now());
    if (plan_.init_silent || operation_started_)
    {
        return;
    }
    kind_ = kind;
    std::array<std::byte, 64> small{};
    if (plan_.init_wait_seconds != 0 && (!waited_once_ || plan_.init_wait_always))
    {
        waited_once_ = true;
        if (const auto size = encode_wait(WaitMessage{plan_.init_wait_seconds}, small))
        {
            send(next_port_++, from, std::span<const std::byte>{small}.first(*size));
        }
        note("answers the first request with WAIT");
        return;
    }
    if (request.options.size() != 0)
    {
        ++log_.init_options_seen;
        if (plan_.init_refuses_options)
        {
            if (const auto size = encode_error(TftpError{TftpErrorCode::option_refused, "no options"}, small))
            {
                send(next_port_++, from, std::span<const std::byte>{small}.first(*size));
            }
            return;
        }
    }
    loader_ = Endpoint{from.address, tftp_control_port};
    if (plan_.honor_port_option)
    {
        const auto options = read_transfer_options(request.options);
        if (options && options->port)
        {
            loader_.port = *options->port;
        }
    }
    log_.loader_port_used = loader_.port;

    Bytes file(512);
    const auto size = encode_initialization_file(InitializationFile{plan_.init_status, plan_.init_text}, file);
    file.resize(size.value_or(0));
    if (file.size() >= 6)
    {
        file[4] = static_cast<std::byte>(plan_.init_version[0]);
        file[5] = static_cast<std::byte>(plan_.init_version[1]);
    }
    if (plan_.init_garbage)
    {
        file.resize(file.size() - 2); // the File Length no longer agrees
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
    transfer.serve(from, request, std::move(file), plan_.serve_block_size_cap, plan_.honor_port_option);
}

void SimTarget::on_init_served()
{
    note("initialization file delivered");
    if (plan_.init_status != StatusCode::accepted || plan_.init_garbage)
    {
        return;
    }
    operation_started_ = true;
    after(plan_.think_time, [this] { start_work(); });
}

// ---- Client side: writes and reads
// ---------------------------------------------------------------

std::optional<ChecksumOption> SimTarget::checksum_for(const Bytes& content) const
{
    if (plan_.checksum == ChecksumMode::none)
    {
        return std::nullopt;
    }
    const auto value = adl::arinc665::compute_check_value(adl::arinc665::CheckValueType::crc16, content);
    if (!value)
    {
        return std::nullopt;
    }
    ChecksumOption option;
    option.type = adl::arinc665::CheckValueType::crc16;
    option.length = static_cast<std::uint8_t>(value->bytes.size());
    std::copy(value->bytes.begin(), value->bytes.end(), option.value.begin());
    if (plan_.checksum == ChecksumMode::wrong)
    {
        option.value[0] ^= std::byte{0x01};
    }
    return option;
}

void SimTarget::write_file(const std::string& name, Bytes content, std::size_t block_size,
                           std::optional<ChecksumOption> checksum, std::function<void(MiniResult::Kind)> then,
                           unsigned attempt)
{
    MiniTransfer& transfer = add_transfer(MiniTransfer::Role::client_write);
    transfer.on_done = [this, name, content, block_size, checksum, then, attempt](MiniTransfer& done)
    {
        const MiniResult result = done.result();
        log_.checksum_acknowledged = log_.checksum_acknowledged || done.checksum_acknowledged();
        remove_later(done.port());
        switch (result.kind)
        {
        case MiniResult::Kind::ok:
            then(MiniResult::Kind::ok);
            return;
        case MiniResult::Kind::waited:
            ++log_.waits_received;
            after(std::chrono::seconds(result.wait_seconds), [this, name, content, block_size, checksum, then, attempt]
                  { write_file(name, content, block_size, checksum, then, attempt); });
            return;
        case MiniResult::Kind::aborted:
            last_abort_status_ = result.abort_status;
            then(MiniResult::Kind::aborted);
            return;
        default:
            if (attempt < tftp_retries)
            { // the DLP retry number is 1, as the TFTP one
                write_file(name, content, block_size, checksum, then, attempt + 1);
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
    request.block_size = block_size;
    request.checksum = checksum;
    transfer.start_client(loader_, std::move(request));
}

void SimTarget::read_file(const ReadItem& item, std::function<void(MiniResult::Kind, const Bytes&)> then,
                          unsigned attempt)
{
    MiniTransfer& transfer = add_transfer(MiniTransfer::Role::client_read);
    transfer.on_done = [this, item, then, attempt](MiniTransfer& done)
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
            ++log_.waits_received;
            after(std::chrono::seconds(result.wait_seconds),
                  [this, item, then, attempt] { read_file(item, then, attempt); });
            return;
        case MiniResult::Kind::aborted:
            last_abort_status_ = result.abort_status;
            then(MiniResult::Kind::aborted, {});
            return;
        default:
            if (attempt < tftp_retries)
            {
                read_file(item, then, attempt + 1);
            }
            else
            {
                then(result.kind, {});
            }
            return;
        }
    };
    MiniTransfer::Request request;
    request.file_name = item.name;
    request.block_size = plan_.request_block_size;
    if (item.crc16)
    {
        ChecksumOption option;
        option.type = adl::arinc665::CheckValueType::crc16;
        option.length = 2;
        option.value[0] = static_cast<std::byte>(*item.crc16 >> 8);
        option.value[1] = static_cast<std::byte>(*item.crc16 & 0xFF);
        request.checksum = option;
    }
    transfer.start_client(loader_, std::move(request));
}

// ---- Status files -------------------------------------------------------------------------------

std::uint32_t SimTarget::progress_percent() const
{
    if (kind_ == Kind::upload)
    {
        return reads_total_ == 0 ? 0 : static_cast<std::uint32_t>(reads_done_ * 100 / reads_total_);
    }
    return writes_total_ == 0 ? 0 : static_cast<std::uint32_t>(writes_done_ * 100 / writes_total_);
}

void SimTarget::post_status(StatusSpec spec)
{
    status_queue_.push_back(std::move(spec));
    pump_status();
}

void SimTarget::pump_status()
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
    StatusSpec spec = status_queue_.front();
    status_queue_.erase(status_queue_.begin());

    if (plan_.silent_after_statuses && status_index_ >= *plan_.silent_after_statuses)
    {
        go_silent();
        return;
    }
    if (plan_.abort_after_statuses && status_index_ == *plan_.abort_after_statuses)
    {
        spec.code = StatusCode::aborted_by_target;
        spec.text = "target gave up";
        status_queue_.clear();
        heartbeat_on_ = false;
        final_posted_ = true;
    }
    if (status_index_ == plan_.skip_counter_at)
    {
        ++counter_; // as if a status file had been lost on its way
    }
    const std::uint16_t counter = static_cast<std::uint16_t>(plan_.first_counter + counter_++);
    const std::size_t index = status_index_++;
    Bytes bytes = build_status(spec, counter);
    if (plan_.truncate_status && index == 1 && bytes.size() > 4)
    {
        bytes.resize(bytes.size() - 3);
    }
    const ProtocolFileKind status_kind = kind_ == Kind::information ? ProtocolFileKind::lcs
                                         : kind_ == Kind::upload    ? ProtocolFileKind::lus
                                                                    : ProtocolFileKind::lns;
    const std::string name = plan_.thw + std::string{extension(status_kind)};
    log_.status_started_at.push_back(now());
    const bool final_status = spec.code == StatusCode::completed || spec.code == StatusCode::aborted_by_target ||
                              spec.code == StatusCode::aborted_by_data_loader ||
                              spec.code == StatusCode::aborted_by_operator;
    const bool sets_quiet = plan_.exception_silence > Duration::zero() && spec.exception_timer != 0;
    const bool repeat = plan_.repeat_status_index && *plan_.repeat_status_index == index;
    const Bytes again = repeat ? bytes : Bytes{};
    write_file(name, std::move(bytes), plan_.request_block_size, std::nullopt,
               [this, counter, final_status, spec, sets_quiet, repeat, again, name](MiniResult::Kind kind)
               {
                   const bool repeating = kind == MiniResult::Kind::ok && repeat;
                   if (repeating)
                   {
                       // The same status file again, as a target does that never saw the ACK of
                       // its last block (a repeat the loader must recognise by the counter).
                       write_file(name, again, plan_.request_block_size, std::nullopt,
                                  [this](MiniResult::Kind)
                                  {
                                      status_busy_ = false;
                                      pump_status();
                                  });
                   }
                   else
                   {
                       status_busy_ = false;
                   }
                   if (kind == MiniResult::Kind::ok)
                   {
                       log_.statuses_sent.push_back(counter);
                       log_.last_status_end = now();
                       note("status " + std::to_string(counter) + " delivered");
                       if (sets_quiet)
                       {
                           quiet_until_set_ = true;
                           quiet_until_ = now() + plan_.exception_silence;
                       }
                       if (final_status)
                       {
                           finished_ = true;
                           log_.final_status = spec.code;
                           log_.finished_at = now();
                       }
                   }
                   else if (kind == MiniResult::Kind::aborted)
                   {
                       note("status refused with ABORT");
                       on_abort_seen(last_abort_status_);
                   }
                   else
                   {
                       ++log_.statuses_failed;
                       note("status failed");
                       if (final_status)
                       {
                           finished_ = true;
                           log_.finished_at = now();
                       }
                   }
                   pump_status();
               });
}

Bytes SimTarget::build_status(const StatusSpec& spec, std::uint16_t counter)
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
        for (const UploadItem& load : plan_.loads)
        {
            const bool done = spec.code == StatusCode::completed;
            loads.push_back(LoadProgress{load.header_name, load.load_part_number,
                                         static_cast<std::uint8_t>(done ? 100 : progress_percent()),
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
        for (const DownloadItem& file : plan_.files)
        {
            files.push_back(FileProgress{file.name, StatusCode::in_progress, ""});
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

void SimTarget::start_heartbeat()
{
    if (!plan_.keep_heartbeat)
    {
        return;
    }
    heartbeat_on_ = true;
    const std::uint64_t generation = ++heartbeat_generation_;
    after(plan_.status_period, [this, generation] { heartbeat_tick(generation); });
}

void SimTarget::heartbeat_tick(std::uint64_t generation)
{
    if (generation != heartbeat_generation_ || !heartbeat_on_ || aborted_ || final_posted_)
    {
        return;
    }
    if (!status_busy_ && status_queue_.empty())
    {
        StatusSpec spec;
        spec.code = StatusCode::in_progress;
        // The exception timer goes with the second status file that says "in progress".
        if (plan_.exception_timer != 0 && status_index_ == 2)
        {
            spec.exception_timer = plan_.exception_timer;
        }
        post_status(std::move(spec));
    }
    after(plan_.status_period, [this, generation] { heartbeat_tick(generation); });
}

// ---- The operations -----------------------------------------------------------------------------

void SimTarget::start_work()
{
    note("starts work");
    const std::uint64_t generation = main_generation_;
    switch (kind_)
    {
    case Kind::information:
    {
        static const Bytes list_bytes = []
        {
            const std::array<HardwarePart, 2> parts{HardwarePart{"ACM47-1234-5678", "", "FMS 1 OPS"},
                                                    HardwarePart{"ACM47-8765-4321", "A", "FMS 1 DATA"}};
            const std::array<HardwareItem, 1> items{HardwareItem{"FMS LEFT", "SN-0001", parts}};
            Bytes bytes(1024);
            const auto size = encode_configuration_list(ConfigurationList{items}, bytes);
            bytes.resize(size.value_or(0));
            return bytes;
        }();
        if (!plan_.fast_information)
        {
            post_status(StatusSpec{StatusCode::accepted, 0, ""});
            start_heartbeat();
        }
        after(plan_.think_time,
              [this, generation]
              {
                  if (generation != main_generation_)
                  {
                      return;
                  }
                  write_file(plan_.thw + ".LCL", list_bytes, plan_.request_block_size, checksum_for(list_bytes),
                             [this, generation](MiniResult::Kind kind)
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
                                     log_.files_written.push_back("LCL");
                                 }
                                 if (plan_.fast_information)
                                 {
                                     finished_ = true;
                                     log_.finished_at = now();
                                 }
                                 else
                                 {
                                     finish_operation(StatusCode::completed, "");
                                 }
                             });
              });
        return;
    }
    case Kind::upload:
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        return; // the load list comes next, from the loader
    case Kind::download_media:
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        return;
    case Kind::download_operator:
    {
        post_status(StatusSpec{StatusCode::accepted, 0, ""});
        start_heartbeat();
        std::vector<FileListing> listing;
        for (const DownloadItem& file : plan_.files)
        {
            listing.push_back(FileListing{file.name, file.description});
        }
        Bytes bytes(16384);
        const auto size = encode_download_list(DownloadList{listing}, bytes);
        bytes.resize(size.value_or(0));
        const auto checksum = checksum_for(bytes);
        write_file(plan_.thw + ".LNL", std::move(bytes), plan_.request_block_size, checksum,
                   [this, generation](MiniResult::Kind kind)
                   {
                       if (generation != main_generation_)
                       {
                           return;
                       }
                       if (kind == MiniResult::Kind::aborted)
                       {
                           on_abort_seen(last_abort_status_);
                       }
                   });
        return;
    }
    }
}

void SimTarget::on_list_received(const std::string& name, const Bytes& bytes)
{
    ++log_.lists_received;
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
        active_loads_.clear();
        reads_done_ = 0;
        reads_total_ = 0;
        for (const LoadReference& reference : request->loads)
        {
            log_.requested_files.emplace_back(reference.header_file_name);
            for (std::size_t i = 0; i < plan_.loads.size(); ++i)
            {
                if (plan_.loads[i].header_name == reference.header_file_name)
                {
                    active_loads_.push_back(i);
                    reads_total_ += plan_.loads[i].reads.size();
                }
            }
        }
        upload_read_next(0, 0, generation);
        return;
    }
    std::vector<std::string> names;
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
            names.emplace_back(file.file_name);
            log_.requested_files.emplace_back(file.file_name);
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
            names.emplace_back(file);
            log_.selected_files.emplace_back(file);
        }
    }
    writes_done_ = 0;
    writes_total_ = names.size() + (plan_.list_unrequested_file ? 1U : 0U);
    download_write_next(std::move(names), 0, generation);
}

void SimTarget::upload_read_next(std::size_t active_index, std::size_t read_index, std::uint64_t generation)
{
    if (generation != main_generation_)
    {
        return;
    }
    while (active_index < active_loads_.size() && read_index >= plan_.loads[active_loads_[active_index]].reads.size())
    {
        ++active_index;
        read_index = 0;
    }
    if (active_index >= active_loads_.size())
    {
        finish_operation(StatusCode::completed, "");
        return;
    }
    const ReadItem item = plan_.loads[active_loads_[active_index]].reads[read_index];
    read_file(item,
              [this, item, active_index, read_index, generation](MiniResult::Kind kind, const Bytes& bytes)
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
                      log_.read_failures.push_back(item.name);
                      finish_operation(StatusCode::aborted_by_target, "could not read " + item.name);
                      return;
                  }
                  (bytes == item.expected ? log_.files_read : log_.files_corrupt).push_back(item.name);
                  stored_[item.name] = bytes;
                  ++reads_done_;
                  upload_read_next(active_index, read_index + 1, generation);
              });
}

void SimTarget::download_write_next(std::vector<std::string> names, std::size_t index, std::uint64_t generation)
{
    if (generation != main_generation_)
    {
        return;
    }
    std::string name;
    Bytes content;
    if (index < names.size())
    {
        name = names[index];
        const auto item = std::ranges::find(plan_.files, name, &DownloadItem::name);
        if (item == plan_.files.end())
        {
            download_write_next(std::move(names), index + 1, generation);
            return;
        }
        content = item->content;
    }
    else if (index == names.size() && plan_.list_unrequested_file)
    {
        name = "ROGUE.BIN";
        content = Bytes(100, std::byte{0x5A});
    }
    else
    {
        finish_operation(StatusCode::completed, "");
        return;
    }
    write_file(name, content, plan_.request_block_size, checksum_for(content),
               [this, name, names = std::move(names), index, generation](MiniResult::Kind kind) mutable
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
                       log_.files_written.push_back(name);
                       ++writes_done_;
                   }
                   download_write_next(std::move(names), index + 1, generation);
               });
}

void SimTarget::reset_operation()
{
    log_ = SimLog{};
    operation_started_ = false;
    finished_ = false;
    aborted_ = false;
    waited_once_ = false;
    status_queue_.clear();
    status_busy_ = false;
    counter_ = 0;
    status_index_ = 0;
    ++heartbeat_generation_;
    heartbeat_on_ = false;
    quiet_until_set_ = false;
    reads_done_ = reads_total_ = writes_done_ = writes_total_ = 0;
    active_loads_.clear();
    ++main_generation_;
    final_posted_ = false;
    work_done_ = false;
    silent_ = false;
}

void SimTarget::finish_operation(StatusCode code, const std::string& text)
{
    if (final_posted_ || aborted_)
    {
        return;
    }
    if (code == StatusCode::completed && plan_.work_time > Duration::zero() && !work_done_)
    {
        // Still checking, programming or just slow: the heartbeat goes on meanwhile.
        work_done_ = true;
        const std::uint64_t generation = main_generation_;
        after(plan_.work_time,
              [this, generation, code, text]
              {
                  if (generation == main_generation_)
                  {
                      finish_operation(code, text);
                  }
              });
        return;
    }
    final_posted_ = true;
    heartbeat_on_ = false;
    post_status(StatusSpec{code, 0, text});
}

void SimTarget::on_abort_seen(StatusCode status)
{
    if (aborted_)
    {
        return;
    }
    aborted_ = true;
    ++main_generation_;
    heartbeat_on_ = false;
    log_.aborts_seen.push_back(status);
    note("sees ABORT");
    status_queue_.clear();
    final_posted_ = true;
    // 5.4.5.2: the target answers an abort with a status file that echoes its code.
    post_status(StatusSpec{status, 0, "aborted"});
}

} // namespace adl::test
