// The library's packets and protocol files, byte for byte, against the ones written
// independently by tests/golden/make_golden_615a.py from the tables of the standard.

#include "bytes.hpp"
#include "codec_615a.hpp"
#include "golden_615a.hpp"
#include "sample_615a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <span>
#include <vector>

using namespace adl::arinc615a;
using adl::test::from_hex;
using adl::test::to_bytes;
namespace golden = adl::test::golden;
namespace sample = adl::test::sample;

namespace
{

// The library writes `value` as the independent writer did, and reads the independent
// bytes back to the same value.
template <typename T, typename Decode>
void agrees(const T& value, std::string_view hex, Decode decode)
{
    const auto expected = from_hex(hex);
    CHECK(to_bytes(value) == expected);

    const auto decoded = decode(expected);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == value);
}

template <typename T>
Result<T> decode_as(std::span<const std::byte> bytes)
{
    const auto packet = decode_packet(bytes);
    if (!packet)
    {
        return std::unexpected{packet.error()};
    }
    if (const T* wanted = std::get_if<T>(&*packet))
    {
        return *wanted;
    }
    return std::unexpected{Error::invalid_value};
}

} // namespace

TEST_CASE("the TFTP requests are the ones written independently", "[golden615a][tftp]")
{
    agrees(sample::rrq_lui(), golden::rrq_lui_hex, decode_as<TftpRequest>);
    agrees(sample::rrq_checksum(), golden::rrq_checksum_hex, decode_as<TftpRequest>);
    agrees(sample::wrq_lus(), golden::wrq_lus_hex, decode_as<TftpRequest>);
    agrees(sample::wrq_plain(), golden::wrq_plain_hex, decode_as<TftpRequest>);
}

TEST_CASE("the option acknowledgement is the one written independently", "[golden615a][tftp]")
{
    agrees(sample::oack(), golden::oack_hex, decode_as<TftpOptionAck>);
}

TEST_CASE("the ACK, DATA and error packets are the ones written independently", "[golden615a][tftp]")
{
    CHECK(to_bytes(TftpAck{0}) == from_hex(golden::ack_0_hex));
    CHECK(to_bytes(TftpAck{65535}) == from_hex(golden::ack_65535_hex));
    CHECK(std::get<TftpAck>(*decode_packet(from_hex(golden::ack_65535_hex))).block == 65535);

    CHECK(to_bytes(TftpData{1, sample::payload32()}) == from_hex(golden::data_1_hex));
    CHECK(to_bytes(TftpData{65535, sample::payload32().first(5)}) == from_hex(golden::data_65535_hex));
    CHECK(to_bytes(TftpData{2, {}}) == from_hex(golden::data_empty_hex));

    CHECK(to_bytes(TftpError{TftpErrorCode::not_defined, "WAIT:50"}) == from_hex(golden::error_wait_hex));
    CHECK(to_bytes(TftpError{TftpErrorCode::not_defined, "ABORT:1004"}) == from_hex(golden::error_abort_hex));
    CHECK(to_bytes(TftpError{TftpErrorCode::file_not_found, "File not found"}) ==
          from_hex(golden::error_file_not_found_hex));
}

TEST_CASE("the WAIT and ABORT messages are the ones written independently", "[golden615a][tftp]")
{
    std::array<std::byte, 32> out{};
    auto wait = encode_wait(WaitMessage{50}, out);
    REQUIRE(wait.has_value());
    CHECK(std::vector<std::byte>(out.begin(), out.begin() + static_cast<long>(*wait)) ==
          from_hex(golden::error_wait_hex));

    auto abort = encode_abort(AbortMessage{StatusCode::aborted_by_data_loader}, out);
    REQUIRE(abort.has_value());
    CHECK(std::vector<std::byte>(out.begin(), out.begin() + static_cast<long>(*abort)) ==
          from_hex(golden::error_abort_hex));

    const auto meaning = interpret_error(std::get<TftpError>(*decode_packet(from_hex(golden::error_abort_hex))));
    REQUIRE(meaning.has_value());
    CHECK(*meaning == ErrorMeaning{AbortMessage{StatusCode::aborted_by_data_loader}});
}

TEST_CASE("the options the builder writes are the ones written independently", "[golden615a][tftp]")
{
    OptionBuilder builder;
    REQUIRE(builder.add_block_size(1024).has_value());
    REQUIRE(builder.add_transfer_size(0).has_value());
    REQUIRE(builder.add_port(1615).has_value());
    CHECK(to_bytes(TftpRequest{TftpRequest::Kind::read, "HNPFMS_L.LUI", builder.list()}) ==
          from_hex(golden::rrq_lui_hex));
}

TEST_CASE("the FIND packets are the ones written independently", "[golden615a][find]")
{
    std::array<std::byte, 8> out{};
    REQUIRE(encode_find_request(out).value() == 4);
    CHECK(std::vector<std::byte>(out.begin(), out.begin() + 4) == from_hex(golden::find_request_hex));

    CHECK(to_bytes(sample::find_answer()) == from_hex(golden::find_answer_hex));
    CHECK(to_bytes(FindAnswer{}) == from_hex(golden::find_answer_empty_hex));
    const auto answer_bytes = from_hex(golden::find_answer_hex);
    const auto decoded = decode_find(answer_bytes);
    REQUIRE(decoded.has_value());
    CHECK(std::get<FindAnswer>(*decoded) == sample::find_answer());
}

TEST_CASE("the .LCI, .LUI, .LND and .LNO are the ones written independently", "[golden615a][protocol-files]")
{
    agrees(sample::initialization_accepted(), golden::initialization_accepted_hex, decode_initialization_file);
    agrees(sample::initialization_denied(), golden::initialization_denied_hex, decode_initialization_file);
}

TEST_CASE("the .LCL and .LCS are the ones written independently", "[golden615a][protocol-files]")
{
    agrees(sample::configuration_list(), golden::configuration_list_hex, decode_configuration_list);
    agrees(sample::configuration_status(), golden::configuration_status_hex, decode_configuration_status);
    agrees(sample::configuration_status_idle(), golden::configuration_status_idle_hex, decode_configuration_status);
}

TEST_CASE("the .LUR and .LUS are the ones written independently", "[golden615a][protocol-files]")
{
    agrees(sample::upload_request(), golden::upload_request_hex, decode_upload_request);
    agrees(sample::upload_status(), golden::upload_status_hex, decode_upload_status);
    agrees(sample::upload_status_start(), golden::upload_status_start_hex, decode_upload_status);
}

TEST_CASE("the .LNR, .LNS, .LNL and .LNA are the ones written independently", "[golden615a][protocol-files]")
{
    agrees(sample::download_request(), golden::download_request_hex, decode_download_request);
    agrees(sample::download_status(), golden::download_status_hex, decode_download_status);
    agrees(sample::download_list(), golden::download_list_hex, decode_download_list);
    agrees(sample::download_answer(), golden::download_answer_hex, decode_download_answer);
}
