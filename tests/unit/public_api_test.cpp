// The small public vocabulary of the two libraries: the enums, their names, the check value
// lengths and the arithmetic of the engine's clock. These are the values a log line, a dump and a
// protocol field show, so every one of them is named here rather than only the paths the larger
// tests happen to take.

#include "adl/arinc615a/checksum.hpp"
#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/error.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp_transfer.hpp"
#include "adl/arinc615a/time.hpp"
#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/error.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace
{

using adl::arinc615a::Duration;
using adl::arinc615a::Instant;

[[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text)
{
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(text[i]);
    }
    return bytes;
}

// The listener with no overrides: every default has to be callable and harmless.
class SilentListener final : public adl::arinc615a::OperationListener
{
};

// One CRC type: the whole string, then the same bytes in two pieces after a reset, and the
// one-call helper. The value is not a check value from the standard (crc_test.cpp holds those);
// this is about the generic machinery being the same however it is fed.
template <typename CrcType>
void check_generic_crc(std::span<const std::byte> bytes)
{
    CrcType crc;
    crc.update(bytes);
    const typename CrcType::value_type whole = crc.value();
    crc.reset();
    crc.update(bytes.first(4));
    crc.update(bytes.subspan(4));
    CHECK(crc.value() == whole);
    CHECK(adl::arinc665::crc_of<CrcType>(bytes) == whole);
}

} // namespace

TEST_CASE("every 615A failure has a sentence", "[error][to-string]")
{
    using adl::arinc615a::Error;
    constexpr std::array<Error, 10> errors{
        Error::buffer_too_small, Error::truncated,   Error::bad_length,     Error::unknown_opcode,
        Error::unsupported_mode, Error::bad_version, Error::malformed_text, Error::invalid_value,
        Error::field_too_large,  Error::bad_option,
    };
    for (const Error error : errors)
    {
        const std::string_view text = adl::arinc615a::to_string(error);
        CHECK_FALSE(text.empty());
        CHECK(text != "unknown error");
    }
}

TEST_CASE("every 665 failure has a sentence", "[error][to-string]")
{
    using adl::arinc665::Error;
    constexpr std::array<Error, 11> errors{
        Error::unsupported_check_type,
        Error::truncated,
        Error::bad_length,
        Error::bad_version,
        Error::bad_pointer,
        Error::bad_crc,
        Error::bad_check_value,
        Error::field_too_large,
        Error::invalid_value,
        Error::inconsistent,
        Error::file_not_found,
    };
    for (const Error error : errors)
    {
        const std::string_view text = adl::arinc665::to_string(error);
        CHECK_FALSE(text.empty());
        CHECK(text != "unknown error");
    }
}

TEST_CASE("every status code says what it means, and which fields may carry it", "[status][to-string]")
{
    using adl::arinc615a::StatusCode;
    using adl::arinc615a::StatusField;
    constexpr std::array<StatusCode, 10> codes{
        StatusCode::accepted,
        StatusCode::in_progress,
        StatusCode::completed,
        StatusCode::in_progress_with_text,
        StatusCode::operation_denied,
        StatusCode::operation_not_supported,
        StatusCode::aborted_by_target,
        StatusCode::aborted_by_data_loader,
        StatusCode::aborted_by_operator,
        StatusCode::item_failed,
    };
    for (const StatusCode code : codes)
    {
        CHECK(adl::arinc615a::is_known(code));
        CHECK_FALSE(adl::arinc615a::to_string(code).empty());
        CHECK((adl::arinc615a::allowed_in(StatusField::operation, code) || code == StatusCode::operation_denied ||
               code == StatusCode::operation_not_supported || code == StatusCode::item_failed));
    }
    CHECK_FALSE(adl::arinc615a::is_known(static_cast<StatusCode>(0x9999)));
    CHECK(adl::arinc615a::allowed_in(StatusField::acceptance, StatusCode::operation_denied));
    CHECK_FALSE(adl::arinc615a::allowed_in(StatusField::acceptance, StatusCode::in_progress));
    CHECK(adl::arinc615a::allowed_in(StatusField::item, StatusCode::item_failed));
    CHECK(adl::arinc615a::allowed_in(StatusField::item, StatusCode::accepted));
    CHECK(adl::arinc615a::description_is_meaningful(StatusCode::operation_denied));
    CHECK_FALSE(adl::arinc615a::description_is_meaningful(StatusCode::completed));
}

TEST_CASE("every transfer outcome and checksum verdict has a sentence", "[to-string]")
{
    using adl::arinc615a::ChecksumVerdict;
    using adl::arinc615a::TransferOutcome;
    constexpr std::array<TransferOutcome, 11> outcomes{
        TransferOutcome::running,        TransferOutcome::succeeded,      TransferOutcome::timed_out,
        TransferOutcome::peer_error,     TransferOutcome::peer_abort,     TransferOutcome::wait_requested,
        TransferOutcome::option_refused, TransferOutcome::protocol_error, TransferOutcome::size_mismatch,
        TransferOutcome::local_error,    TransferOutcome::aborted,
    };
    for (const TransferOutcome outcome : outcomes)
    {
        CHECK_FALSE(std::string_view{adl::arinc615a::to_string(outcome)}.empty());
    }
    constexpr std::array<ChecksumVerdict, 4> verdicts{
        ChecksumVerdict::not_given,
        ChecksumVerdict::unsupported,
        ChecksumVerdict::matched,
        ChecksumVerdict::mismatched,
    };
    for (const ChecksumVerdict verdict : verdicts)
    {
        CHECK_FALSE(std::string_view{adl::arinc615a::to_string(verdict)}.empty());
    }
}

TEST_CASE("every notice has a sentence", "[notice][to-string]")
{
    using adl::arinc615a::Notice;
    constexpr std::array<Notice, 13> notices{
        Notice::status_counter_gap, Notice::status_duplicate,
        Notice::status_stale,       Notice::status_file_failed,
        Notice::checksum_mismatch,  Notice::checksum_unsupported,
        Notice::file_not_available, Notice::request_refused,
        Notice::transfer_failed,    Notice::waiting,
        Notice::option_fallback,    Notice::port_option_ignored,
        Notice::files_missing,
    };
    for (const Notice notice : notices)
    {
        CHECK_FALSE(std::string_view{adl::arinc615a::to_string(notice)}.empty());
    }
}

TEST_CASE("every check value type has a name and the length Table 5.1 gives it", "[check-value][to-string]")
{
    using adl::arinc665::CheckValueType;
    struct Entry
    {
        CheckValueType type;
        std::size_t length;
    };
    constexpr std::array<Entry, 6> types{{
        {CheckValueType::none, 0},
        {CheckValueType::crc8, 2},
        {CheckValueType::crc16, 2},
        {CheckValueType::crc32, 4},
        {CheckValueType::md5, 16},
        {CheckValueType::sha1, 20},
    }};
    for (const Entry& entry : types)
    {
        CHECK_FALSE(adl::arinc665::to_string(entry.type).empty());
        CHECK(adl::arinc665::check_value_length(entry.type) == entry.length);
    }
}

TEST_CASE("the engine's clock saturates instead of wrapping", "[time][limits]")
{
    using adl::arinc615a::never;
    CHECK(never + Duration{1} == never);
    CHECK(Instant{Duration::max()} + Duration{1} == never);
    CHECK(Instant{Duration::max()} - Duration{-1} == never);
    CHECK(Instant{Duration::min()} - Duration{1} == Instant{Duration::min()});
    CHECK(Instant{Duration::min()} + Duration{-1} == Instant{Duration::min()});
    CHECK(Instant{Duration{10}} + Duration{5} == Instant{Duration{15}});
    CHECK(Instant{Duration{10}} - Duration{5} == Instant{Duration{5}});
    CHECK(Instant{Duration{10}} - Instant{Duration{5}} == Duration{5});
    CHECK(adl::arinc615a::earlier_of(Instant{Duration{5}}, Instant{Duration{9}}) == Instant{Duration{5}});
    CHECK(adl::arinc615a::whole_seconds(3) == std::chrono::seconds{3});
}

TEST_CASE("the generic CRC starts again, can be fed in pieces, and crc_of agrees", "[crc][generic]")
{
    using adl::arinc665::Crc16;
    using adl::arinc665::Crc32;
    using adl::arinc665::Crc8;
    const std::vector<std::byte> bytes = bytes_of("123456789");
    check_generic_crc<Crc8>(bytes);
    check_generic_crc<Crc16>(bytes);
    check_generic_crc<Crc32>(bytes);
}

TEST_CASE("the default listener says nothing and cancels a choice", "[loader][listener]")
{
    SilentListener listener;
    adl::arinc615a::FileSelection selection;
    CHECK(selection.select("A.BIN"));
    CHECK(selection.size() == 1);
    CHECK(selection.names().size() == 1);
    CHECK(selection.names().front() == "A.BIN");
    selection.clear();
    CHECK(selection.size() == 0);

    const adl::arinc615a::InitializationFile initialization{};
    const adl::arinc615a::ConfigurationStatus configuration{};
    const adl::arinc615a::UploadStatus upload{};
    const adl::arinc615a::DownloadStatus download{};
    const adl::arinc615a::ConfigurationList list{};
    const adl::arinc615a::DownloadList files{};
    const adl::arinc615a::ReceivedFile received{};
    listener.on_initialization(initialization);
    listener.on_configuration_status(configuration);
    listener.on_upload_status(upload);
    listener.on_download_status(download);
    listener.on_configuration_list(list);
    listener.on_file_received("A.BIN", received);
    listener.on_file_sent("A.BIN", 1);
    listener.on_notice(adl::arinc615a::Notice::waiting, "target", 5);
    CHECK(listener.on_download_list(files, selection) == adl::arinc615a::SelectionDecision::cancelled);
}
