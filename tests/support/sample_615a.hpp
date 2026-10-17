#pragma once

// The packets and protocol files that tests/golden/make_golden_615a.py writes, built
// with the library's types, input for input, so that the two can be compared byte for
// byte. The lists they hold view static arrays, so what these return stays valid.

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/tftp.hpp"

namespace adl::test::sample {

using namespace adl::arinc615a;

// ---- TFTP ------------------------------------------------------------------------------

inline TftpRequest rrq_lui() {
    static constexpr std::array options = {TftpOption{"blksize", "1024"}, TftpOption{"tsize", "0"},
                                           TftpOption{"port", "1615"}};
    return {TftpRequest::Kind::read, "HNPFMS_L.LUI", options};
}

inline TftpRequest rrq_checksum() {
    static constexpr std::array options = {TftpOption{"checksum_2", "B1C2"}};
    return {TftpRequest::Kind::read, "APP.LUP", options};
}

inline TftpRequest wrq_lus() {
    static constexpr std::array options = {TftpOption{"blksize", "1400"}, TftpOption{"tsize", "87"},
                                           TftpOption{"timeout", "2"}};
    return {TftpRequest::Kind::write, "HNPFMS_L.LUS", options};
}

inline TftpRequest wrq_plain() {
    return {TftpRequest::Kind::write, "HNPFMS_L.LCS", {}};
}

inline TftpOptionAck oack() {
    static constexpr std::array options = {TftpOption{"blksize", "1024"},
                                           TftpOption{"checksum_3", "DEADBEEF"}};
    return {options};
}

// The bytes 00 01 ... 1F.
inline std::span<const std::byte> payload32() {
    static const auto bytes = [] {
        std::array<std::byte, 32> a{};
        for (std::size_t i = 0; i < a.size(); ++i) {
            a[i] = static_cast<std::byte>(i);
        }
        return a;
    }();
    return bytes;
}

// ---- FIND ------------------------------------------------------------------------------

inline FindAnswer find_answer() {
    return {"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
}

// ---- Protocol files --------------------------------------------------------------------

inline InitializationFile initialization_accepted() {
    return {StatusCode::accepted, ""};
}

inline InitializationFile initialization_denied() {
    return {StatusCode::operation_denied, "Busy loading"};
}

inline ConfigurationList configuration_list() {
    static constexpr std::array left = {HardwarePart{"HNP-HW-0001", "A", "FMGEC 1"},
                                        HardwarePart{"ACM47-1234-5678", "", "FMS 1 OPS"}};
    static constexpr std::array right = {HardwarePart{"HNP-HW-0001", "B", "FMGEC 2"}};
    static const std::array hardware = {HardwareItem{"FMS LEFT", "SN0001", left},
                                        HardwareItem{"FMS RIGHT", "SN0002", right}};
    return {hardware};
}

inline ConfigurationStatus configuration_status() {
    return {7, StatusCode::in_progress_with_text, 30, 120, "Erasing flash"};
}

inline ConfigurationStatus configuration_status_idle() {
    return {0, StatusCode::accepted, 0, 0, ""};
}

inline UploadRequest upload_request() {
    static constexpr std::array loads = {LoadReference{"ACM47.LUH", "ACM47-1234-5678"},
                                         LoadReference{"ACM48.LUH", "ACM4F-5555-6666"}};
    return {loads};
}

inline UploadStatus upload_status() {
    static constexpr std::array loads = {
        LoadProgress{"ACM47.LUH", "ACM47-1234-5678", 100, StatusCode::completed, ""},
        LoadProgress{"ACM48.LUH", "ACM4F-5555-6666", 5, StatusCode::in_progress_with_text,
                     "Erasing"}};
    return {StatusCode::in_progress_with_text,
            "Programming",
            65535,
            10,
            estimated_time_unknown,
            45,
            loads};
}

inline UploadStatus upload_status_start() {
    return {StatusCode::accepted, "", 0, 0, 0, 0, {}};
}

inline DownloadRequest download_request() {
    static constexpr std::array dead_beef = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE},
                                             std::byte{0xEF}};
    static constexpr std::array files = {FileRequest{"LOG1.BIN", dead_beef},
                                         FileRequest{"LOG2.BIN", {}}};
    return {files};
}

inline DownloadStatus download_status() {
    static constexpr std::array files = {
        FileProgress{"LOG1.BIN", StatusCode::completed, ""},
        FileProgress{"LOG2.BIN", StatusCode::item_failed, "Disk error"}};
    return {StatusCode::in_progress, "", 300, 5, 60, 50, files};
}

inline DownloadList download_list() {
    static constexpr std::array files = {FileListing{"LOG1.BIN", "Maintenance log"},
                                         FileListing{"LOG2.BIN", ""}};
    return {files};
}

inline DownloadAnswer download_answer() {
    static constexpr std::array names = {std::string_view{"LOG1.BIN"},
                                         std::string_view{"LOG2.BIN"}};
    return {names};
}

}  // namespace adl::test::sample
