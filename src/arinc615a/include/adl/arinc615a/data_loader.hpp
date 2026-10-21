#pragma once

// The data loader's side of an ARINC 615A operation: Information (5.4.2), Upload (5.4.3, the
// short and full loads are the same protocol) and Download (5.4.4, in its media defined and
// operator defined modes), for one target at a time. It is a state machine in the same sense as
// TftpTransfer: it is given the time and the datagrams that arrive, it hands back the datagrams
// to send and the time it next wants to be called, and it holds no socket, thread, clock or file
// system. runner.hpp drives it over adl::platform; a test drives it over a simulated network.
//
// The loader is TFTP client for the target's initialization file (<THW_ID_POS>.LCI, .LUI, .LND,
// .LNO) and for the list it sends (.LUR, .LNR, .LNA), and TFTP server, on its own control port
// (59, or the one the "port" option named), for what the target starts: its status files, the
// configuration and download lists, the files of a download and, in an upload, the reads of the
// load's header and data files. Several of those run at once, each in one of a fixed number of
// slots; a slot is one TFTP transfer and, for the runner, one socket.
//
// The loader works on the bytes of the protocol files (adl/arinc615a/protocol_files.hpp) and of
// the load's files, through the FileSource and FileSink the caller supplies. It builds no ARINC
// 665 model, allocates nothing (the one exception: calculating a checksum, see checksum.hpp),
// throws nothing and needs no RTTI. All of its memory is a workspace the caller gives it.
//
// docs/arinc615a-engine.md says what the standard leaves unclear and how this resolves it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/error.hpp"
#include "adl/arinc615a/file_io.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp_transfer.hpp"
#include "adl/arinc615a/time.hpp"

namespace adl::arinc615a {

// ---- What is asked of the loader -------------------------------------------------------------

// An Information operation: the target's configuration (<THW_ID_POS>.LCL).
struct InformationOperation {};

// An Upload operation: the loads to send. The list is encoded when the operation starts; it
// need not outlive start().
struct UploadOperation {
    UploadRequest loads;
};

// A media defined Download: the files to fetch, sent as <THW_ID_POS>.LNR. The list is encoded
// and its names copied when the operation starts.
struct MediaDownloadOperation {
    DownloadRequest files;
};

// An operator defined Download: the target sends the list of files it could give
// (<THW_ID_POS>.LNL), the operation listener chooses, and the choice goes back as .LNA.
struct OperatorDownloadOperation {};

using Operation = std::variant<InformationOperation, UploadOperation, MediaDownloadOperation,
                               OperatorDownloadOperation>;

enum class OperationKind : std::uint8_t { information, upload, download_media, download_operator };

// The files chosen in an operator defined download.
class FileSelection {
public:
    static constexpr std::size_t capacity = 128;

    // False if the selection is full or the name is empty or longer than 254 characters.
    bool select(std::string_view name) noexcept;
    void clear() noexcept { count_ = 0; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] std::span<const std::string_view> names() const noexcept {
        return std::span<const std::string_view>{names_}.first(count_);
    }

private:
    std::array<std::string_view, capacity> names_{};
    std::size_t count_ = 0;
};

enum class SelectionDecision : std::uint8_t {
    selected,   // the files are in the selection: send them
    deferred,   // the choice is not made yet: call DataLoader::provide_selection() when it is
    cancelled,  // the operator wants none: abort the operation
};

// ---- What the loader reports -------------------------------------------------------------------

// Things worth telling the operator that are not the end of an operation. Each carries a name
// (a file, or empty) and a number (what is in the comment).
enum class Notice : std::uint8_t {
    status_counter_gap,    // value: status files lost between two that arrived
    status_duplicate,      // a status file with a counter already seen (a repeat)
    status_stale,          // a status file older than the last (reordered); ignored
    status_file_failed,    // a status file transfer ended in failure; the target repeats it
    checksum_mismatch,     // subject: the file; it is kept all the same (5.3.2.3.8.5)
    checksum_unsupported,  // subject: the file; value: the algorithm's number
    file_not_available,    // subject: the file; value: seconds the target was told to wait
    request_refused,       // subject: the file; the target asked for what is not allowed
    transfer_failed,       // subject: the file; value: a TransferOutcome
    waiting,               // the target said WAIT:x; value: x
    option_fallback,       // the target refused our options; we asked again without any
    port_option_ignored,   // the target did not take the "port" option
    files_missing,         // value: how many requested files never arrived
};

[[nodiscard]] constexpr const char* to_string(Notice notice) noexcept {
    switch (notice) {
        case Notice::status_counter_gap:
            return "status files lost";
        case Notice::status_duplicate:
            return "status file repeated";
        case Notice::status_stale:
            return "status file out of order";
        case Notice::status_file_failed:
            return "status file transfer failed";
        case Notice::checksum_mismatch:
            return "checksum did not match";
        case Notice::checksum_unsupported:
            return "checksum algorithm not supported";
        case Notice::file_not_available:
            return "file not available";
        case Notice::request_refused:
            return "request refused";
        case Notice::transfer_failed:
            return "transfer failed";
        case Notice::waiting:
            return "target asked to wait";
        case Notice::option_fallback:
            return "options refused, asked again without them";
        case Notice::port_option_ignored:
            return "port option not taken by the target";
        case Notice::files_missing:
            return "requested files missing";
    }
    return "unknown";
}

// Everything is passed as views that are valid only during the call: copy what must be kept.
// Every function has an empty default, so a listener overrides what it wants to show.
class OperationListener {
public:
    virtual ~OperationListener() = default;

    // [Information_Initialization_Response] and its upload and download counterparts: the
    // target's answer to the start of the operation.
    virtual void on_initialization(const InitializationFile& /*file*/) noexcept {}

    // [Information_Status], [Upload_Information_Status], [Downloading_Information_Status].
    virtual void on_configuration_status(const ConfigurationStatus& /*status*/) noexcept {}
    virtual void on_upload_status(const UploadStatus& /*status*/) noexcept {}
    virtual void on_download_status(const DownloadStatus& /*status*/) noexcept {}

    // [TH_Information]: what the target holds.
    virtual void on_configuration_list(const ConfigurationList& /*list*/) noexcept {}

    // [Downloading_File_List_Receipt] and [File_Selection]: the target's list, to choose from.
    virtual SelectionDecision on_download_list(const DownloadList& /*list*/,
                                               FileSelection& /*selection*/) noexcept {
        return SelectionDecision::cancelled;
    }

    // [Downloading_File_Receipt]: a file has arrived and been handed to the FileSink.
    virtual void on_file_received(std::string_view /*name*/,
                                  const ReceivedFile& /*file*/) noexcept {}

    // A file of the load has been sent to the target.
    virtual void on_file_sent(std::string_view /*name*/, std::uint64_t /*bytes*/) noexcept {}

    virtual void on_notice(Notice /*notice*/, std::string_view /*subject*/,
                           std::uint32_t /*value*/) noexcept {}
};

enum class OutcomeCode : std::uint8_t {
    running,
    completed,                     // the target said so (status 0x0003)
    denied,                        // the target refused the operation (0x1000)
    not_supported,                 // the target does not do it (0x1002)
    aborted_by_operator,           // 0x1005
    aborted_by_data_loader,        // 0x1004
    aborted_by_target,             // 0x1003
    unsupported_protocol_version,  // the target's files are not version "A4"
    transfer_failed,               // a file could not be transferred, after the DLP retry
    dlp_timed_out,                 // no status file within the DLP time-out (13 s)
    exception_timer_expired,       // the target stayed silent longer than it said it would
    protocol_error,                // a file of an invalid structure, or one sent out of turn
    local_error,  // the FileSource or FileSink failed, or the request would not fit
};

[[nodiscard]] constexpr const char* to_string(OutcomeCode code) noexcept {
    switch (code) {
        case OutcomeCode::running:
            return "running";
        case OutcomeCode::completed:
            return "completed";
        case OutcomeCode::denied:
            return "denied by the target";
        case OutcomeCode::not_supported:
            return "not supported by the target";
        case OutcomeCode::aborted_by_operator:
            return "aborted by the operator";
        case OutcomeCode::aborted_by_data_loader:
            return "aborted by the data loader";
        case OutcomeCode::aborted_by_target:
            return "aborted by the target";
        case OutcomeCode::unsupported_protocol_version:
            return "unsupported protocol version";
        case OutcomeCode::transfer_failed:
            return "a file transfer failed";
        case OutcomeCode::dlp_timed_out:
            return "data load protocol time-out";
        case OutcomeCode::exception_timer_expired:
            return "the target was silent longer than its exception timer";
        case OutcomeCode::protocol_error:
            return "protocol error";
        case OutcomeCode::local_error:
            return "local error";
    }
    return "unknown";
}

// How an operation ended (`code` is `running` until it has), with the last status the
// target reported and counts of the status files and the files seen.
struct Outcome {
    OutcomeCode code = OutcomeCode::running;
    // The operation status of the last status file seen, if any came.
    std::optional<StatusCode> last_status;
    // For transfer_failed: how the last attempt ended.
    TransferOutcome failed_transfer = TransferOutcome::running;
    // After an abort of ours: the target's own status file said it had stopped.
    bool abort_confirmed = false;

    std::uint32_t statuses_received = 0;
    std::uint32_t statuses_lost = 0;      // by the counter
    std::uint32_t statuses_repeated = 0;  // duplicates and out of order, ignored
    std::uint32_t files_requested = 0;    // a download: the files asked for
    std::uint32_t files_received = 0;
    std::uint32_t files_served = 0;  // an upload: files the target read from us
    std::uint32_t items_failed = 0;  // loads (or files) the last status listed as failed
    std::uint32_t checksum_mismatches = 0;
};

// ---- Set-up ----------------------------------------------------------------------------------

// The longest <THW_ID_POS> the loader takes, so that the protocol files named after it
// (<THW_ID_POS>.LCI and the others) stay within 254 characters.
inline constexpr std::size_t max_thw_id_pos_length = 240;

// Whether `id` can be the <THW_ID_POS> of a LoaderConfig: not empty, at most
// max_thw_id_pos_length characters, and none of a control character, a '/' or a backslash, which
// would make the name of a protocol file a path. DataLoader::start refuses anything else with
// Error::invalid_value; a caller that makes something from the id first (a directory) asks
// beforehand.
[[nodiscard]] bool is_valid_thw_id_pos(std::string_view id) noexcept;

// How the loader is set up: the target and the port the loader listens on, block
// sizes, the time-outs and retries of Attachment 4, and the sizes of its buffers.
struct LoaderConfig {
    // The target's TFTP control endpoint: its address and port 59.
    Endpoint target{{}, tftp_control_port};
    // The <THW_ID_POS> of the target, as its protocol files are named (is_valid_thw_id_pos).
    std::string_view thw_id_pos;
    // The port the loader listens on for what the target starts. Anything but 59 is sent to the
    // target in the "port" option of the initialization request (5.3.2.3.8.6).
    std::uint16_t control_port = tftp_control_port;

    // The block size our own requests ask for, and the most we agree to when the target asks.
    std::size_t preferred_block_size = 1024;
    std::size_t max_block_size = 4096;

    // Attachment 4. The defaults are the standard's.
    Duration tftp_timeout = adl::arinc615a::tftp_timeout;
    unsigned tftp_retries = adl::arinc615a::tftp_retries;
    Duration dlp_timeout = adl::arinc615a::dlp_timeout;
    unsigned dlp_retries = adl::arinc615a::dlp_retries;
    Duration dally = tftp_dally;

    // How many times a target may answer one of our requests with WAIT before we give up.
    unsigned max_waits = 10;

    // At most this many transfers at once, in all (our own requests included), up to
    // DataLoader::max_slots.
    std::size_t max_transfers = 6;

    // The largest protocol file we receive or send, and so the size of each slot's buffer for
    // one (a configuration list, a download list, a status file listing many loads).
    std::size_t max_protocol_file_bytes = std::size_t{32} * 1024;

    // Room for the names of the files a download expects.
    std::size_t name_pool_bytes = std::size_t{16} * 1024;
};

// What the loader calls out to: where an upload reads the load's files, where a
// download writes the target's, and who is told of progress. With no source or sink
// the loader refuses the target's requests for files; a listener is optional.
struct LoaderServices {
    FileSource* source = nullptr;  // an upload reads the load's files through this
    FileSink* sink = nullptr;      // a download writes the target's files through this
    OperationListener* listener = nullptr;
};

// What the runner is to do after each call, in order.
struct Command {
    enum class Kind : std::uint8_t {
        send_from_slot,     // send `bytes` to `to` from the socket of slot `slot`
        send_from_control,  // send `bytes` to `to` from the control socket
        release_slot,       // close the socket of slot `slot`
    };
    Kind kind = Kind::release_slot;
    std::uint8_t slot = 0;
    Endpoint to;
    std::span<const std::byte> bytes;
};

// The data loader's side of one operation. Construct it with its workspace, call
// start(), then give it the datagrams that arrive and the time, running commands()
// after each call, until done() and idle(). One operation per DataLoader.
class DataLoader {
public:
    static constexpr std::size_t max_slots = 8;
    static constexpr std::size_t max_expected_files = 256;
    static constexpr std::size_t max_commands = 64;

    // The bytes of workspace a configuration needs (the transfer buffers, the protocol file
    // buffers and the pool of names). The workspace must be aligned for std::byte only.
    [[nodiscard]] static std::size_t workspace_bytes(const LoaderConfig& config) noexcept;

    // `workspace` is used until the DataLoader is destroyed. It is checked in start().
    DataLoader(const LoaderConfig& config, std::span<std::byte> workspace,
               const LoaderServices& services) noexcept;
    DataLoader(const DataLoader&) = delete;
    DataLoader& operator=(const DataLoader&) = delete;

    // Begins an operation: validates the request, encodes the list, and sends the request for
    // the initialization file. Error::buffer_too_small if the workspace is smaller than
    // workspace_bytes() or a list does not fit a protocol file buffer, Error::field_too_large
    // for more than max_expected_files files or a name pool that is too small, Error::
    // invalid_value for a bad <THW_ID_POS> or configuration. One operation per DataLoader.
    [[nodiscard]] Result<void> start(Instant now, const Operation& operation) noexcept;

    // ---- driving; after each call, run take_commands() ----------------------------------------

    // A datagram arrived on the control socket (a request from the target).
    void on_control_datagram(Instant now, const Endpoint& from,
                             std::span<const std::byte> bytes) noexcept;
    // A datagram arrived on the socket of `slot`.
    void on_transfer_datagram(Instant now, std::size_t slot, const Endpoint& from,
                              std::span<const std::byte> bytes) noexcept;
    // Time passed. Call it at next_wake() (or any time: it does only what is due).
    void on_timer(Instant now) noexcept;

    // The operator's abort (6.2.17), or the protocol's own (6.3.5): `status` is
    // aborted_by_operator or aborted_by_data_loader. Other transfers are ended at once with an
    // ABORT message; the target's next status file write request is answered with one, and the
    // operation ends when the target's status file confirms, or after the DLP time-out.
    void abort(Instant now, StatusCode status) noexcept;

    // Gives the file choice of an operator defined download that on_download_list() deferred.
    // Error::invalid_value if no choice is awaited.
    [[nodiscard]] Result<void> provide_selection(Instant now,
                                                 std::span<const std::string_view> names) noexcept;

    // ---- what to do ------------------------------------------------------------------------

    // The datagrams to send and sockets to close, until the next call.
    [[nodiscard]] std::span<const Command> commands() const noexcept {
        return std::span<const Command>{commands_}.first(command_count_);
    }
    // The same commands, handed over once: the next take_commands() (or commands()) before the
    // next call returns none. A driver that carries them out after each of its steps, whether
    // that step called the loader or not, uses this one, since carrying a command out twice is
    // not harmless (a repeated "close the socket of slot n" closes the socket a new transfer
    // has been given in that slot since). The span is valid until the next call, as above.
    [[nodiscard]] std::span<const Command> take_commands() noexcept {
        const auto taken = std::span<const Command>{commands_}.first(command_count_);
        command_count_ = 0;
        return taken;
    }
    // When on_timer() should next be called, or `never`.
    [[nodiscard]] Instant next_wake() const noexcept;

    // ---- state -----------------------------------------------------------------------------

    // The operation has an outcome (it may still have transfers lingering, see idle()).
    [[nodiscard]] bool done() const noexcept { return outcome_.code != OutcomeCode::running; }
    // Done, and every transfer has ended: nothing more will be asked of the runner.
    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] const Outcome& outcome() const noexcept { return outcome_; }
    [[nodiscard]] std::uint16_t control_port() const noexcept { return config_.control_port; }
    // The size of buffer a datagram for the loader must fit: the largest block plus its header,
    // and at least a request with a long file name and options. A datagram that does not fit
    // was cut short and is to be dropped.
    [[nodiscard]] std::size_t datagram_buffer_bytes() const noexcept {
        return config_.max_block_size + 4 > 2048 ? config_.max_block_size + 4 : 2048;
    }
    [[nodiscard]] std::size_t slot_count() const noexcept { return slot_count_; }

private:
    enum class Purpose : std::uint8_t {
        none,
        init_file,      // we read <THW_ID_POS>.LCI/.LUI/.LND/.LNO
        list_write,     // we write .LUR, .LNR or .LNA
        status_file,    // the target writes .LCS/.LUS/.LNS
        config_list,    // the target writes .LCL
        download_list,  // the target writes .LNL
        data_in,        // the target writes a file of a download
        data_out,       // the target reads a file of an upload
    };

    // The BlockSink and BlockSource of one slot: a protocol file in memory, or a file through
    // the FileSink/FileSource.
    class SlotIo final : public BlockSink, public BlockSource {
    public:
        enum class Mode : std::uint8_t { none, memory_sink, memory_source, file_sink, file_source };

        IoResult<void> write(std::span<const std::byte> data) noexcept override;
        std::uint64_t size() const noexcept override;
        IoResult<std::size_t> read(std::uint64_t offset,
                                   std::span<std::byte> out) noexcept override;

        Mode mode = Mode::none;
        std::span<std::byte> buffer;      // memory_sink: where it is collected
        std::size_t length = 0;           // memory_sink: bytes so far
        std::span<const std::byte> data;  // memory_source
        FileSink* sink = nullptr;
        SinkHandle sink_handle = 0;
        FileSource* source = nullptr;
        SourceHandle source_handle = 0;
        std::uint64_t source_size = 0;
    };

    struct Slot {
        TftpTransfer transfer;
        SlotIo io;
        Purpose purpose = Purpose::none;
        bool used = false;
        bool handled = false;        // the end of the transfer has been dealt with
        bool suspends = false;       // counted in suspend_count_ while it runs
        bool retry_pending = false;  // a client transfer to start again at retry_at
        bool plain = false;          // ask again without options
        unsigned attempts = 0;       // DLP retries used
        unsigned waits = 0;
        Instant retry_at = never;
        std::size_t expected_index = no_index;
        std::array<char, 256> name{};
        std::size_t name_length = 0;
        std::span<std::byte> file;  // the buffer a protocol file is collected in

        [[nodiscard]] std::string_view name_view() const noexcept {
            return {name.data(), name_length};
        }
    };

    struct Expected {
        std::size_t name_offset = 0;
        std::size_t name_length = 0;
        bool received = false;
        bool in_progress = false;
    };

    static constexpr std::size_t no_index = static_cast<std::size_t>(-1);

    // data_loader.cpp
    void begin_calls() noexcept;
    void push(const Command& command) noexcept;
    void pump(Instant now, std::size_t index) noexcept;
    void run_retries(Instant now) noexcept;
    [[nodiscard]] std::size_t allocate_slot() noexcept;
    void release_slot(std::size_t index) noexcept;
    void start_client(Instant now, std::size_t index) noexcept;
    void handle_request(Instant now, const Endpoint& from, const TftpRequest& request) noexcept;
    void refuse(const Endpoint& to, TftpErrorCode code, std::string_view text) noexcept;
    void refuse_wait(const Endpoint& to, std::uint16_t seconds) noexcept;
    void refuse_abort(const Endpoint& to) noexcept;
    void begin_suspension(Slot& slot) noexcept;
    void end_suspension(Instant now, Slot& slot) noexcept;
    void end_calls(Instant now) noexcept;
    void do_abort(Instant now, StatusCode status) noexcept;
    void notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept;
    [[nodiscard]] TransferSettings transfer_settings() const noexcept;
    [[nodiscard]] bool name_matches(std::string_view stem) const noexcept;

    // data_loader_operations.cpp
    void on_slot_finished(Instant now, std::size_t index) noexcept;
    void on_client_finished(Instant now, Slot& slot) noexcept;
    void on_server_finished(Instant now, Slot& slot) noexcept;
    void on_init_file(Instant now, Slot& slot) noexcept;
    void on_list_written(Instant now) noexcept;
    void on_status_file(Instant now, Slot& slot) noexcept;
    void on_configuration_list(Instant now, Slot& slot) noexcept;
    void on_download_list(Instant now, Slot& slot) noexcept;
    void on_data_received(Instant now, Slot& slot) noexcept;
    void apply_status(Instant now, StatusCode status, std::uint16_t exception_timer) noexcept;
    [[nodiscard]] bool take_counter(std::uint16_t counter) noexcept;
    void send_list(Instant now) noexcept;
    void send_selection(Instant now, std::span<const std::string_view> names) noexcept;
    void finish_operation(Instant now, OutcomeCode code) noexcept;
    void finish_aborted(Instant now, bool confirmed) noexcept;
    void abort_active(Instant now, StatusCode status) noexcept;
    void arm_watchdog(Instant now, Duration window, bool from_exception) noexcept;
    void check_watchdog(Instant now) noexcept;
    void check_completion(Instant now) noexcept;
    [[nodiscard]] std::size_t active_data_transfers() const noexcept;
    [[nodiscard]] Result<void> expect_file(std::string_view name) noexcept;
    [[nodiscard]] std::size_t find_expected(std::string_view name) const noexcept;

    LoaderConfig config_;
    LoaderServices services_;
    std::span<std::byte> workspace_;
    bool laid_out_ = false;

    // Aggregate value-initialisation zero-fills before the member initialisers
    // run; the check reads that zero as an enum value.
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    std::array<Slot, max_slots> slots_{};
    std::size_t slot_count_ = 0;
    std::span<std::byte> outbound_;  // the encoded .LUR, .LNR or .LNA
    std::size_t outbound_length_ = 0;
    std::span<char> names_;  // the pool the expected names are copied into
    std::size_t names_used_ = 0;
    std::array<Expected, max_expected_files> expected_{};
    std::size_t expected_count_ = 0;

    std::array<Command, max_commands> commands_{};
    std::size_t command_count_ = 0;
    std::array<std::byte, 128> reply_{};  // control socket replies (errors, WAIT, ABORT)

    char init_name_[260]{};

    // The operation.
    Outcome outcome_;
    bool started_ = false;
    OperationKind kind_ = OperationKind::information;
    bool accepted_ = false;   // the initialization file said "accepted"
    bool list_sent_ = false;  // .LUR/.LNR/.LNA has gone
    bool list_started_ = false;
    bool list_received_ = false;  // .LCL or .LNL has arrived
    bool selection_pending_ = false;
    bool completing_ = false;  // the target said 0x0003; waiting for its last transfers

    // The abort path.
    std::optional<StatusCode> abort_status_;
    bool abort_answered_ = false;
    std::optional<OutcomeCode> abort_outcome_;  // set when the abort is the protocol's own

    // Status files.
    bool have_status_ = false;
    std::uint16_t last_counter_ = 0;
    std::optional<StatusCode> last_status_;

    // The DLP time-out.
    bool watching_ = false;
    Instant watch_deadline_ = never;
    Duration watch_window_ = dlp_timeout;
    bool watch_from_exception_ = false;
    std::size_t suspend_count_ = 0;
};

}  // namespace adl::arinc615a
