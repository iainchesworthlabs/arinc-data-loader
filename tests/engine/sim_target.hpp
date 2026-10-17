#pragma once

// A simulated 615A target on the fake network: an adl::fake::Agent that behaves as section 5.4
// and 6.3 say a target does, so that the engine can be run against something that is not
// itself. It answers the data loader's request for the initialization file, writes its status
// files (the heartbeat) while it works, reads the load's files from the loader (an upload),
// writes the files of a download to it, and writes the lists of an Information or an operator
// defined Download operation. Its TFTP is MiniTransfer, which shares no code with the engine.
//
// What it does is steered by a SimPlan, so that a test can make it slow, silent, refuse, abort,
// lose a status file, send a wrong checksum or a file that is not valid; and by the network's
// loss, duplication and reordering, which are the fake network's.

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "fake_host.hpp"
#include "mini_tftp.hpp"

namespace adl::test {

// A file the target reads from the loader, with what it expects to get.
struct ReadItem {
    std::string name;
    Bytes expected;
    std::optional<std::uint16_t> crc16;  // sent as the checksum option, as the header file's CRC
};

// A load to upload: its header file and the files the target will read for it (the header
// first, then the data and support files).
struct UploadItem {
    std::string header_name;
    std::string load_part_number;
    std::vector<ReadItem> reads;
};

// A file the target holds, for a download.
struct DownloadItem {
    std::string name;
    Bytes content;
    std::string description;
};

enum class ChecksumMode { none, correct, wrong };

struct SimPlan {
    std::string thw = "HNPFMS_L";

    // ---- the initialization file ----
    std::uint16_t init_wait_seconds = 0;  // answer the first request with WAIT:x
    bool init_wait_always = false;        // ... and every later one too
    bool init_silent = false;             // never answer it
    adl::arinc615a::StatusCode init_status = adl::arinc615a::StatusCode::accepted;
    std::string init_text;
    char init_version[3] = {'A', '4', 0};
    bool init_garbage = false;          // a file that is not a valid structure
    bool init_refuses_options = false;  // error 8 for a request that carries options
    bool honor_port_option = true;

    // ---- how it works ----
    adl::arinc615a::Duration think_time = std::chrono::seconds(1);
    adl::arinc615a::Duration status_period = std::chrono::seconds(4);
    std::size_t request_block_size = 512;  // asked for when it writes a file
    std::size_t serve_block_size_cap = 1024;
    ChecksumMode checksum = ChecksumMode::correct;
    bool fast_information = false;         // an Information with no status files
    adl::arinc615a::Duration work_time{};  // how long it keeps at it after the last file
    bool wait_for_list_before_files = true;

    // ---- the status files ----
    std::uint16_t first_counter = 0;
    std::uint16_t skip_counter_at = 0xFFFF;  // lose the status file with this index (counter bump)
    std::uint16_t exception_timer = 0;       // seconds, sent in the second in-progress status
    adl::arinc615a::Duration exception_silence{};      // then stay silent this long
    std::optional<std::size_t> silent_after_statuses;  // goes quiet after this many are sent
    std::optional<std::size_t> abort_after_statuses;   // sends 0x1003 after this many
    bool truncate_status = false;                      // the second status file is cut short
    std::optional<std::size_t> repeat_status_index;    // writes this status file twice
    bool keep_heartbeat = true;

    // ---- the data ----
    std::vector<UploadItem> loads;
    std::vector<DownloadItem> files;
    bool list_unrequested_file = false;  // a download: also writes a file nobody asked for

    // ---- FIND ----
    std::optional<adl::arinc615a::FindAnswer> find_answer;
    unsigned find_answer_copies = 1;
    adl::arinc615a::Duration find_delay = std::chrono::milliseconds(50);
};

struct SimLog {
    std::vector<std::string> events;
    std::vector<adl::arinc615a::Instant> init_requests_at;
    std::vector<adl::arinc615a::Instant> status_started_at;
    std::vector<std::string> files_read;     // read from the loader and as expected
    std::vector<std::string> files_corrupt;  // read, but not what was expected
    std::vector<std::string> files_written;  // written to the loader
    std::vector<std::string> read_failures;
    std::vector<std::uint16_t> statuses_sent;  // counters of status files the loader acknowledged
    unsigned statuses_failed = 0;
    unsigned waits_received = 0;
    std::vector<adl::arinc615a::StatusCode> aborts_seen;
    std::optional<adl::arinc615a::StatusCode> final_status;
    bool checksum_acknowledged = false;
    std::optional<adl::arinc615a::Instant> last_status_end;
    std::optional<adl::arinc615a::Instant> finished_at;
    std::uint16_t loader_port_used = 0;
    unsigned lists_received = 0;
    std::vector<std::string> selected_files;   // the names in the .LNA it was sent
    std::vector<std::string> requested_files;  // the names in the .LNR it was sent
    unsigned init_options_seen = 0;
};

class SimTarget final : public adl::fake::Agent, private MiniHost {
public:
    SimTarget(adl::fake::FakeWorld& world, adl::fake::FakeNetwork& network,
              adl::platform::Ipv4Address address, SimPlan plan);
    ~SimTarget() override;

    void on_datagram(adl::platform::Instant at, const adl::platform::Endpoint& from,
                     const adl::platform::Endpoint& to,
                     std::span<const std::byte> payload) override;

    [[nodiscard]] SimPlan& plan() noexcept { return plan_; }
    [[nodiscard]] const SimLog& log() const noexcept { return log_; }
    [[nodiscard]] bool finished() const noexcept { return finished_; }

    // Stops all activity at once, as a target that lost power would.
    void go_silent();

private:
    enum class Kind { information, upload, download_media, download_operator };

    struct StatusSpec {
        adl::arinc615a::StatusCode code = adl::arinc615a::StatusCode::in_progress;
        std::uint16_t exception_timer = 0;
        std::string text;
    };

    // MiniHost
    void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to,
              std::span<const std::byte> payload) override;
    void after(adl::arinc615a::Duration delay, std::function<void()> action) override;

    [[nodiscard]] adl::arinc615a::Instant now() const;
    void note(const std::string& text);
    MiniTransfer& add_transfer(MiniTransfer::Role role);
    void remove_later(std::uint16_t port);

    void handle_request(const adl::arinc615a::Endpoint& from,
                        const adl::arinc615a::TftpRequest& request);
    void serve_init(const adl::arinc615a::Endpoint& from,
                    const adl::arinc615a::TftpRequest& request, Kind kind);
    void on_init_served();
    void on_list_received(const std::string& name, const Bytes& bytes);

    // Client side: writes and reads, with the DLP retry and the WAIT behaviour.
    void write_file(const std::string& name, Bytes content, std::size_t block_size,
                    std::optional<adl::arinc615a::ChecksumOption> checksum,
                    std::function<void(MiniResult::Kind)> then, unsigned attempt = 0);
    void read_file(const ReadItem& item, std::function<void(MiniResult::Kind, const Bytes&)> then,
                   unsigned attempt = 0);
    [[nodiscard]] std::optional<adl::arinc615a::ChecksumOption> checksum_for(
        const Bytes& content) const;

    // Status files.
    void post_status(StatusSpec spec);
    void pump_status();
    [[nodiscard]] Bytes build_status(const StatusSpec& spec, std::uint16_t counter);
    void start_heartbeat();
    void heartbeat_tick(std::uint64_t generation);

    // The operations.
    void start_work();
    void upload_read_next(std::size_t active_index, std::size_t read_index,
                          std::uint64_t generation);
    void download_write_next(std::vector<std::string> names, std::size_t index,
                             std::uint64_t generation);
    void handle_find(const adl::arinc615a::Endpoint& from, std::span<const std::byte> payload);
    void finish_operation(adl::arinc615a::StatusCode code, const std::string& text);
    void on_abort_seen(adl::arinc615a::StatusCode status);
    [[nodiscard]] std::uint32_t progress_percent() const;

    adl::fake::FakeWorld& world_;
    adl::fake::FakeNetwork& network_;
    adl::platform::Ipv4Address address_;
    SimPlan plan_;
    SimLog log_;

    std::map<std::uint16_t, std::unique_ptr<MiniTransfer>> transfers_;
    std::uint16_t next_port_ = 40000;

    Kind kind_ = Kind::information;
    bool operation_started_ = false;
    bool silent_ = false;
    bool finished_ = false;
    bool aborted_ = false;
    bool waited_once_ = false;
    adl::arinc615a::Endpoint loader_;

    // status lane
    std::vector<StatusSpec> status_queue_;
    bool status_busy_ = false;
    std::uint16_t counter_ = 0;
    std::size_t status_index_ = 0;  // how many status files have been started
    std::uint64_t heartbeat_generation_ = 0;
    bool heartbeat_on_ = false;
    bool quiet_until_set_ = false;
    adl::arinc615a::Instant quiet_until_{};

    // progress
    std::size_t reads_done_ = 0;
    std::size_t reads_total_ = 0;
    std::size_t writes_done_ = 0;
    std::size_t writes_total_ = 0;
    std::vector<std::size_t> active_loads_;
    std::uint64_t main_generation_ = 0;
    adl::arinc615a::StatusCode last_abort_status_ =
        adl::arinc615a::StatusCode::aborted_by_data_loader;
    bool final_posted_ = false;
    bool work_done_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

}  // namespace adl::test
