// The FIND packets of ARINC 615A-3 Attachment 3: an opcode, zero-terminated strings and a
// final 0x10, with the field limits of 3-6.

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/limits.hpp"
#include "bytes.hpp"
#include "codec_615a.hpp"
#include "sample_615a.hpp"

using namespace adl::arinc615a;
using adl::test::bytes_of;
using adl::test::from_hex;
using adl::test::to_bytes;

namespace {

const FindAnswer& answer_of(const FindPacket& packet) {
    REQUIRE(std::holds_alternative<FindAnswer>(packet));
    return std::get<FindAnswer>(packet);
}

std::vector<std::byte> answer_bytes(std::string_view a, std::string_view b, std::string_view c,
                                    std::string_view d, std::string_view e) {
    std::string text("\0\2", 2);
    for (const auto field : {a, b, c, d, e}) {
        text += field;
        text += '\0';
    }
    text += '\x10';
    return bytes_of(text);
}

}  // namespace

TEST_CASE("a FIND request is opcode 1, one NUL and 0x10", "[find]") {
    // Attachment 3, 3-7: "Opcode (IRQ) 0x0001, ASCII string terminator 0x00, Packet
    // Terminator 0x10".
    std::array<std::byte, 8> buffer{};
    const auto size = encode_find_request(buffer);

    REQUIRE(size.has_value());
    CHECK(*size == 4);
    CHECK(std::vector<std::byte>(buffer.begin(), buffer.begin() + 4) == from_hex("0001 00 10"));

    const auto decoded = decode_find(from_hex("0001 00 10"));
    REQUIRE(decoded.has_value());
    CHECK(std::holds_alternative<FindRequest>(*decoded));
}

TEST_CASE("a FIND answer is opcode 2, five strings and 0x10", "[find]") {
    // Attachment 3, 3-7: HNPFMS, FMS, L, "FMS LEFT" and the manufacturer code, each ended by
    // a NUL, in that order.
    const auto bytes = to_bytes(adl::test::sample::find_answer());

    CHECK(bytes ==
          from_hex("0002 484e50464d53 00 464d53 00 4c 00 464d53204c454654 00 484e50 00 10"));
    const auto decoded = decode_find(bytes);
    REQUIRE(decoded.has_value());
    CHECK(answer_of(*decoded) == adl::test::sample::find_answer());
}

TEST_CASE("the fields of an answer may be empty, each as a single NUL", "[find]") {
    const auto bytes = to_bytes(FindAnswer{});

    CHECK(bytes == from_hex("0002 00 00 00 00 00 10"));
    const auto decoded = decode_find(bytes);
    REQUIRE(decoded.has_value());
    CHECK(answer_of(*decoded) == FindAnswer{});
}

TEST_CASE("each field of an answer is allowed its limit and no more", "[find]") {
    // 15, 8, 8, 20 and 3 characters (3-6), written out here so that the test does not follow
    // the constants it checks.
    const std::array<std::size_t, 5> limits = {15, 8, 8, 20, 3};
    const std::array<std::string_view FindAnswer::*, 5> members = {
        &FindAnswer::target_hardware_id, &FindAnswer::target_type_name,
        &FindAnswer::target_position, &FindAnswer::literal_name, &FindAnswer::manufacturer_code};
    CHECK(find_max_answer_size == 62);

    std::array<std::string, 5> longest;
    for (std::size_t i = 0; i < 5; ++i) {
        longest[i] = std::string(limits[i], 'A');
    }
    FindAnswer all_longest;
    for (std::size_t i = 0; i < 5; ++i) {
        all_longest.*members[i] = longest[i];
    }
    const auto bytes = to_bytes(all_longest);
    CHECK(bytes.size() == 62);
    CHECK(decode_find(bytes).has_value());

    std::array<std::byte, 128> out{};
    for (std::size_t field = 0; field < 5; ++field) {
        CAPTURE(field);
        const std::string text(limits[field] + 1, 'X');

        FindAnswer too_long = all_longest;
        too_long.*members[field] = text;
        CHECK(encode_find_answer(too_long, out).error() == Error::field_too_large);
        CHECK(encoded_size(too_long).error() == Error::field_too_large);

        // And on the wire.
        std::array<std::string, 5> fields = longest;
        fields[field] = text;
        CHECK(decode_find(answer_bytes(fields[0], fields[1], fields[2], fields[3], fields[4]))
                  .error() == Error::bad_length);
    }
}

TEST_CASE("a manufacturer code is three characters, whatever the example in the standard shows",
          "[find]") {
    // Attachment 3's example prints the code as "7HNP", four characters, against "3
    // characters" in 3-6. The limit is followed; see docs/arinc615a-codec.md.
    CHECK(decode_find(answer_bytes("HNPFMS", "FMS", "L", "FMS LEFT", "7HNP")).error() ==
          Error::bad_length);
    CHECK(decode_find(answer_bytes("HNPFMS", "FMS", "L", "FMS LEFT", "HNP")).has_value());
}

TEST_CASE("a FIND request carries no data", "[find]") {
    CHECK(decode_find(from_hex("0001 61 00 10")).error() == Error::invalid_value);
    CHECK(decode_find(from_hex("0001 00 00 10")).error() == Error::invalid_value);
    CHECK(decode_find(from_hex("0001 10")).error() == Error::truncated);
}

TEST_CASE("a FIND packet ends with 0x10 and nothing follows it", "[find]") {
    CHECK(decode_find(from_hex("0001 00")).error() == Error::truncated);
    CHECK(decode_find(from_hex("0001 00 11")).error() == Error::invalid_value);
    CHECK(decode_find(from_hex("0001 00 10 00")).error() == Error::bad_length);
    CHECK(decode_find(from_hex("0001 00 10 10")).error() == Error::bad_length);

    auto answer = to_bytes(adl::test::sample::find_answer());
    answer.push_back(std::byte{0});
    CHECK(decode_find(answer).error() == Error::bad_length);
    answer.pop_back();
    answer.back() = std::byte{0x11};
    CHECK(decode_find(answer).error() == Error::invalid_value);
}

TEST_CASE("an answer with fewer than five strings is refused", "[find]") {
    CHECK_FALSE(decode_find(from_hex("0002 41 00 42 00 43 00 44 00 10")).has_value());
    CHECK_FALSE(decode_find(from_hex("0002 10")).has_value());
    CHECK_FALSE(decode_find(from_hex("0002 00 00 00 00 10")).has_value());
}

TEST_CASE("an answer with more than five strings is refused", "[find]") {
    CHECK_FALSE(decode_find(from_hex("0002 00 00 00 00 00 00 10")).has_value());
}

TEST_CASE("FIND strings carry no control characters", "[find]") {
    CHECK(decode_find(answer_bytes("A\nB", "", "", "", "")).error() == Error::malformed_text);
    CHECK(decode_find(answer_bytes("A\x10"
                                   "B",
                                   "", "", "", ""))
              .error() == Error::malformed_text);

    std::array<std::byte, 128> out{};
    CHECK(encode_find_answer(FindAnswer{"A\tB", "", "", "", ""}, out).error() ==
          Error::malformed_text);
    CHECK(
        encode_find_answer(FindAnswer{std::string_view{"A\0B", 3}, "", "", "", ""}, out).error() ==
        Error::malformed_text);
}

TEST_CASE("an unknown FIND opcode is refused", "[find]") {
    for (const char* hex : {"0000 00 10", "0003 00 10", "0100 00 10", "ffff 00 10"}) {
        CAPTURE(hex);
        CHECK(decode_find(from_hex(hex)).error() == Error::unknown_opcode);
    }
    CHECK(decode_find({}).error() == Error::truncated);
    CHECK(decode_find(from_hex("00")).error() == Error::truncated);
}

TEST_CASE("a FIND encoder reports a buffer that is too small and writes nothing past it",
          "[find]") {
    const auto good = to_bytes(adl::test::sample::find_answer());
    for (std::size_t room = 0; room < good.size(); ++room) {
        std::vector<std::byte> memory(room + 8, std::byte{0xAA});
        const auto result = encode_find_answer(adl::test::sample::find_answer(),
                                               std::span<std::byte>{memory}.first(room));
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == Error::buffer_too_small);
        for (std::size_t i = room; i < memory.size(); ++i) {
            REQUIRE(memory[i] == std::byte{0xAA});
        }
    }
    for (std::size_t room = 0; room < 4; ++room) {
        std::array<std::byte, 8> memory{};
        CHECK(encode_find_request(std::span<std::byte>{memory}.first(room)).error() ==
              Error::buffer_too_small);
    }
}

TEST_CASE("every shorter FIND datagram is refused", "[find]") {
    for (const auto& good : {to_bytes(adl::test::sample::find_answer()), from_hex("0001 00 10")}) {
        for (std::size_t length = 0; length < good.size(); ++length) {
            CAPTURE(length);
            CHECK_FALSE(decode_find(std::span<const std::byte>{good}.first(length)).has_value());
        }
    }
}
