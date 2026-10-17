// Files come from outside, from a medium or a network, and a loader must meet a
// damaged or hostile one without crashing, hanging or reading past its end. These
// run every decoder over every truncation and every single-byte change of a good
// file, under the sanitizers the Debug build turns on: whatever comes back, an
// error or a value, the program has to be well-behaved.

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <random>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/header_file.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/arinc665/media_set.hpp"
#include "edit.hpp"
#include "fixtures.hpp"

using namespace adl::arinc665;

namespace {

using Decoder = std::function<bool(std::span<const std::byte>)>;

struct Subject {
    const char* name;
    std::vector<std::byte> good;
    Decoder decode;                // true if the bytes decode
    std::size_t unprotected_tail;  // bytes at the end that no CRC of the file itself covers
};

std::vector<Subject> subjects() {
    const auto members = build_media_set(adl::test::sample_media_set_spec()).value();
    return {
        {"header file", adl::test::sample_load().header_bytes,
         [](std::span<const std::byte> b) { return decode_header_file(b).has_value(); },
         4},  // the Load CRC
        {"batch file", encode_batch_file(adl::test::sample_batch()).value(),
         [](std::span<const std::byte> b) { return decode_batch_file(b).has_value(); }, 0},
        {"LOADS.LUM", adl::test::member_file(members[0], "\\", "LOADS.LUM"),
         [](std::span<const std::byte> b) { return decode_loads_list(b).has_value(); }, 0},
        {"FILES.LUM", adl::test::member_file(members[0], "\\", "FILES.LUM"),
         [](std::span<const std::byte> b) { return decode_files_list(b).has_value(); }, 0},
        {"BATCHES.LUM", adl::test::member_file(members[0], "\\", "BATCHES.LUM"),
         [](std::span<const std::byte> b) { return decode_batches_list(b).has_value(); }, 0},
    };
}

}  // namespace

TEST_CASE("every decoder refuses every truncation of a good file", "[robustness]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        REQUIRE(subject.decode(subject.good));
        for (std::size_t length = 0; length < subject.good.size(); ++length) {
            CAPTURE(length);
            CHECK_FALSE(subject.decode(std::span<const std::byte>{subject.good}.first(length)));
        }
    }
}

TEST_CASE("every decoder survives every single-bit change of a good file", "[robustness]") {
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        std::size_t accepted = 0;
        for (std::size_t byte = 0; byte + subject.unprotected_tail < subject.good.size(); ++byte) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                auto damaged = subject.good;
                damaged[byte] ^= static_cast<std::byte>(1U << bit);
                // A CRC catches a single-bit error, so none of these should decode. (The
                // header's Load CRC covers files this decoder has not got, and is left out.)
                if (subject.decode(damaged)) {
                    ++accepted;
                }
            }
        }
        CHECK(accepted == 0);
    }
}

TEST_CASE("every decoder survives damage the CRC does not catch", "[robustness]") {
    // The same, with the CRC put right afterwards, so the decoder has to rely on its own
    // checks of lengths and pointers. The outcome does not matter; reaching the end does.
    const auto fix = [](const Subject& subject, std::vector<std::byte>& bytes) {
        const std::string_view name = subject.name;
        if (name == "header file") {
            adl::test::refresh_header_crc(bytes);
        } else {
            adl::test::refresh_final_crc(bytes);
        }
    };
    std::size_t decoded = 0;
    std::size_t refused = 0;
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        for (std::size_t byte = 0; byte + 2 < subject.good.size(); ++byte) {
            for (const unsigned value : {0x00U, 0x01U, 0x7FU, 0x80U, 0xFFU}) {
                auto damaged = subject.good;
                damaged[byte] = static_cast<std::byte>(value);
                fix(subject, damaged);
                (subject.decode(damaged) ? decoded : refused) += 1;
            }
        }
    }
    CHECK(decoded + refused > 0);
    CHECK(refused > 0);
}

TEST_CASE("every decoder survives random bytes", "[robustness]") {
    std::mt19937 random{12345};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        for (int round = 0; round < 300; ++round) {
            std::vector<std::byte> noise(random() % 600);
            for (std::byte& b : noise) {
                b = static_cast<std::byte>(random() & 0xFFU);
            }
            CHECK_FALSE(subject.decode(noise));
        }
    }
}

TEST_CASE("every decoder survives random bytes behind a plausible start", "[robustness]") {
    // The right length and format version, so the decoder gets past the framing and into the
    // fields, with random contents and the CRC put right.
    std::mt19937 random{54321};
    for (const Subject& subject : subjects()) {
        CAPTURE(subject.name);
        for (int round = 0; round < 300; ++round) {
            auto bytes = subject.good;
            for (std::size_t i = 8; i + 4 < bytes.size(); ++i) {
                if (random() % 5 == 0) {
                    bytes[i] = static_cast<std::byte>(random() & 0xFFU);
                }
            }
            if (std::string_view{subject.name} == "header file") {
                adl::test::refresh_header_crc(bytes);
            } else {
                adl::test::refresh_final_crc(bytes);
            }
            static_cast<void>(subject.decode(bytes));
        }
    }
    SUCCEED();
}

TEST_CASE("reading a media set survives damaged members", "[robustness]") {
    const auto good = build_media_set(adl::test::sample_media_set_spec()).value();
    std::mt19937 random{777};

    for (int round = 0; round < 200; ++round) {
        auto members = good;
        MediaMember& member = members[random() % members.size()];
        MediaFile& file = member.files[random() % member.files.size()];
        if (file.bytes.empty()) {
            continue;
        }
        file.bytes[random() % file.bytes.size()] ^= static_cast<std::byte>(1U + random() % 255U);
        // Whatever it makes of it, it must come back.
        const auto result = read_media_set(members);
        CHECK_FALSE(result.has_value());  // every file is covered by a CRC somewhere
    }
}
