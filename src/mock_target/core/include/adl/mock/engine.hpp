#pragma once

// The mock target's engine: the behavior of an ARINC 615A target, written like the loader's
// engine is (plain C++23, no Qt, no sockets of its own). It serves the initialization files
// (<THW_ID_POS>.LCI/.LUI/.LND/.LNO) and runs the Information, Upload and Download operations
// of section 5.4 through adl::arinc615a's codecs and adl::arinc615a_mini's TFTP, speaking
// through a TargetLink (real sockets in the application, the fake network in the tests).
//
// It is single-threaded and event-driven at heart: every datagram and every timer is handled
// at once, on the thread that drives it. run() is the loop the application uses over a real
// link (wait, feed the datagrams, let the link fire the timers); over the fake link the
// world's own events drive it directly, as they drive the simulated target of the tests.
//
// It keeps no socket, thread or clock, and holds the files it has read in memory (demo loads
// are small): a completed upload is handed over to the EngineListener, which is where the
// model writes it to storage. One operation at a time; when one ends (whatever the status)
// the engine is ready for the next.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "adl/arinc615a/time.hpp"
#include "adl/arinc615a_mini/mini_tftp.hpp"
#include "adl/arinc665/files.hpp"
#include "adl/mock/link.hpp"

namespace adl::mock {

using adl::arinc615a_mini::Bytes;

// How much a log line matters (the model maps these to its own severities).
enum class Severity : std::uint8_t { info = 0, notice = 1, warning = 2, error = 3 };

// A file of a completed upload, as the model writes it to storage.
struct StoredFile {
    std::string name;
    Bytes bytes;

    friend bool operator==(const StoredFile&, const StoredFile&) = default;
};

// One completed upload: the load's part number (from the .LUR) and the files the target
// read for it, header first. Handed to the listener when the operation ends completed.
struct UploadRecord {
    std::string load_part_number;
    std::vector<StoredFile> files;
};

// What the engine reports. Everything is a plain string or number, valid during the call
// only; the model copies it into Qt values and emits a signal.
class EngineListener {
public:
    virtual ~EngineListener() = default;

    virtual void log(Severity severity, std::string_view text) = 0;
    // The operation being served ("Information", "Upload", "Download (media defined)",
    // "Download (operator defined)"), or empty between operations.
    virtual void operation_changed(std::string_view name) = 0;
    // The percentage of the list the engine has got through, 0 to 100, or -1 when nothing
    // is running.
    virtual void progress_changed(int percent) = 0;
    virtual void upload_received(const UploadRecord& record) = 0;
};

// Which of the four operations.
enum class OperationKind : std::uint8_t {
    information,
    upload,
    download_media,
    download_operator,
};

// A part and a piece of hardware the target reports in its configuration list (.LCL).
struct HardwarePart {
    std::string part_number;
    std::string amendment;
    std::string designation;
};
struct HardwareItem {
    std::string literal_name;
    std::string serial_number;
    std::vector<HardwarePart> parts;
};

// A file the target holds, served in a download.
struct HeldFile {
    std::string name;
    Bytes content;
    std::string description;
};

// Refusing one operation: its initialization file answers with `code` (0x1000 denied or
// 0x1002 not supported) and `text`.
struct Refusal {
    OperationKind kind = OperationKind::information;
    adl::arinc615a::StatusCode code = adl::arinc615a::StatusCode::operation_denied;
    std::string text;
};

struct Config {
    // ---- identity ----
    std::string thw = "HNPFMS_L";  // <THW_ID_POS>: the stem of the protocol file names
    adl::arinc615a::FindAnswer find_answer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::arinc615a::Duration find_delay = std::chrono::milliseconds(50);
    unsigned find_copies = 1;
    std::uint16_t tftp_port = adl::arinc615a::tftp_control_port;
    std::uint16_t find_port = adl::arinc615a::find_port;

    // ---- how it works ----
    adl::arinc615a::Duration think_time = std::chrono::seconds(1);
    adl::arinc615a::Duration status_period = std::chrono::seconds(4);
    std::size_t request_block_size = 512;  // asked for when the engine writes a file

    // ---- the initialization file ----
    std::uint16_t wait_first_seconds = 0;  // answer the first request with WAIT:x
    bool wait_always = false;              // ... and every later one too
    std::optional<Refusal> refusal;
    bool garbage_init = false;  // serve an initialization file of an invalid structure

    // ---- injections (all default to "behave") ----
    bool corrupt_served = false;     // flip a bit in every served file
    bool wrong_checksum = false;     // a wrong check value option on what it writes
    unsigned drop_status_every = 0;  // lose every Nth status file (0: lose none)
    std::optional<std::size_t> abort_after_statuses;  // send 0x1003 after this many
    bool silent = false;  // after the first status file: answer nothing (the DLP time-out)

    // ---- data ----
    std::vector<HardwareItem> information;  // empty: the default list
    std::vector<HeldFile> holds;            // the files a download serves
};

class MockEngine final : public adl::arinc615a_mini::MiniHost {
public:
    explicit MockEngine(EngineListener* listener = nullptr);
    ~MockEngine() override;
    MockEngine(const MockEngine&) = delete;
    MockEngine& operator=(const MockEngine&) = delete;

    // Binds the FIND and TFTP ports and starts serving. An error is for the operator.
    [[nodiscard]] std::optional<std::string> start(TargetLink& link, Config config);

    // A datagram arrived on `port`. The fake link's agent calls this directly; over a real
    // link run() feeds the datagrams it receives.
    void on_datagram(std::uint16_t port, const adl::arinc615a::Endpoint& from,
                     std::span<const std::byte> payload);

    // The loop for a real link: waits on the ports, feeds the datagrams, and lets the link
    // fire the timers, until the link asks to stop. Not used over the fake.
    void run();

    // Why run() returned of its own accord (a failed wait), or start() failed; empty when
    // the stop was asked for.
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    // MiniHost, for the MiniTransfers.
    void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to,
              std::span<const std::byte> payload) override;
    void after(adl::arinc615a::Duration delay, std::function<void()> action) override;

    // ---- state, for the tests ----
    // What happened, each line prefixed with the seconds of the link's clock.
    [[nodiscard]] const std::vector<std::string>& log() const noexcept { return log_; }
    // The files it has read from the loader, by name (the last read wins).
    [[nodiscard]] const std::map<std::string, Bytes>& stored() const noexcept { return stored_; }
    [[nodiscard]] bool operation_running() const noexcept { return operation_started_; }
    // The list's percentage 0..100, or -1 between operations.
    [[nodiscard]] int progress() const noexcept { return reported_progress_; }
    // "Information" and so on, or empty between operations.
    [[nodiscard]] std::string current_operation() const;

private:
    using ChecksumOption = adl::arinc615a::ChecksumOption;
    using Endpoint = adl::arinc615a::Endpoint;
    using Instant = adl::arinc615a::Instant;
    using StatusCode = adl::arinc615a::StatusCode;
    using TftpErrorCode = adl::arinc615a::TftpErrorCode;
    using TftpRequest = adl::arinc615a::TftpRequest;
    using MiniResult = adl::arinc615a_mini::MiniResult;
    using MiniTransfer = adl::arinc615a_mini::MiniTransfer;

    enum class Kind { information, upload, download_media, download_operator };

    struct StatusSpec {
        StatusCode code = StatusCode::in_progress;
        std::uint16_t exception_timer = 0;
        std::string text;
    };

    // One file of a load, as the header file lists it, with the check value to ask for.
    struct UploadFile {
        std::string name;
        std::optional<ChecksumOption> checksum;
    };
    // One load of the upload request being worked through.
    struct UploadLoad {
        std::string header_name;
        std::string part_number;
        std::vector<UploadFile> files;
        std::size_t files_done = 0;
    };
    // One file of a download request.
    struct DownloadRequested {
        std::string name;
        bool failed = false;
    };

    // MiniHost helpers.
    [[nodiscard]] Instant now() const;
    void note(Severity severity, std::string text);
    void report_progress();
    MiniTransfer& add_transfer(MiniTransfer::Role role);
    void remove_later(std::uint16_t port);
    void send_error(const Endpoint& to, TftpErrorCode code, std::string_view text);

    // Datagrams.
    void handle_find(const Endpoint& from, std::span<const std::byte> payload);
    void handle_request(const Endpoint& from, const TftpRequest& request);
    void serve_init(const Endpoint& from, const TftpRequest& request, Kind kind);
    void on_init_served();
    void on_list_received(const std::string& name, const Bytes& bytes);

    // Client side: writes and reads, with the DLP retry and the WAIT behaviour.
    void write_file(const std::string& name, Bytes content, std::optional<ChecksumOption> checksum,
                    std::function<void(MiniResult::Kind)> then, unsigned attempt = 0);
    void read_file(const std::string& name, std::optional<ChecksumOption> checksum,
                   std::function<void(MiniResult::Kind, const Bytes&)> then, unsigned attempt = 0);
    [[nodiscard]] std::optional<ChecksumOption> checksum_for(const Bytes& content) const;
    [[nodiscard]] std::optional<ChecksumOption> checksum_for_file(
        const adl::arinc665::UserSuppliedFile& file) const;

    // Status files.
    void post_status(StatusSpec spec);
    void pump_status();
    [[nodiscard]] Bytes build_status(const StatusSpec& spec, std::uint16_t counter);
    void start_heartbeat();
    void heartbeat_tick(std::uint64_t generation);
    [[nodiscard]] std::uint32_t progress_percent() const;

    // The operations.
    void start_work();
    void upload_read_header(std::uint64_t generation);
    void upload_read_next(std::uint64_t generation);
    void download_write_next(std::uint64_t generation);
    void finish_operation(StatusCode code, const std::string& text);
    void on_abort_seen(StatusCode status);
    void go_silent();
    void reset_operation_state();
    void hand_over_uploads();
    [[nodiscard]] Bytes build_configuration_list() const;
    [[nodiscard]] Bytes build_download_list() const;
    [[nodiscard]] static std::string operation_name(Kind kind);
    [[nodiscard]] static OperationKind operation_kind(Kind kind);

    // The link, and everything the engine owns between operations.
    TargetLink* link_ = nullptr;
    Config config_;
    EngineListener* listener_ = nullptr;
    std::vector<std::string> log_;
    std::string last_error_;
    std::map<std::uint16_t, std::unique_ptr<MiniTransfer>> transfers_;
    std::uint16_t next_port_ = 40000;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    int reported_progress_ = -1;

    // One operation's state.
    Kind kind_ = Kind::information;
    bool operation_started_ = false;
    bool finished_ = false;
    bool aborted_ = false;
    bool waited_once_ = false;
    bool silent_ = false;
    bool first_status_sent_ = false;
    Endpoint loader_{};

    // The status lane.
    std::vector<StatusSpec> status_queue_;
    bool status_busy_ = false;
    std::uint16_t counter_ = 0;
    std::size_t status_index_ = 0;
    std::uint64_t heartbeat_generation_ = 0;
    bool heartbeat_on_ = false;
    bool quiet_until_set_ = false;
    Instant quiet_until_{};

    // The data of the operation running.
    std::vector<UploadLoad> loads_;
    std::size_t loads_done_ = 0;
    std::size_t load_index_ = 0;
    std::size_t file_index_ = 0;
    std::vector<DownloadRequested> requests_;
    std::size_t request_index_ = 0;
    std::size_t writes_done_ = 0;
    std::size_t writes_total_ = 0;
    std::map<std::string, Bytes> stored_;

    std::uint64_t main_generation_ = 0;
    StatusCode last_abort_status_ = StatusCode::aborted_by_data_loader;
    bool final_posted_ = false;
    bool checksum_acknowledged_ = false;
};

}  // namespace adl::mock
