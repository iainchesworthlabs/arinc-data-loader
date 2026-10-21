// Packets and files come from the network, and a loader must meet a damaged or hostile
// one without crashing, hanging or reading past its end. These run every 615A decoder
// over every truncation and every single-byte and single-bit change of a good one, and
// over random bytes, under the sanitizers the Debug build turns on. Whatever comes back,
// an error or a value, the program has to be well-behaved, and a value has to be one the
// encoder writes back to bytes that decode to the same thing.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>

#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "bytes.hpp"
#include "codec_615a.hpp"
#include "edit.hpp"
#include "sample_615a.hpp"

using namespace adl::arinc615a;
using adl::test::from_hex;
using adl::test::to_bytes;
namespace sample = adl::test::sample;

namespace {

// What a decoder made of some bytes: whether it accepted them, and the bytes its encoder
// writes for what it accepted (nothing, if the encoder will not write it back).
struct Probe {
    bool accepted = false;
    std::optional<std::vector<std::byte>> reencoded;
};

using Prober = std::function<Probe(std::span<const std::byte>)>;

template <typename T, typename Decode, typename Size, typename Encode>
Prober prober(Decode decode, Size size, Encode encode) {
    return [=](std::span<const std::byte> bytes) {
        Probe probe;
        const auto value = decode(bytes);
        if (!value) {
            return probe;
        }
        probe.accepted = true;
        const auto needed = size(*value);
        if (!needed) {
            return probe;
        }
        std::vector<std::byte> out(*needed);
        if (const auto written = encode(*value, std::span<std::byte>{out});
            written && *written == out.size()) {
            probe.reencoded = std::move(out);
        }
        return probe;
    };
}

template <typename T>
Prober file_prober(Result<T> (*decode)(std::span<const std::byte>) noexcept,
                   Result<std::size_t> (*encode)(const T&, std::span<std::byte>) noexcept) {
    return prober<T>(
        decode, [](const T& file) { return encoded_size(file); },
        [encode](const T& file, std::span<std::byte> out) { return encode(file, out); });
}

struct Subject {
    const char* name;
    std::vector<std::byte> good;
    Prober probe;
    bool prefixes_fail;  // no proper prefix is a whole packet or file
};

std::vector<Subject> subjects() {
    const auto packet_prober = prober<TftpPacket>(
        [](std::span<const std::byte> b) { return decode_packet(b); },
        [](const TftpPacket& p) { return encoded_size(p); },
        [](const TftpPacket& p, std::span<std::byte> out) { return encode_packet(p, out); });
    const auto find_prober = prober<FindPacket>(
        [](std::span<const std::byte> b) { return decode_find(b); },
        [](const FindPacket& p) {
            return std::holds_alternative<FindAnswer>(p) ? encoded_size(std::get<FindAnswer>(p))
                                                         : Result<std::size_t>{4};
        },
        [](const FindPacket& p, std::span<std::byte> out) {
            return std::holds_alternative<FindAnswer>(p)
                       ? encode_find_answer(std::get<FindAnswer>(p), out)
                       : encode_find_request(out);
        });
    return {
        // Prefixes of these at an option boundary, or after any data, are packets too.
        {"RRQ", to_bytes(sample::rrq_lui()), packet_prober, false},
        {"WRQ", to_bytes(sample::wrq_lus()), packet_prober, false},
        {"OACK", to_bytes(sample::oack()), packet_prober, false},
        {"DATA", to_bytes(TftpData{1, sample::payload32()}), packet_prober, false},
        {"ACK", to_bytes(TftpAck{5}), packet_prober, true},
        {"ERROR", to_bytes(TftpError{TftpErrorCode::not_defined, "ABORT:1004"}), packet_prober,
         true},
        {"FIND request", from_hex("0001 00 10"), find_prober, true},
        {"FIND answer", to_bytes(sample::find_answer()), find_prober, true},
        {".LCI", to_bytes(sample::initialization_denied()),
         file_prober<InitializationFile>(decode_initialization_file, encode_initialization_file),
         true},
        {".LCL", to_bytes(sample::configuration_list()),
         file_prober<ConfigurationList>(decode_configuration_list, encode_configuration_list),
         true},
        {".LCS", to_bytes(sample::configuration_status()),
         file_prober<ConfigurationStatus>(decode_configuration_status, encode_configuration_status),
         true},
        {".LUR", to_bytes(sample::upload_request()),
         file_prober<UploadRequest>(decode_upload_request, encode_upload_request), true},
        {".LUS", to_bytes(sample::upload_status()),
         file_prober<UploadStatus>(decode_upload_status, encode_upload_status), true},
        {".LNR", to_bytes(sample::download_request()),
         file_prober<DownloadRequest>(decode_download_request, encode_download_request), true},
        {".LNS", to_bytes(sample::download_status()),
         file_prober<DownloadStatus>(decode_download_status, encode_download_status), true},
        {".LNL", to_bytes(sample::download_list()),
         file_prober<DownloadList>(decode_download_list, encode_download_list), true},
        {".LNA", to_bytes(sample::download_answer()),
         file_prober<DownloadAnswer>(decode_download_answer, encode_download_answer), true},
    };
}

// A value that was accepted must survive being written and read again.
void check_stable(const Subject& subject, std::span<const std::byte> bytes, std::size_t& accepted,
                  std::size_t& refused) {
    const Probe first = subject.probe(bytes);
    if (!first.accepted) {
        ++refused;
        return;
    }
    ++accepted;
    if (!first.reencoded) {
        return;
    }
    const Probe second = subject.probe(*first.reencoded);
    REQUIRE(second.accepted);
    REQUIRE(second.reencoded.has_value());
    REQUIRE(*second.reencoded == *first.reencoded);
}

}  // namespace

TEST_CASE("every good packet and file is accepted", "[robustness615a]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        CHECK(subject.probe(subject.good).accepted);
    }
}

TEST_CASE("every decoder refuses every truncation that cannot be whole", "[robustness615a]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (std::size_t length = 0; length < subject.good.size(); ++length) {
            CAPTURE(length);
            const auto prefix = std::span<const std::byte>{subject.good}.first(length);
            if (subject.prefixes_fail) {
                CHECK_FALSE(subject.probe(prefix).accepted);
            } else {
                check_stable(subject, prefix, accepted, refused);
            }
        }
    }
}

TEST_CASE("the datagrams that start a transfer are not whole before their mode",
          "[robustness615a]") {
    for (const auto& good : {to_bytes(sample::rrq_lui()), to_bytes(sample::wrq_lus())}) {
        // 2 (opcode) + name + NUL + "octet" + NUL: a request of no options ends here.
        const std::size_t shortest = 2 + 13 + 6;
        for (std::size_t length = 0; length < shortest; ++length) {
            CHECK_FALSE(decode_packet(std::span<const std::byte>{good}.first(length)).has_value());
        }
        CHECK(decode_packet(std::span<const std::byte>{good}.first(shortest)).has_value());
    }
    // An OACK is whole only with at least one option.
    const auto oack = to_bytes(sample::oack());
    for (std::size_t length = 0; length < 2 + 8 + 5; ++length) {
        CHECK_FALSE(decode_packet(std::span<const std::byte>{oack}.first(length)).has_value());
    }
}

TEST_CASE("a DATA packet is whole from its four-byte header", "[robustness615a]") {
    const auto good = to_bytes(TftpData{1, sample::payload32()});
    for (std::size_t length = 0; length < 4; ++length) {
        CHECK_FALSE(decode_packet(std::span<const std::byte>{good}.first(length)).has_value());
    }
    for (std::size_t length = 4; length <= good.size(); ++length) {
        const auto decoded = decode_packet(std::span<const std::byte>{good}.first(length));
        REQUIRE(decoded.has_value());
        CHECK(std::get<TftpData>(*decoded).data.size() == length - 4);
    }
}

TEST_CASE("every decoder survives every single-bit change of a good one", "[robustness615a]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (std::size_t byte = 0; byte < subject.good.size(); ++byte) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                auto damaged = subject.good;
                damaged[byte] ^= static_cast<std::byte>(1U << bit);
                check_stable(subject, damaged, accepted, refused);
            }
        }
        CHECK(accepted + refused == subject.good.size() * 8);
        // Not every change can matter, and not every change can be missed.
        CHECK(refused > 0);
    }
}

TEST_CASE("every decoder survives every byte of a good one set to the edge values",
          "[robustness615a]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (std::size_t byte = 0; byte < subject.good.size(); ++byte) {
            for (const unsigned value : {0x00U, 0x01U, 0x0AU, 0x10U, 0x20U, 0x7FU, 0x80U, 0xFFU}) {
                auto damaged = subject.good;
                damaged[byte] = static_cast<std::byte>(value);
                check_stable(subject, damaged, accepted, refused);
            }
        }
        CHECK(refused > 0);
    }
}

TEST_CASE("every decoder survives a good one with several bytes changed", "[robustness615a]") {
    std::mt19937 random{615};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (int round = 0; round < 3000; ++round) {
            auto damaged = subject.good;
            const unsigned changes = 1 + random() % 4;
            for (unsigned i = 0; i < changes; ++i) {
                damaged[random() % damaged.size()] = static_cast<std::byte>(random() & 0xFFU);
            }
            check_stable(subject, damaged, accepted, refused);
        }
        CHECK(refused > 0);
    }
}

TEST_CASE("every decoder survives a good one with bytes inserted or removed", "[robustness615a]") {
    std::mt19937 random{616};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (int round = 0; round < 1500; ++round) {
            auto damaged = subject.good;
            const std::size_t at = random() % damaged.size();
            if (random() % 2 == 0) {
                damaged.insert(damaged.begin() + static_cast<std::ptrdiff_t>(at),
                               static_cast<std::byte>(random() & 0xFFU));
            } else {
                damaged.erase(damaged.begin() + static_cast<std::ptrdiff_t>(at));
            }
            check_stable(subject, damaged, accepted, refused);
        }
        CHECK(refused > 0);
    }
}

TEST_CASE("every decoder survives random bytes", "[robustness615a]") {
    std::mt19937 random{617};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (int round = 0; round < 1000; ++round) {
            std::vector<std::byte> noise(random() % 600);
            for (std::byte& b : noise) {
                b = static_cast<std::byte>(random() & 0xFFU);
            }
            check_stable(subject, noise, accepted, refused);
        }
        CHECK(refused > 0);
    }
}

TEST_CASE("every decoder survives random bytes behind a valid beginning", "[robustness615a]") {
    // Random bytes almost never get past the opcode or the version; this puts the front of
    // a good one first so that the fields after it are what is tried.
    std::mt19937 random{618};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (int round = 0; round < 2000; ++round) {
            const std::size_t keep = subject.good.size() < 8 ? 2 : 6 + random() % 3;
            std::vector<std::byte> noise(
                subject.good.begin(), subject.good.begin() + static_cast<std::ptrdiff_t>(std::min(
                                                                 keep, subject.good.size())));
            const std::size_t more = random() % 80;
            for (std::size_t i = 0; i < more; ++i) {
                // Mostly small values, which are what lengths and counts are.
                noise.push_back(
                    static_cast<std::byte>(random() % 4 == 0 ? random() & 0xFFU : random() % 12));
            }
            // A protocol file's length field is put right so that the fields get read.
            if (subject.name[0] == '.' && noise.size() >= 4) {
                adl::test::put_u32(noise, 0, static_cast<std::uint32_t>(noise.size()));
            }
            check_stable(subject, noise, accepted, refused);
        }
        // A DATA packet is any four bytes and then anything, so little of this is refused.
        if (std::string_view{subject.name} != "DATA") {
            CHECK(refused > 0);
        }
    }
}

TEST_CASE("a hostile count or length cannot make a decoder read or do much", "[robustness615a]") {
    // 65535 entries claimed in a file of a few bytes.
    for (const char* hex :
         {"0000000a 4134 ffff 00000000", "0000001a 4134 0001 00 0000 0000 0000 202030 ffff"}) {
        auto bytes = from_hex(hex);
        adl::test::put_u32(bytes, 0, static_cast<std::uint32_t>(bytes.size()));
        CHECK_FALSE(decode_upload_request(bytes).has_value());
        CHECK_FALSE(decode_upload_status(bytes).has_value());
        CHECK_FALSE(decode_download_status(bytes).has_value());
        CHECK_FALSE(decode_download_request(bytes).has_value());
        CHECK_FALSE(decode_download_list(bytes).has_value());
        CHECK_FALSE(decode_download_answer(bytes).has_value());
        CHECK_FALSE(decode_configuration_list(bytes).has_value());
    }
}
