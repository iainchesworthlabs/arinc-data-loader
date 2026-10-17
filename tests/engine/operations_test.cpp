// The data loader with every datagram written out by hand: what it sends and when, what it
// refuses, and the figures of Attachment 4, without a network or a target in the way.

#include <catch2/catch_test_macros.hpp>

#include "rig.hpp"

using namespace adl::arinc615a;
using namespace adl::test;

namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

constexpr Endpoint target_control{Address{{10, 0, 0, 2}}, 59};
constexpr Endpoint target_tid{Address{{10, 0, 0, 2}}, 4000};
constexpr Endpoint stranger{Address{{10, 0, 0, 9}}, 4000};

struct Sent {
    Command::Kind kind;
    std::uint8_t slot;
    Endpoint to;
    Bytes bytes;
};

Bytes init_file(StatusCode status = StatusCode::accepted, const char* version = "A4") {
    Bytes file(300);
    const auto size = encode_initialization_file(InitializationFile{status, ""}, file);
    file.resize(size.value_or(0));
    file[4] = static_cast<std::byte>(version[0]);
    file[5] = static_cast<std::byte>(version[1]);
    return file;
}

Bytes status_file(StatusCode status, std::uint16_t counter, std::uint16_t exception = 0) {
    Bytes file(300);
    const auto size = encode_configuration_status(
        ConfigurationStatus{counter, status, exception, estimated_time_unknown, ""}, file);
    file.resize(size.value_or(0));
    return file;
}

// A DataLoader driven by hand, with what it asks for written down.
class Harness {
public:
    explicit Harness(const LoaderConfig& base = {}) : config(base) {
        config.target = target_control;
        config.thw_id_pos = "HNPFMS_L";
        workspace.assign(DataLoader::workspace_bytes(config), std::byte{0});
        LoaderServices services;
        services.listener = &recorder;
        services.sink = &sink;
        services.source = &source;
        loader = std::make_unique<DataLoader>(config, workspace, services);
    }

    Result<void> start(const Operation& operation = InformationOperation{}) {
        const auto started = loader->start(now, operation);
        collect();
        return started;
    }

    void collect() {
        for (const Command& command : loader->commands()) {
            Sent sent{command.kind, command.slot, command.to,
                      Bytes(command.bytes.begin(), command.bytes.end())};
            if (command.kind != Command::Kind::release_slot) {
                // Whatever the loader sends is a packet the codec can read.
                CHECK(decode_packet(sent.bytes).has_value());
            }
            log.push_back(std::move(sent));
        }
    }

    std::vector<Sent> drain() {
        std::vector<Sent> out = std::move(log);
        log.clear();
        return out;
    }

    void to_slot(std::size_t slot, const TftpPacket& packet, const Endpoint& from = target_tid) {
        const Bytes bytes = mini_encode(packet);
        loader->on_transfer_datagram(now, slot, from, bytes);
        collect();
    }
    void to_control(const TftpPacket& packet, const Endpoint& from = target_tid) {
        const Bytes bytes = mini_encode(packet);
        loader->on_control_datagram(now, from, bytes);
        collect();
    }
    void advance_to(Instant at) {
        now = at;
        loader->on_timer(now);
        collect();
    }
    void advance(Duration by) { advance_to(now + by); }

    // The target hands the initialization file over and the operation is under way.
    void accept(StatusCode status = StatusCode::accepted, const char* version = "A4") {
        REQUIRE(start());
        drain();
        to_slot(0, TftpData{1, init_file(status, version)});
    }

    static std::optional<TftpPacket> decoded(const Sent& sent) {
        const auto packet = decode_packet(sent.bytes);
        return packet ? std::optional<TftpPacket>(*packet) : std::nullopt;
    }

    LoaderConfig config;
    std::vector<std::byte> workspace;
    Recorder recorder;
    MemoryFileSink sink;
    MediaSource source{MediaFiles{}};
    std::unique_ptr<DataLoader> loader;
    Instant now{};
    std::vector<Sent> log;
};

TftpRequest write_request(std::string_view name) {
    return TftpRequest{TftpRequest::Kind::write, name, {}};
}

TftpRequest read_request(std::string_view name) {
    return TftpRequest{TftpRequest::Kind::read, name, {}};
}

}  // namespace

TEST_CASE("the first thing sent is the request for the initialization file", "[engine][loader]") {
    for (const bool moved : {false, true}) {
        LoaderConfig config;
        config.control_port = moved ? 1615 : 59;
        config.preferred_block_size = 1024;
        Harness harness(config);
        REQUIRE(harness.start());
        const auto sent = harness.drain();
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].kind == Command::Kind::send_from_slot);
        CHECK(sent[0].slot == 0);
        CHECK(sent[0].to == target_control);
        const auto packet = Harness::decoded(sent[0]);
        REQUIRE(packet);
        const auto* request = std::get_if<TftpRequest>(&*packet);
        REQUIRE(request != nullptr);
        CHECK(request->kind == TftpRequest::Kind::read);
        CHECK(request->file_name == "HNPFMS_L.LCI");
        const auto options = read_transfer_options(request->options);
        REQUIRE(options);
        CHECK(options->block_size == 1024);
        CHECK(options->transfer_size == 0);
        // 5.3.2.3.8.6: the port option is how the loader moves its control port off 59.
        CHECK(options->port == (moved ? std::optional<std::uint16_t>(1615) : std::nullopt));
    }
}

TEST_CASE("each operation asks for its own initialization file", "[engine][loader]") {
    static const std::array<LoadReference, 1> loads{LoadReference{"A.LUH", "ACM47-1234-5678"}};
    const std::array<std::pair<Operation, const char*>, 4> cases{{
        {InformationOperation{}, "HNPFMS_L.LCI"},
        {UploadOperation{UploadRequest{loads}}, "HNPFMS_L.LUI"},
        {MediaDownloadOperation{}, "HNPFMS_L.LND"},
        {OperatorDownloadOperation{}, "HNPFMS_L.LNO"},
    }};
    for (const auto& [operation, name] : cases) {
        Harness harness;
        // (a list of no files is a valid .LNR; a .LUR of no loads is refused, so give it one)
        REQUIRE(harness.start(operation));
        const auto sent = harness.drain();
        REQUIRE(sent.size() == 1);
        const auto packet = Harness::decoded(sent[0]);
        REQUIRE(packet);
        CHECK(std::get<TftpRequest>(*packet).file_name == name);
    }
}

TEST_CASE("a request that cannot be made is refused when the operation starts",
          "[engine][loader]") {
    SECTION("a load list with no loads") {
        Harness harness;
        const auto started = harness.start(UploadOperation{});
        REQUIRE_FALSE(started);
        CHECK(started.error() == Error::invalid_value);
    }
    SECTION("a name with a path in it") {
        LoaderConfig config;
        Harness harness(config);
        harness.config.thw_id_pos = "A/B";
        harness.loader =
            std::make_unique<DataLoader>(harness.config, harness.workspace, LoaderServices{});
        REQUIRE_FALSE(harness.start());
    }
    SECTION("a block size outside the protocol's limits") {
        LoaderConfig config;
        config.preferred_block_size = 4;
        Harness harness(config);
        REQUIRE_FALSE(harness.start());
    }
    SECTION("too many files to expect") {
        std::vector<FileRequest> files;
        std::vector<std::string> names;
        for (std::size_t i = 0; i <= DataLoader::max_expected_files; ++i) {
            names.push_back("F" + std::to_string(i) + ".BIN");
        }
        for (const std::string& name : names) {
            files.push_back(FileRequest{name, {}});
        }
        LoaderConfig config;
        config.max_protocol_file_bytes = 64 * 1024;
        Harness harness(config);
        const auto started = harness.start(MediaDownloadOperation{DownloadRequest{files}});
        REQUIRE_FALSE(started);
        CHECK(started.error() == Error::field_too_large);
    }
    SECTION("a list too large for a protocol file") {
        std::vector<FileRequest> files(300, FileRequest{"A_FAIRLY_LONG_NAME_FOR_A_FILE.BIN", {}});
        LoaderConfig config;
        config.max_protocol_file_bytes = 1024;
        Harness harness(config);
        const auto started = harness.start(MediaDownloadOperation{DownloadRequest{files}});
        REQUIRE_FALSE(started);
        CHECK(started.error() == Error::buffer_too_small);
    }
}

TEST_CASE("the request is repeated after two seconds and the transfer retried once",
          "[engine][loader][timeout]") {
    Harness harness;
    REQUIRE(harness.start());
    const auto first = harness.drain();
    REQUIRE(first.size() == 1);
    CHECK(harness.loader->next_wake() == Instant{seconds(2)});

    harness.advance_to(Instant{seconds(2)});
    auto sent = harness.drain();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].bytes == first[0].bytes);  // the same request again

    // At four seconds the transfer has failed, and is made again at once (Attachment 4, 4-4.2).
    harness.advance_to(Instant{seconds(4)});
    sent = harness.drain();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].bytes == first[0].bytes);
    CHECK(harness.loader->next_wake() == Instant{seconds(6)});

    harness.advance_to(Instant{seconds(6)});
    CHECK(harness.drain().size() == 1);
    harness.advance_to(Instant{seconds(8)});
    CHECK(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::transfer_failed);
    CHECK(harness.loader->outcome().failed_transfer == TransferOutcome::timed_out);
    CHECK(harness.loader->idle());
    CHECK(harness.loader->next_wake() == never);
}

TEST_CASE("a target's request before the operation is under way is refused", "[engine][loader]") {
    Harness harness;
    REQUIRE(harness.start());
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto sent = harness.drain();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].kind == Command::Kind::send_from_control);
    CHECK(sent[0].to == target_tid);
    const auto packet = Harness::decoded(sent[0]);
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::illegal_operation);
}

TEST_CASE("datagrams from a host other than the target are ignored", "[engine][loader]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"), stranger);
    CHECK(harness.drain().empty());
    harness.to_slot(0, TftpData{1, Bytes(10)}, stranger);
    CHECK(harness.drain().empty());
    // And the control socket ignores what is not a request.
    harness.to_control(TftpAck{0});
    CHECK(harness.drain().empty());
    const std::array<std::byte, 2> garbage{std::byte{0xFF}, std::byte{0xFF}};
    harness.loader->on_control_datagram(harness.now, target_tid, garbage);
    CHECK(harness.loader->commands().empty());
}

TEST_CASE("a status file is accepted on a socket of its own", "[engine][loader]") {
    Harness harness;
    harness.accept();
    const auto init_ack = harness.drain();
    REQUIRE(init_ack.size() == 1);
    CHECK(init_ack[0].slot == 0);

    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto sent = harness.drain();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].kind == Command::Kind::send_from_slot);
    CHECK(sent[0].slot == 1);  // slot 0 is still lingering after the initialization file
    CHECK(sent[0].to == target_tid);
    const auto packet = Harness::decoded(sent[0]);
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpAck>(*packet));
    CHECK(std::get<TftpAck>(*packet).block == 0);

    // The file arrives; it is shown, and acknowledged.
    harness.to_slot(1, TftpData{1, status_file(StatusCode::in_progress, 0)});
    const auto ack = harness.drain();
    REQUIRE_FALSE(ack.empty());
    CHECK(ack[0].kind == Command::Kind::send_from_slot);
    CHECK(std::get<TftpAck>(*Harness::decoded(ack[0])).block == 1);
    CHECK(harness.recorder.status_counters == std::vector<std::uint16_t>{0});
    CHECK(harness.recorder.status_codes == std::vector<StatusCode>{StatusCode::in_progress});
}

TEST_CASE("a repeated request does not start a second transfer", "[engine][loader]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto first = harness.drain();
    REQUIRE(first.size() == 1);
    harness.to_control(write_request("HNPFMS_L.LCS"));  // the target never saw our ACK 0
    const auto again = harness.drain();
    REQUIRE(again.size() == 1);
    CHECK(again[0].slot == first[0].slot);
    CHECK(again[0].bytes == first[0].bytes);
}

TEST_CASE("a newer request for the status file replaces one that was abandoned",
          "[engine][loader]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"), target_tid);
    harness.drain();
    // The target gave up on that one and asks again from another port.
    const Endpoint other{target_tid.address, 4001};
    harness.to_control(write_request("HNPFMS_L.LCS"), other);
    const auto sent = harness.drain();
    REQUIRE_FALSE(sent.empty());
    CHECK(sent.back().to == other);
    // The old transfer was dropped without a word to the target: nothing was sent to it.
    for (const Sent& datagram : sent) {
        if (datagram.kind == Command::Kind::send_from_slot) {
            CHECK(datagram.to == other);
        }
    }
    // The newer one works.
    harness.to_slot(sent.back().slot, TftpData{1, status_file(StatusCode::in_progress, 0)}, other);
    CHECK(harness.recorder.status_counters == std::vector<std::uint16_t>{0});
}

TEST_CASE("files that are not part of the operation are refused", "[engine][loader]") {
    struct Case {
        TftpRequest request;
        TftpErrorCode code;
    };
    const std::array<Case, 6> cases{{
        {write_request("HNPFMS_L.LUS"), TftpErrorCode::access_violation},  // another operation's
        {write_request("HNPFMS_L.LNS"), TftpErrorCode::access_violation},
        {write_request("OTHER_L.LCS"), TftpErrorCode::access_violation},  // another target's
        {write_request("HNPFMS_L.LNL"), TftpErrorCode::access_violation},
        {write_request("DATA.BIN"), TftpErrorCode::access_violation},  // not a download
        {read_request("APP.LUP"), TftpErrorCode::access_violation},    // not an upload
    }};
    for (const Case& scenario : cases) {
        CAPTURE(scenario.request.file_name);
        Harness harness;
        harness.accept();
        harness.drain();
        harness.to_control(scenario.request);
        const auto sent = harness.drain();
        REQUIRE(sent.size() == 1);
        CHECK(sent[0].kind == Command::Kind::send_from_control);
        const auto packet = Harness::decoded(sent[0]);
        REQUIRE(packet);
        REQUIRE(std::holds_alternative<TftpError>(*packet));
        CHECK(std::get<TftpError>(*packet).code == scenario.code);
        CHECK(harness.recorder.count(Notice::request_refused) == 1);
    }
}

TEST_CASE("when every transfer is in use the target is told error 3", "[engine][loader]") {
    LoaderConfig config;
    config.max_transfers = 2;
    config.dally = milliseconds(100);
    Harness harness(config);
    MediaFiles files;
    for (const char* name : {"A.BIN", "B.BIN", "C.BIN"}) {
        files.entries.push_back(MediaFiles::Entry{name, "\\", 1, Bytes(10, std::byte{1}), 0});
    }
    harness.source = MediaSource{files};
    static const std::array<LoadReference, 1> loads{LoadReference{"A.LUH", "ACM47-1234-5678"}};
    REQUIRE(harness.start(UploadOperation{UploadRequest{loads}}));
    harness.drain();
    harness.to_slot(0, TftpData{1, init_file()});
    harness.advance(seconds(1));  // the initialization file's transfer has finished lingering
    harness.drain();

    harness.to_control(read_request("A.BIN"), Endpoint{target_tid.address, 4001});
    harness.to_control(read_request("B.BIN"), Endpoint{target_tid.address, 4002});
    CHECK(harness.drain().size() == 2);  // a block each, from sockets of their own
    harness.to_control(read_request("C.BIN"), Endpoint{target_tid.address, 4003});
    const auto sent = harness.drain();
    // One slot for each of the first two, none for the third.
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].kind == Command::Kind::send_from_control);
    const auto packet = Harness::decoded(sent[0]);
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::disk_full);
    // The files that were opened for the two are closed when the transfers are.
    harness.to_slot(0, TftpAck{1}, Endpoint{target_tid.address, 4001});
    CHECK(harness.source.open_files() == 1);
}

TEST_CASE("the time-out of thirteen seconds runs from the end of the initialization file",
          "[engine][loader][timeout]") {
    Harness harness;
    harness.now = Instant{seconds(100)};
    harness.accept();
    const Instant accepted = harness.now;
    harness.drain();
    // Nothing before the thirteen seconds are up...
    harness.advance_to(accepted + seconds(13) - Duration(1));
    CHECK_FALSE(harness.loader->done());
    // ...and at the instant they are, the operation has failed.
    harness.advance_to(accepted + seconds(13));
    REQUIRE(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::dlp_timeout);
}

TEST_CASE("a status file's end starts the thirteen seconds again", "[engine][loader][timeout]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.advance(seconds(5));
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto reply = harness.drain();
    const std::size_t slot = reply.back().slot;
    // While a status file is being transferred the time-out is not running...
    harness.advance(seconds(1));
    harness.to_slot(slot, TftpData{1, status_file(StatusCode::in_progress, 0)});
    const Instant ended = harness.now;
    // ...and at its end the thirteen seconds start afresh.
    harness.advance_to(ended + seconds(13) - Duration(1));
    CHECK_FALSE(harness.loader->done());
    harness.advance_to(ended + seconds(13));
    CHECK(harness.loader->outcome().code == OutcomeCode::dlp_timeout);
}

TEST_CASE("a file that takes longer than the time-out to arrive does not end the operation",
          "[engine][loader][timeout]") {
    // A block every 1.5 seconds: each exchange is within the TFTP time-out, the whole is not
    // within the data load time-out. While a status file, or the list when no status file has
    // come, is arriving, that time-out is not running.
    for (const char* name : {"HNPFMS_L.LCS", "HNPFMS_L.LCL"}) {
        CAPTURE(name);
        Harness harness;
        harness.accept();
        harness.drain();
        harness.to_control(write_request(name));
        const std::size_t slot = harness.drain().back().slot;
        for (std::uint16_t block = 1; block <= 12; ++block) {
            harness.advance(milliseconds(1500));
            harness.to_slot(slot, TftpData{block, Bytes(512, std::byte{0})});
            harness.drain();
            REQUIRE_FALSE(harness.loader->done());
        }
        CHECK(harness.now.since_origin > seconds(13));
        CHECK_FALSE(harness.loader->done());
    }
}

namespace {

Bytes big_configuration_list() {
    static const std::array<HardwarePart, 1> parts{
        HardwarePart{"ACM47-1234-5678", "", "FMS 1 OPS"}};
    static const std::vector<HardwareItem> items(40, HardwareItem{"FMS LEFT", "SN-0001", parts});
    Bytes bytes(16384);
    const auto size = encode_configuration_list(ConfigurationList{items}, bytes);
    bytes.resize(size.value_or(0));
    return bytes;
}

}  // namespace

TEST_CASE("a status file that says completed waits for a list still coming",
          "[engine][loader][status]") {
    Harness harness;
    harness.accept();
    harness.drain();
    const Bytes list = big_configuration_list();
    REQUIRE(list.size() > 1024);
    const Endpoint list_peer{target_tid.address, 4400};
    harness.to_control(write_request("HNPFMS_L.LCL"), list_peer);
    const std::size_t list_slot = harness.drain().back().slot;
    const auto send_block = [&](std::size_t index) {
        const std::size_t start = index * 512;
        const std::size_t count =
            start >= list.size() ? 0 : std::min<std::size_t>(512, list.size() - start);
        harness.to_slot(list_slot,
                        TftpData{static_cast<std::uint16_t>(index + 1),
                                 std::span<const std::byte>{list}.subspan(start, count)},
                        list_peer);
        harness.drain();
    };
    send_block(0);

    // The last status file, 0x0003, arrives on its own transfer while the list is still coming.
    const Endpoint status_peer{target_tid.address, 4401};
    harness.to_control(write_request("HNPFMS_L.LCS"), status_peer);
    const std::size_t status_slot = harness.drain().back().slot;
    harness.to_slot(status_slot, TftpData{1, status_file(StatusCode::completed, 0)}, status_peer);
    harness.drain();
    CHECK_FALSE(harness.loader->done());

    // The operation is complete when the list is.
    std::size_t index = 1;
    while (index * 512 <= list.size()) {
        CHECK_FALSE(harness.loader->done());
        send_block(index++);
    }
    REQUIRE(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::completed);
    CHECK(harness.recorder.configuration_lists == 1);
}

TEST_CASE("an exception timer longer than thirteen seconds is waited for",
          "[engine][loader][timeout]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const std::size_t slot = harness.drain().back().slot;
    harness.to_slot(slot, TftpData{1, status_file(StatusCode::in_progress, 0, 40)});
    const Instant ended = harness.now;
    harness.advance_to(ended + seconds(39));
    CHECK_FALSE(harness.loader->done());
    harness.advance_to(ended + seconds(40));
    CHECK(harness.loader->outcome().code == OutcomeCode::exception_timer_expired);

    // One shorter than the time-out changes nothing.
    Harness shorter;
    shorter.accept();
    shorter.drain();
    shorter.to_control(write_request("HNPFMS_L.LCS"));
    const std::size_t slot2 = shorter.drain().back().slot;
    shorter.to_slot(slot2, TftpData{1, status_file(StatusCode::in_progress, 0, 3)});
    const Instant ended2 = shorter.now;
    shorter.advance_to(ended2 + seconds(13));
    CHECK(shorter.loader->outcome().code == OutcomeCode::dlp_timeout);
}

TEST_CASE("an abort is answered at the first status file request, and confirmed by the next",
          "[engine][loader][abort]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.loader->abort(harness.now, StatusCode::aborted_by_operator);
    harness.collect();
    CHECK(harness.drain().empty());  // nothing is open to be told
    CHECK_FALSE(harness.loader->done());

    harness.advance(seconds(3));
    harness.drain();  // (the initialization file's transfer has finished lingering)
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto answer = harness.drain();
    REQUIRE(answer.size() == 1);
    CHECK(answer[0].kind == Command::Kind::send_from_control);
    const auto packet = Harness::decoded(answer[0]);
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::not_defined);
    CHECK(std::get<TftpError>(*packet).message == "ABORT:1005");

    // Any other request is stopped the same way.
    harness.to_control(write_request("HNPFMS_L.LCL"), Endpoint{target_tid.address, 4005});
    const auto other = harness.drain();
    REQUIRE(other.size() == 1);
    CHECK(std::get<TftpError>(*Harness::decoded(other[0])).message == "ABORT:1005");

    // The status file after that is accepted: it is the target's confirmation.
    harness.to_control(write_request("HNPFMS_L.LCS"), Endpoint{target_tid.address, 4006});
    const auto accepted = harness.drain();
    REQUIRE_FALSE(accepted.empty());
    CHECK(accepted.back().kind == Command::Kind::send_from_slot);
    harness.to_slot(accepted.back().slot,
                    TftpData{1, status_file(StatusCode::aborted_by_operator, 0)},
                    Endpoint{target_tid.address, 4006});
    REQUIRE(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::aborted_by_operator);
    CHECK(harness.loader->outcome().abort_confirmed);
}

TEST_CASE("a protocol version the loader does not know is aborted at the first status request",
          "[engine][loader][version]") {
    Harness harness;
    harness.accept(StatusCode::accepted, "B5");
    harness.drain();
    CHECK_FALSE(harness.loader->done());
    CHECK(harness.recorder.initialization.empty());
    harness.advance(seconds(2) + milliseconds(1));
    harness.drain();  // (the initialization file's transfer has finished lingering)
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const auto sent = harness.drain();
    REQUIRE(sent.size() == 1);
    CHECK(std::get<TftpError>(*Harness::decoded(sent[0])).message == "ABORT:1004");
    REQUIRE(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::unsupported_protocol_version);
}

TEST_CASE("a status file that is not a valid structure is a fatal error",
          "[engine][loader][status]") {
    Harness harness;
    harness.accept();
    harness.drain();
    harness.to_control(write_request("HNPFMS_L.LCS"));
    const std::size_t slot = harness.drain().back().slot;
    Bytes damaged = status_file(StatusCode::in_progress, 0);
    damaged.back() ^= std::byte{0x01};
    damaged.resize(damaged.size() - 1);  // the file length no longer agrees
    harness.to_slot(slot, TftpData{1, damaged});
    REQUIRE(harness.loader->done());
    CHECK(harness.loader->outcome().code == OutcomeCode::protocol_error);
}

TEST_CASE("status files out of order are not shown twice", "[engine][loader][status]") {
    Harness harness;
    harness.accept();
    harness.drain();
    const auto send_status = [&](std::uint16_t counter, std::uint16_t port) {
        const Endpoint from{target_tid.address, port};
        harness.to_control(write_request("HNPFMS_L.LCS"), from);
        const std::size_t slot = harness.drain().back().slot;
        harness.to_slot(slot, TftpData{1, status_file(StatusCode::in_progress, counter)}, from);
        harness.drain();
    };
    send_status(0, 4100);
    send_status(1, 4101);
    send_status(1, 4102);  // the same counter again: a repeat
    send_status(5, 4103);  // three lost
    send_status(3, 4104);  // older than the last: out of order
    send_status(6, 4105);
    CHECK(harness.recorder.status_counters == std::vector<std::uint16_t>{0, 1, 5, 6});
    const Outcome& outcome = harness.loader->outcome();
    CHECK(outcome.statuses_received == 4);
    CHECK(outcome.statuses_repeated == 2);
    CHECK(outcome.statuses_lost == 3);
    CHECK(harness.recorder.count(Notice::status_stale) == 1);
    CHECK(harness.recorder.count(Notice::status_duplicate) == 1);
    CHECK(harness.recorder.count(Notice::status_counter_gap) == 1);
}

TEST_CASE("the counter wraps from 65535 to 0", "[engine][loader][status]") {
    Harness harness;
    harness.accept();
    harness.drain();
    std::uint16_t port = 4200;
    const auto send_status = [&](std::uint16_t counter) {
        const Endpoint from{target_tid.address, port++};
        harness.to_control(write_request("HNPFMS_L.LCS"), from);
        const std::size_t slot = harness.drain().back().slot;
        harness.to_slot(slot, TftpData{1, status_file(StatusCode::in_progress, counter)}, from);
        harness.drain();
    };
    send_status(65534);
    send_status(65535);
    send_status(0);
    send_status(1);
    CHECK(harness.loader->outcome().statuses_received == 4);
    // Only the first is a gap (it says 65533 were lost before it); 65535 then 0 then 1 is not.
    CHECK(harness.loader->outcome().statuses_lost == 65533);
    CHECK(harness.loader->outcome().statuses_repeated == 0);
}

TEST_CASE("every datagram the loader sends is a packet the codec decodes", "[engine][loader]") {
    // The Harness checks this for each command; run a busy exchange through it.
    Harness harness;
    harness.accept();
    harness.advance(seconds(3));
    harness.to_control(write_request("HNPFMS_L.LCS"));
    harness.to_control(read_request("HNPFMS_L.LCL"), Endpoint{target_tid.address, 4300});
    harness.to_control(write_request("X"), Endpoint{target_tid.address, 4301});
    harness.advance(seconds(20));
    CHECK(harness.log.size() > 3);
}
