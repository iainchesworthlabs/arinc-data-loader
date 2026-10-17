// The TFTP transfer state machine (adl/arinc615a/tftp_transfer.hpp) on its own: two of them
// across a link in memory, and one at a time with every datagram written out by hand.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <map>
#include <set>

#include "mini_tftp.hpp"
#include "transfer_pair.hpp"

using namespace adl::arinc615a;
using namespace adl::test;

namespace {

constexpr Endpoint peer{Address{{10, 0, 0, 2}}, 3000};
constexpr Endpoint server{Address{{10, 0, 0, 2}}, 59};

TransferSettings settings_for(std::size_t max_block) {
    TransferSettings settings;
    settings.max_block_size = max_block;
    return settings;
}

std::uint16_t block_of(std::span<const std::byte> datagram) {
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(datagram[2]) << 8) |
                                      std::to_integer<unsigned>(datagram[3]));
}

bool is_opcode(std::span<const std::byte> datagram, Opcode opcode) {
    return datagram.size() >= 2 && datagram[0] == std::byte{0} &&
           std::to_integer<unsigned>(datagram[1]) == static_cast<unsigned>(opcode);
}

// One transfer, fed by hand.
struct Direct {
    explicit Direct(std::size_t buffer = 2048) : tx(buffer), transfer(tx) {}

    void feed(const TftpPacket& packet, const Endpoint& from = peer) {
        const Bytes bytes = mini_encode(packet);
        transfer.on_datagram(now, from, bytes);
    }
    void feed_bytes(std::span<const std::byte> bytes, const Endpoint& from = peer) {
        transfer.on_datagram(now, from, bytes);
    }
    std::optional<Bytes> take() {
        const auto out = transfer.take_outgoing();
        if (!out) {
            return std::nullopt;
        }
        return Bytes(out->bytes.begin(), out->bytes.end());
    }
    std::optional<TftpPacket> take_packet() {
        // The views of the packet are into `taken`, which is kept.
        const auto bytes = take();
        if (!bytes) {
            return std::nullopt;
        }
        taken = *bytes;
        const auto packet = decode_packet(taken);
        return packet ? std::optional<TftpPacket>(*packet) : std::nullopt;
    }
    void advance(Duration by) {
        now = now + by;
        transfer.on_timer(now);
    }

    Bytes tx;
    TftpTransfer transfer;
    Instant now{};
    Bytes taken;
};

}  // namespace

// ---- Whole transfers across the link --------------------------------------------------------

TEST_CASE("a file is read whatever its size and block size", "[engine][tftp-transfer]") {
    for (const std::size_t size :
         {0U, 1U, 7U, 8U, 511U, 512U, 513U, 1023U, 1024U, 1025U, 5000U, 10240U}) {
        for (const std::size_t block : {0U, 8U, 100U, 512U, 1024U, 4000U}) {
            CAPTURE(size, block);
            TransferPair pair;
            MemorySource source(pattern(size, size + block));
            MemorySink sink;
            pair.server_mode = TransferPair::ServerMode::read;
            pair.server_source = &source;
            pair.server_settings = settings_for(1024);
            ClientOptions options;
            options.block_size = block;
            options.transfer_size = 0;
            REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN", options,
                                             sink, settings_for(4000)));
            REQUIRE(pair.run());
            CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
            CHECK(pair.server().result().outcome == TransferOutcome::succeeded);
            CHECK(sink.bytes == source.bytes);
            CHECK(pair.client().result().bytes == size);
            CHECK(pair.client().result().checksum == ChecksumVerdict::not_given);
            if (block != 0) {
                // The size comes back in the OACK, and the block size is the smaller of what
                // was asked and what the server allows.
                CHECK(pair.client().result().announced_size == size);
            }
        }
    }
}

TEST_CASE("a file is written whatever its size and block size", "[engine][tftp-transfer]") {
    for (const std::size_t size : {0U, 1U, 8U, 511U, 512U, 513U, 1024U, 5000U}) {
        for (const std::size_t block : {0U, 8U, 512U, 1024U}) {
            CAPTURE(size, block);
            TransferPair pair;
            MemorySource source(pattern(size, size * 7 + block));
            MemorySink sink;
            pair.server_mode = TransferPair::ServerMode::write;
            pair.server_sink = &sink;
            pair.server_settings = settings_for(1024);
            ClientOptions options;
            options.block_size = block;
            REQUIRE(pair.client().start_write(Instant{}, pair.server_control, "FILE.BIN", options,
                                              source, settings_for(1024)));
            REQUIRE(pair.run());
            CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
            CHECK(pair.server().result().outcome == TransferOutcome::succeeded);
            CHECK(sink.bytes == source.bytes);
        }
    }
}

TEST_CASE("the server takes the smaller block size", "[engine][tftp-transfer]") {
    TransferPair pair;
    MemorySource source(pattern(3000));
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::read;
    pair.server_source = &source;
    pair.server_settings = settings_for(600);  // below what is asked
    ClientOptions options;
    options.block_size = 1024;
    REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN", options, sink,
                                     settings_for(1024)));
    REQUIRE(pair.run());
    CHECK(sink.bytes == source.bytes);
    // 3000 bytes in blocks of 600: five full blocks and the empty one that ends the file.
    std::size_t data_packets = 0;
    for (const Seen& seen : pair.seen()) {
        if (!seen.from_client && is_opcode(seen.bytes, Opcode::data)) {
            ++data_packets;
        }
    }
    CHECK(data_packets == 6);
}

TEST_CASE("the largest block size works", "[engine][tftp-transfer]") {
    TransferPair pair(tftp_max_block_size + 4);
    MemorySource source(pattern(3 * tftp_max_block_size + 17));
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::read;
    pair.server_source = &source;
    pair.server_settings = settings_for(tftp_max_block_size);
    ClientOptions options;
    options.block_size = tftp_max_block_size;
    REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN", options, sink,
                                     settings_for(tftp_max_block_size)));
    REQUIRE(pair.run());
    CHECK(sink.bytes == source.bytes);
}

TEST_CASE("block numbers go 65535 and then 1, and never 0", "[engine][tftp-transfer][wrap]") {
    TransferPair pair;
    // 8 byte blocks: 65540 full ones and a short one is past the wrap by a few blocks.
    MemorySource source(pattern(8U * 65540U + 3U));
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::read;
    pair.server_source = &source;
    pair.server_settings = settings_for(1024);
    ClientOptions options;
    options.block_size = 8;
    REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "BIG.BIN", options, sink,
                                     settings_for(1024)));
    REQUIRE(pair.run());
    CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
    CHECK(sink.bytes == source.bytes);

    std::vector<std::uint16_t> data_blocks;
    std::vector<std::uint16_t> ack_blocks;
    for (const Seen& seen : pair.seen()) {
        if (!seen.from_client && is_opcode(seen.bytes, Opcode::data)) {
            data_blocks.push_back(block_of(seen.bytes));
        } else if (seen.from_client && is_opcode(seen.bytes, Opcode::ack)) {
            ack_blocks.push_back(block_of(seen.bytes));
        }
    }
    REQUIRE(data_blocks.size() == 65541);
    CHECK(data_blocks.front() == 1);
    CHECK(data_blocks[65534] == 65535);
    CHECK(data_blocks[65535] == 1);
    CHECK(data_blocks[65536] == 2);
    CHECK(std::ranges::count(data_blocks, 0) == 0);
    // The receiver acknowledged each one (block 0 once, for the OACK).
    CHECK(ack_blocks.front() == 0);
    CHECK(ack_blocks[65535] == 65535);
    CHECK(ack_blocks[65536] == 1);
}

TEST_CASE("a file is written across the wrap too", "[engine][tftp-transfer][wrap]") {
    TransferPair pair;
    MemorySource source(pattern(8U * 65540U + 8U));  // an exact multiple: an empty last block
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::write;
    pair.server_sink = &sink;
    pair.server_settings = settings_for(1024);
    ClientOptions options;
    options.block_size = 8;
    REQUIRE(pair.client().start_write(Instant{}, pair.server_control, "BIG.BIN", options, source,
                                      settings_for(1024)));
    REQUIRE(pair.run());
    CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
    CHECK(sink.bytes == source.bytes);
}

// ---- Faults on the link -----------------------------------------------------------------------

TEST_CASE("duplicated datagrams never make a sender resend a block",
          "[engine][tftp-transfer][sas]") {
    // Attachment 4, 4-3.3: every datagram arrives twice, so every ACK does. A sender that
    // answered a duplicate ACK with the block again would send each block twice or more.
    TransferPair pair;
    pair.profile().duplicate_per_million = 1'000'000;
    MemorySource source(pattern(20 * 512 + 100));
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::read;
    pair.server_source = &source;
    pair.server_settings = settings_for(1024);
    REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN", ClientOptions{},
                                     sink, settings_for(1024)));
    REQUIRE(pair.run());
    CHECK(sink.bytes == source.bytes);
    CHECK(sink.writes == 21);  // each block written once, though each arrived twice
    std::map<std::uint16_t, std::size_t> sent;
    for (const Seen& seen : pair.seen()) {
        if (!seen.from_client && is_opcode(seen.bytes, Opcode::data)) {
            ++sent[block_of(seen.bytes)];
        }
    }
    // 21 blocks, each sent once by the sender; the receiver's repeated ACKs (one per duplicate
    // block it saw) were not answered with more. Block 1 may go twice: the request arrived twice,
    // and a repeated request is answered with the first answer again.
    CHECK(sent.size() == 21);
    for (const auto& [block, count] : sent) {
        CAPTURE(block);
        CHECK(count == (block == 1 ? 2U : 1U));
    }
}

TEST_CASE("a duplicate ACK is ignored by the sender", "[engine][tftp-transfer][sas]") {
    Direct direct;
    MemorySource source(pattern(2000));
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", {}};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    const auto first = direct.take();
    REQUIRE(first);
    CHECK(is_opcode(*first, Opcode::data));
    CHECK(block_of(*first) == 1);

    direct.feed(TftpAck{1});
    const auto second = direct.take();
    REQUIRE(second);
    CHECK(block_of(*second) == 2);

    for (int repeat = 0; repeat < 5; ++repeat) {
        direct.feed(TftpAck{1});  // the duplicate of the ACK it has had
        CHECK_FALSE(direct.take());
    }
    direct.feed(TftpAck{0});
    CHECK_FALSE(direct.take());
    direct.feed(TftpAck{3});  // an ACK for a block never sent
    CHECK_FALSE(direct.take());
    CHECK(direct.transfer.result().outcome == TransferOutcome::running);
}

TEST_CASE("loss, duplication and reordering still deliver the file or fail cleanly",
          "[engine][tftp-transfer][faults]") {
    unsigned succeeded = 0;
    unsigned failed = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        CAPTURE(seed);
        for (const bool reading : {true, false}) {
            TransferPair pair(1024 + 4, seed);
            pair.profile().loss_per_million = 20'000;
            pair.profile().duplicate_per_million = 100'000;
            pair.profile().jitter = std::chrono::milliseconds(40);
            MemorySource source(pattern(60 * 512 + 11, seed));
            MemorySink sink;
            if (reading) {
                pair.server_mode = TransferPair::ServerMode::read;
                pair.server_source = &source;
                REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN",
                                                 ClientOptions{}, sink, settings_for(1024)));
            } else {
                pair.server_mode = TransferPair::ServerMode::write;
                pair.server_sink = &sink;
                REQUIRE(pair.client().start_write(Instant{}, pair.server_control, "FILE.BIN",
                                                  ClientOptions{}, source, settings_for(1024)));
            }
            pair.server_settings = settings_for(1024);
            REQUIRE(pair.run());
            const TransferOutcome outcome = pair.client().result().outcome;
            if (outcome == TransferOutcome::succeeded) {
                ++succeeded;
                // A received file is exactly the file, whatever the link did to the datagrams.
                CHECK(sink.bytes == source.bytes);
            } else {
                ++failed;
                // With one retry a transfer can run out of luck, and then it says so.
                CHECK(outcome == TransferOutcome::timed_out);
            }
        }
    }
    CHECK(succeeded > 90);  // the loss is mild: nearly all get through
    CHECK(succeeded + failed == 120);
}

TEST_CASE("reordered datagrams do not corrupt a transfer", "[engine][tftp-transfer][faults]") {
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        CAPTURE(seed);
        TransferPair pair(1024 + 4, seed);
        pair.profile().latency = std::chrono::milliseconds(1);
        pair.profile().jitter =
            std::chrono::milliseconds(900);  // under the time-out, so reordering
        MemorySource source(pattern(40 * 512 + 5, seed));
        MemorySink sink;
        pair.server_mode = TransferPair::ServerMode::read;
        pair.server_source = &source;
        pair.server_settings = settings_for(1024);
        REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN",
                                         ClientOptions{}, sink, settings_for(1024)));
        REQUIRE(pair.run());
        CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
        CHECK(sink.bytes == source.bytes);
    }
}

// ---- Time-outs ----------------------------------------------------------------------------------

TEST_CASE("a request nobody answers is sent twice and then fails after four seconds",
          "[engine][tftp-transfer][timeout]") {
    TransferPair pair;
    MemorySink sink;
    pair.server_mode = TransferPair::ServerMode::ignore;
    REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN", ClientOptions{},
                                     sink, settings_for(1024)));
    REQUIRE(pair.run());
    CHECK(pair.client().result().outcome == TransferOutcome::timed_out);
    REQUIRE(pair.seen().size() == 2);  // Attachment 4: one try and one retry
    CHECK(pair.seen()[0].at == Instant{});
    CHECK(pair.seen()[1].at.since_origin == std::chrono::seconds(2));
    CHECK(pair.now().since_origin == std::chrono::seconds(4));
    CHECK(pair.seen()[0].bytes == pair.seen()[1].bytes);
}

TEST_CASE("lost ACKs are covered by the retry, and enough of them fail the transfer",
          "[engine][tftp-transfer][timeout]") {
    // The server's ACK of block 3 is sent when the block arrives, again when the repeated block
    // arrives, and a third time when the server's own time-out runs out waiting for block 4: so
    // two lost ACKs are made up for by the third, and three are not.
    for (const unsigned drops : {1U, 2U, 3U}) {
        CAPTURE(drops);
        TransferPair pair;
        MemorySource source(pattern(5 * 512));
        MemorySink sink;
        pair.server_mode = TransferPair::ServerMode::write;
        pair.server_sink = &sink;
        pair.server_settings = settings_for(1024);
        unsigned dropped = 0;
        pair.drop = [&](bool from_client, std::span<const std::byte> bytes, std::size_t) {
            if (!from_client && is_opcode(bytes, Opcode::ack) && block_of(bytes) == 3 &&
                dropped < drops) {
                ++dropped;
                return true;
            }
            return false;
        };
        REQUIRE(pair.client().start_write(Instant{}, pair.server_control, "FILE.BIN",
                                          ClientOptions{}, source, settings_for(1024)));
        REQUIRE(pair.run());
        std::size_t block3 = 0;
        for (const Seen& seen : pair.seen()) {
            if (seen.from_client && is_opcode(seen.bytes, Opcode::data) &&
                block_of(seen.bytes) == 3) {
                ++block3;
            }
        }
        CHECK(block3 <= 2);  // the original and the one retry, never more
        if (drops < 3) {
            CHECK(pair.client().result().outcome == TransferOutcome::succeeded);
            CHECK(sink.bytes == source.bytes);
        } else {
            CHECK(pair.client().result().outcome == TransferOutcome::timed_out);
            CHECK(block3 == 2);
        }
    }
}

TEST_CASE("the timeout option the peer asks for sets our time-out",
          "[engine][tftp-transfer][timeout]") {
    Direct direct;
    MemorySink sink;
    OptionBuilder options;
    REQUIRE(options.add_timeout(5));
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    const auto oack = direct.take_packet();
    REQUIRE(oack);
    REQUIRE(std::holds_alternative<TftpOptionAck>(*oack));
    CHECK(find_option(std::get<TftpOptionAck>(*oack).options, "timeout") == "5");
    CHECK(direct.transfer.next_wake().since_origin == std::chrono::seconds(5));
}

// ---- WAIT, ABORT, errors
// ------------------------------------------------------------------------------

TEST_CASE("a WAIT answer to a request ends the transfer and says how long",
          "[engine][tftp-transfer][wait]") {
    for (const bool reading : {true, false}) {
        TransferPair pair;
        std::array<std::byte, 64> packet{};
        const auto size = encode_wait(WaitMessage{50}, packet);
        REQUIRE(size);
        pair.canned_reply.assign(packet.begin(),
                                 packet.begin() + static_cast<std::ptrdiff_t>(*size));
        MemorySink sink;
        MemorySource source(pattern(100));
        if (reading) {
            REQUIRE(pair.client().start_read(Instant{}, pair.server_control, "FILE.BIN",
                                             ClientOptions{}, sink, settings_for(1024)));
        } else {
            REQUIRE(pair.client().start_write(Instant{}, pair.server_control, "FILE.BIN",
                                              ClientOptions{}, source, settings_for(1024)));
        }
        REQUIRE(pair.run());
        CHECK(pair.client().result().outcome == TransferOutcome::wait_requested);
        CHECK(pair.client().result().wait_seconds == 50);
        CHECK(pair.client().peer_text() == "WAIT:50");
        CHECK_FALSE(pair.client().active());
        CHECK(pair.seen().size() == 2);  // the request and the answer: nothing was retried
    }
}

TEST_CASE("a WAIT in the middle of a transfer is an ordinary error",
          "[engine][tftp-transfer][wait]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    static_cast<void>(direct.take());
    std::array<std::byte, 64> packet{};
    const auto size = encode_wait(WaitMessage{5}, packet);
    REQUIRE(size);
    direct.feed_bytes(std::span<const std::byte>{packet}.first(*size));
    CHECK(direct.transfer.result().outcome == TransferOutcome::peer_error);
}

TEST_CASE("an ABORT from the peer ends the transfer and carries its status",
          "[engine][tftp-transfer][abort]") {
    for (const StatusCode status :
         {StatusCode::aborted_by_target, StatusCode::aborted_by_data_loader,
          StatusCode::aborted_by_operator}) {
        Direct direct;
        MemorySink sink;
        REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                           settings_for(1024)));
        static_cast<void>(direct.take());
        direct.feed(TftpData{1, Bytes(512, std::byte{1})});
        static_cast<void>(direct.take());
        std::array<std::byte, 64> packet{};
        const auto size = encode_abort(AbortMessage{status}, packet);
        REQUIRE(size);
        direct.feed_bytes(std::span<const std::byte>{packet}.first(*size));
        CHECK(direct.transfer.result().outcome == TransferOutcome::peer_abort);
        CHECK(direct.transfer.result().abort_status == status);
        CHECK_FALSE(direct.take());  // nothing is sent back to an abort
    }
}

TEST_CASE("our own abort sends the ABORT message, and only to a peer we know",
          "[engine][tftp-transfer][abort]") {
    Direct direct;
    MemorySource source(pattern(2000));
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", {}};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    static_cast<void>(direct.take());
    direct.transfer.abort(StatusCode::aborted_by_operator);
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::not_defined);
    CHECK(std::get<TftpError>(*packet).message == "ABORT:1005");
    CHECK(direct.transfer.result().outcome == TransferOutcome::aborted);
    CHECK_FALSE(direct.transfer.active());
    CHECK(direct.transfer.next_wake() == never);

    // A client that has had no answer yet does not know where to send it.
    Direct client;
    MemorySink sink;
    REQUIRE(client.transfer.start_read(client.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(client.take());
    client.transfer.abort(StatusCode::aborted_by_data_loader);
    CHECK_FALSE(client.take());
    CHECK(client.transfer.result().outcome == TransferOutcome::aborted);
}

TEST_CASE("an ordinary error packet ends the transfer", "[engine][tftp-transfer][errors]") {
    for (const bool first_reply : {true, false}) {
        Direct direct;
        MemorySink sink;
        REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                           settings_for(1024)));
        static_cast<void>(direct.take());
        if (!first_reply) {
            direct.feed(TftpData{1, Bytes(512, std::byte{1})});
            static_cast<void>(direct.take());
        }
        direct.feed(TftpError{TftpErrorCode::file_not_found, "no such file"});
        CHECK(direct.transfer.result().outcome == TransferOutcome::peer_error);
        CHECK(direct.transfer.result().error_code == TftpErrorCode::file_not_found);
        CHECK(direct.transfer.peer_text() == "no such file");
        CHECK_FALSE(direct.transfer.active());
        CHECK_FALSE(direct.take());
    }
}

// ---- Options ------------------------------------------------------------------------------------

TEST_CASE("our request carries the options we ask for", "[engine][tftp-transfer][options]") {
    Direct direct;
    MemorySink sink;
    ClientOptions options;
    options.block_size = 1024;
    options.transfer_size = 0;
    options.port = 1615;
    REQUIRE(direct.transfer.start_read(direct.now, server, "HNPFMS_L.LUI", options, sink,
                                       settings_for(4096)));
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    const auto* request = std::get_if<TftpRequest>(&*packet);
    REQUIRE(request != nullptr);
    CHECK(request->kind == TftpRequest::Kind::read);
    CHECK(request->file_name == "HNPFMS_L.LUI");
    const auto read = read_transfer_options(request->options);
    REQUIRE(read);
    CHECK(read->block_size == 1024);
    CHECK(read->transfer_size == 0);
    CHECK(read->port == 1615);
    CHECK_FALSE(read->timeout);
    CHECK_FALSE(read->checksum);

    // A write says how big the file is.
    Direct writer;
    MemorySource source(pattern(777));
    REQUIRE(writer.transfer.start_write(writer.now, server, "HNPFMS_L.LUR", ClientOptions{}, source,
                                        settings_for(1024)));
    const auto write_packet = writer.take_packet();
    REQUIRE(write_packet);
    const auto* write_request = std::get_if<TftpRequest>(&*write_packet);
    REQUIRE(write_request != nullptr);
    CHECK(write_request->kind == TftpRequest::Kind::write);
    const auto write_options = read_transfer_options(write_request->options);
    REQUIRE(write_options);
    CHECK(write_options->transfer_size == 777);
    CHECK_FALSE(write_options->block_size);
}

TEST_CASE("the block size we ask for is held to what we can take",
          "[engine][tftp-transfer][options]") {
    Direct direct(1024 + 4);
    MemorySink sink;
    ClientOptions options;
    options.block_size = 60000;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", options, sink,
                                       settings_for(2000)));  // the buffer holds 1024
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    const auto read = read_transfer_options(std::get<TftpRequest>(*packet).options);
    REQUIRE(read);
    CHECK(read->block_size == 1024);
}

TEST_CASE("an OACK that breaks the rules refuses the transfer with error 8",
          "[engine][tftp-transfer][options]") {
    const auto start = [](Direct& direct, MemorySink& sink) {
        ClientOptions options;
        options.block_size = 512;
        REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", options, sink,
                                           settings_for(1024)));
        static_cast<void>(direct.take());
    };
    SECTION("a larger block size than was asked") {
        Direct direct;
        MemorySink sink;
        start(direct, sink);
        OptionBuilder oack;
        REQUIRE(oack.add_block_size(1024));
        direct.feed(TftpOptionAck{oack.list()});
        CHECK(direct.transfer.result().outcome == TransferOutcome::option_refused);
        const auto reply = direct.take_packet();
        REQUIRE(reply);
        REQUIRE(std::holds_alternative<TftpError>(*reply));
        CHECK(std::get<TftpError>(*reply).code == TftpErrorCode::option_refused);
    }
    SECTION("a block size that was not asked for") {
        Direct direct;
        MemorySink sink;
        REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                           settings_for(1024)));
        static_cast<void>(direct.take());
        OptionBuilder oack;
        REQUIRE(oack.add_block_size(512));
        direct.feed(TftpOptionAck{oack.list()});
        CHECK(direct.transfer.result().outcome == TransferOutcome::option_refused);
    }
    SECTION("a timeout that was not asked for") {
        Direct direct;
        MemorySink sink;
        start(direct, sink);
        OptionBuilder oack;
        REQUIRE(oack.add_timeout(9));
        direct.feed(TftpOptionAck{oack.list()});
        CHECK(direct.transfer.result().outcome == TransferOutcome::option_refused);
    }
    SECTION("a value that is not a number") {
        Direct direct;
        MemorySink sink;
        start(direct, sink);
        OptionBuilder oack;
        REQUIRE(oack.add("blksize", "many"));
        direct.feed(TftpOptionAck{oack.list()});
        CHECK(direct.transfer.result().outcome == TransferOutcome::option_refused);
    }
}

TEST_CASE("an option we do not know is left out of the answer",
          "[engine][tftp-transfer][options]") {
    Direct direct;
    MemorySink sink;
    OptionBuilder options;
    REQUIRE(options.add("zeroize", "1"));
    REQUIRE(options.add_block_size(700));
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    const auto* oack = std::get_if<TftpOptionAck>(&*packet);
    REQUIRE(oack != nullptr);
    CHECK(find_option(oack->options, "blksize") == "700");
    CHECK_FALSE(find_option(oack->options, "zeroize"));
    CHECK(oack->options.size() == 1);
}

TEST_CASE("a request with malformed options is refused with error 8",
          "[engine][tftp-transfer][options]") {
    Direct direct;
    MemorySink sink;
    OptionBuilder options;
    REQUIRE(options.add("blksize", "7"));  // below the least RFC 2348 allows
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    CHECK(direct.transfer.result().outcome == TransferOutcome::option_refused);
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::option_refused);
}

TEST_CASE("a write request with no options is answered with ACK 0",
          "[engine][tftp-transfer][options]") {
    Direct direct;
    MemorySink sink;
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", {}};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpAck>(*packet));
    CHECK(std::get<TftpAck>(*packet).block == 0);
}

TEST_CASE("a read request's size is answered with the file's size",
          "[engine][tftp-transfer][options]") {
    Direct direct;
    MemorySource source(pattern(1234));
    OptionBuilder options;
    REQUIRE(options.add_transfer_size(0));
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", options.list()};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    const auto* oack = std::get_if<TftpOptionAck>(&*packet);
    REQUIRE(oack != nullptr);
    CHECK(find_option(oack->options, "tsize") == "1234");
    // The data starts after the target acknowledges the OACK with block 0.
    CHECK_FALSE(direct.take());
    direct.feed(TftpAck{0});
    const auto data = direct.take();
    REQUIRE(data);
    CHECK(block_of(*data) == 1);
}

// ---- The size and the checksum of what arrives
// ----------------------------------------------------

TEST_CASE("a transfer size that is not what arrives fails the transfer",
          "[engine][tftp-transfer][tsize]") {
    for (const std::uint64_t announced : {std::uint64_t{600}, std::uint64_t{1400}}) {
        Direct direct;
        MemorySink sink;
        OptionBuilder options;
        REQUIRE(options.add_transfer_size(announced));
        const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
        direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
        static_cast<void>(direct.take());
        direct.feed(TftpData{1, Bytes(512, std::byte{1})});
        static_cast<void>(direct.take());
        direct.feed(TftpData{2, Bytes(100, std::byte{2})});  // 612 bytes in all
        CHECK(direct.transfer.result().outcome == TransferOutcome::size_mismatch);
        const auto packet = direct.take_packet();
        REQUIRE(packet);
        CHECK(std::holds_alternative<TftpError>(*packet));
    }
    // A matching size is no trouble.
    Direct direct;
    MemorySink sink;
    OptionBuilder options;
    REQUIRE(options.add_transfer_size(612));
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    direct.feed(TftpData{2, Bytes(100, std::byte{2})});
    CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
}

namespace {

ChecksumOption crc16_of(std::span<const std::byte> data, bool damage) {
    const auto value =
        adl::arinc665::compute_check_value(adl::arinc665::CheckValueType::crc16, data);
    ChecksumOption option;
    option.type = adl::arinc665::CheckValueType::crc16;
    option.length = static_cast<std::uint8_t>(value->bytes.size());
    std::copy(value->bytes.begin(), value->bytes.end(), option.value.begin());
    if (damage) {
        option.value[1] ^= std::byte{0x40};
    }
    return option;
}

}  // namespace

TEST_CASE("a checksum is verified and never fails the transfer",
          "[engine][tftp-transfer][checksum]") {
    for (const bool damage : {false, true}) {
        CAPTURE(damage);
        const Bytes content = pattern(1500, 9);
        Direct direct;
        MemorySink sink;
        OptionBuilder options;
        REQUIRE(options.add_block_size(1024));
        REQUIRE(options.add_checksum(crc16_of(content, damage)));
        const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
        direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
        const auto oack = direct.take_packet();
        REQUIRE(oack);
        const auto* ack = std::get_if<TftpOptionAck>(&*oack);
        REQUIRE(ack != nullptr);
        CHECK(find_option(ack->options, "checksum_2").has_value());  // acknowledged
        direct.feed(TftpData{1, std::span<const std::byte>{content}.first(1024)});
        static_cast<void>(direct.take());
        direct.feed(TftpData{2, std::span<const std::byte>{content}.subspan(1024)});
        // 5.3.2.3.8.5: whatever the checksum says, the file is complete and kept.
        CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
        CHECK(sink.bytes == content);
        CHECK(direct.transfer.result().checksum ==
              (damage ? ChecksumVerdict::mismatched : ChecksumVerdict::matched));
    }
}

TEST_CASE("every algorithm the library can calculate is checked",
          "[engine][tftp-transfer][checksum]") {
    using adl::arinc665::CheckValueType;
    const Bytes content = pattern(333, 4);
    for (const CheckValueType type :
         {CheckValueType::crc8, CheckValueType::crc16, CheckValueType::crc32, CheckValueType::md5,
          CheckValueType::sha1}) {
        CAPTURE(static_cast<int>(type));
        const auto value = adl::arinc665::compute_check_value(type, content);
        REQUIRE(value);
        ChecksumOption option;
        option.type = type;
        option.length = static_cast<std::uint8_t>(value->bytes.size());
        std::copy(value->bytes.begin(), value->bytes.end(), option.value.begin());
        Direct direct;
        MemorySink sink;
        OptionBuilder options;
        REQUIRE(options.add_checksum(option));
        const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", options.list()};
        direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
        static_cast<void>(direct.take());
        direct.feed(TftpData{1, content});
        CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
        CHECK(direct.transfer.result().checksum == ChecksumVerdict::matched);
    }
}

TEST_CASE("a file with no checksum option has no verdict", "[engine][tftp-transfer][checksum]") {
    Direct direct;
    MemorySink sink;
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", {}};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(10, std::byte{1})});
    CHECK(direct.transfer.result().checksum == ChecksumVerdict::not_given);
}

TEST_CASE("a read request's 16-bit CRC is acknowledged and nothing else is",
          "[engine][tftp-transfer][checksum]") {
    for (const auto type :
         {adl::arinc665::CheckValueType::crc16, adl::arinc665::CheckValueType::crc32}) {
        Direct direct;
        MemorySource source(pattern(100));
        ChecksumOption option;
        option.type = type;
        option.length = type == adl::arinc665::CheckValueType::crc16 ? 2 : 4;
        OptionBuilder options;
        REQUIRE(options.add_checksum(option));
        REQUIRE(options.add_transfer_size(0));
        const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", options.list()};
        direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
        const auto packet = direct.take_packet();
        REQUIRE(packet);
        const auto* oack = std::get_if<TftpOptionAck>(&*packet);
        REQUIRE(oack != nullptr);
        CHECK(find_option(oack->options, type == adl::arinc665::CheckValueType::crc16
                                             ? "checksum_2"
                                             : "checksum_3")
                  .has_value() == (type == adl::arinc665::CheckValueType::crc16));
    }
}

// ---- Receiving: duplicates, strays, lingering
// -----------------------------------------------------

TEST_CASE("a block that arrives again is acknowledged again and written once",
          "[engine][tftp-transfer][receive]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    const auto first = direct.take();
    REQUIRE(first);
    CHECK(block_of(*first) == 1);
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    const auto again = direct.take();
    REQUIRE(again);
    CHECK(is_opcode(*again, Opcode::ack));
    CHECK(block_of(*again) == 1);
    CHECK(sink.writes == 1);
    // A block from the future, or from a long time ago, is not acknowledged.
    direct.feed(TftpData{5, Bytes(512, std::byte{5})});
    CHECK_FALSE(direct.take());
    direct.feed(TftpData{0, Bytes(512, std::byte{0})});
    CHECK_FALSE(direct.take());
    CHECK(sink.writes == 1);
}

TEST_CASE("datagrams from anyone else, or that are garbage, are ignored",
          "[engine][tftp-transfer][receive]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});  // from `peer`: this is the server's TID
    static_cast<void>(direct.take());

    // Another port of the same host: the wrong transfer ID.
    direct.feed(TftpData{2, Bytes(512, std::byte{2})}, Endpoint{peer.address, 3001});
    CHECK_FALSE(direct.take());
    // Another host.
    direct.feed(TftpData{2, Bytes(512, std::byte{2})}, Endpoint{Address{{10, 0, 0, 9}}, 3000});
    CHECK_FALSE(direct.take());
    // Garbage from the right place.
    const std::array<std::byte, 3> garbage{std::byte{0}, std::byte{3}, std::byte{9}};
    direct.feed_bytes(garbage);
    CHECK_FALSE(direct.take());
    direct.feed_bytes({});
    CHECK_FALSE(direct.take());
    CHECK(direct.transfer.result().outcome == TransferOutcome::running);
    CHECK(sink.writes == 1);
}

TEST_CASE("a block longer than the block size is an illegal operation",
          "[engine][tftp-transfer][receive]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(513, std::byte{1})});  // the block size is 512
    CHECK(direct.transfer.result().outcome == TransferOutcome::protocol_error);
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::illegal_operation);
    CHECK(sink.writes == 0);
}

TEST_CASE("the last block is acknowledged again for as long as the dally",
          "[engine][tftp-transfer][receive]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(100, std::byte{1})});  // short: the whole file
    const auto ack = direct.take();
    REQUIRE(ack);
    CHECK(block_of(*ack) == 1);
    CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
    CHECK(direct.transfer.finished());
    CHECK(direct.transfer.lingering());
    CHECK(direct.transfer.active());
    CHECK(direct.transfer.next_wake().since_origin == tftp_dally);

    // Its ACK was lost: the sender repeats the block, and is answered, without another write.
    direct.advance(std::chrono::milliseconds(500));
    direct.feed(TftpData{1, Bytes(100, std::byte{1})});
    const auto again = direct.take();
    REQUIRE(again);
    CHECK(is_opcode(*again, Opcode::ack));
    CHECK(sink.writes == 1);

    direct.advance(tftp_dally);
    CHECK_FALSE(direct.transfer.active());
    CHECK(direct.transfer.next_wake() == never);
    direct.feed(TftpData{1, Bytes(100, std::byte{1})});
    CHECK_FALSE(direct.take());
}

TEST_CASE("a receiver that waits for a block repeats its ACK once",
          "[engine][tftp-transfer][timeout]") {
    Direct direct;
    MemorySink sink;
    REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                       settings_for(1024)));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    const auto ack = direct.take();
    REQUIRE(ack);
    direct.advance(tftp_timeout);
    const auto repeat = direct.take();
    REQUIRE(repeat);
    CHECK(*repeat == *ack);  // the same ACK again
    direct.advance(tftp_timeout);
    CHECK(direct.transfer.result().outcome == TransferOutcome::timed_out);
    CHECK_FALSE(direct.take());
}

TEST_CASE("a repeated request is answered with the first answer again",
          "[engine][tftp-transfer][receive]") {
    Direct direct;
    MemorySink sink;
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", {}};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    const auto first = direct.take();
    REQUIRE(first);
    direct.transfer.on_repeated_request(direct.now);
    const auto again = direct.take();
    REQUIRE(again);
    CHECK(*again == *first);
    // Once the data has started it is not a repeat of anything that matters.
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    static_cast<void>(direct.take());
    direct.transfer.on_repeated_request(direct.now);
    CHECK_FALSE(direct.take());
}

// ---- Sending: what the data looks like
// ------------------------------------------------------------

TEST_CASE("the blocks sent are what the codec writes", "[engine][tftp-transfer][send]") {
    Direct direct;
    const Bytes content = pattern(1030, 3);
    MemorySource source(content);
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", {}};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    const auto first = direct.take();
    REQUIRE(first);
    Bytes expected(1024);
    const auto size =
        encode_data(TftpData{1, std::span<const std::byte>{content}.first(512)}, expected);
    REQUIRE(size);
    expected.resize(*size);
    CHECK(*first == expected);
    direct.feed(TftpAck{1});
    const auto second = direct.take();
    REQUIRE(second);
    const auto size2 =
        encode_data(TftpData{2, std::span<const std::byte>{content}.subspan(512, 512)}, expected);
    REQUIRE(size2);
    expected.resize(*size2);
    CHECK(*second == expected);
    direct.feed(TftpAck{2});
    const auto third = direct.take();
    REQUIRE(third);
    CHECK(third->size() == 4 + 6);  // the last six bytes
    direct.feed(TftpAck{3});
    CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
    CHECK(direct.transfer.result().bytes == 1030);
    CHECK_FALSE(direct.transfer.active());
}

TEST_CASE("a file of a whole number of blocks ends with an empty one",
          "[engine][tftp-transfer][send]") {
    Direct direct;
    MemorySource source(pattern(1024));
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", {}};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    static_cast<void>(direct.take());
    direct.feed(TftpAck{1});
    static_cast<void>(direct.take());
    direct.feed(TftpAck{2});
    const auto last = direct.take();
    REQUIRE(last);
    CHECK(last->size() == 4);
    CHECK(block_of(*last) == 3);
    direct.feed(TftpAck{3});
    CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
}

// ---- The files the transfer touches fail
// --------------------------------------------------------------

TEST_CASE("a full disk fails the transfer with error 3", "[engine][tftp-transfer][io]") {
    Direct direct;
    MemorySink sink;
    sink.fail_after_bytes = 700;
    const TftpRequest request{TftpRequest::Kind::write, "FILE.BIN", {}};
    direct.transfer.accept_write(direct.now, peer, request, sink, settings_for(1024));
    static_cast<void>(direct.take());
    direct.feed(TftpData{1, Bytes(512, std::byte{1})});
    static_cast<void>(direct.take());
    direct.feed(TftpData{2, Bytes(512, std::byte{1})});
    CHECK(direct.transfer.result().outcome == TransferOutcome::local_error);
    CHECK(direct.transfer.result().io_error == IoError::no_space);
    const auto packet = direct.take_packet();
    REQUIRE(packet);
    REQUIRE(std::holds_alternative<TftpError>(*packet));
    CHECK(std::get<TftpError>(*packet).code == TftpErrorCode::disk_full);
}

TEST_CASE("a file that cannot be read fails the transfer", "[engine][tftp-transfer][io]") {
    Direct direct;
    MemorySource source(pattern(2000));
    const TftpRequest request{TftpRequest::Kind::read, "FILE.BIN", {}};
    direct.transfer.accept_read(direct.now, peer, request, source, settings_for(1024));
    static_cast<void>(direct.take());
    source.fail_reads = true;
    direct.feed(TftpAck{1});
    CHECK(direct.transfer.result().outcome == TransferOutcome::local_error);
    CHECK(direct.take());  // an error packet for the peer
}

TEST_CASE("a request that does not fit the buffer is reported", "[engine][tftp-transfer][io]") {
    std::vector<std::byte> tiny(16);
    TftpTransfer transfer(tiny);
    MemorySink sink;
    const auto started = transfer.start_read(Instant{}, server, "A_LONG_FILE_NAME.BIN",
                                             ClientOptions{}, sink, settings_for(1024));
    CHECK_FALSE(started);
    CHECK(started.error() == Error::buffer_too_small);
    CHECK_FALSE(transfer.active());
}

TEST_CASE("a finished transfer can be used again", "[engine][tftp-transfer]") {
    Direct direct;
    MemorySink sink;
    for (int round = 0; round < 3; ++round) {
        REQUIRE(direct.transfer.start_read(direct.now, server, "FILE.BIN", ClientOptions{}, sink,
                                           settings_for(1024)));
        static_cast<void>(direct.take());
        direct.feed(TftpData{1, Bytes(4, std::byte{1})});
        CHECK(direct.transfer.result().outcome == TransferOutcome::succeeded);
        direct.transfer.reset();
        CHECK(direct.transfer.result().outcome == TransferOutcome::running);
        CHECK_FALSE(direct.transfer.active());
    }
    CHECK(sink.writes == 3);
}
