#pragma once

// Helpers for the 615A codec tests: encode a value into a vector of exactly the size the
// library says it needs, as the caller of an embedded build would size its buffer.

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <span>
#include <vector>

#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "adl/arinc615a/tftp.hpp"

namespace adl::test {

template <typename T, typename Encode>
std::vector<std::byte> encode_to_vector(const T& value, Encode encode) {
    const auto size = adl::arinc615a::encoded_size(value);
    REQUIRE(size.has_value());
    std::vector<std::byte> bytes(*size);
    const auto written = encode(value, std::span<std::byte>{bytes});
    REQUIRE(written.has_value());
    REQUIRE(*written == *size);
    return bytes;
}

using namespace adl::arinc615a;

inline std::vector<std::byte> to_bytes(const TftpRequest& v) {
    return encode_to_vector(TftpPacket{v}, encode_packet);
}
inline std::vector<std::byte> to_bytes(const TftpData& v) {
    return encode_to_vector(TftpPacket{v}, encode_packet);
}
inline std::vector<std::byte> to_bytes(const TftpAck& v) {
    return encode_to_vector(TftpPacket{v}, encode_packet);
}
inline std::vector<std::byte> to_bytes(const TftpError& v) {
    return encode_to_vector(TftpPacket{v}, encode_packet);
}
inline std::vector<std::byte> to_bytes(const TftpOptionAck& v) {
    return encode_to_vector(TftpPacket{v}, encode_packet);
}
inline std::vector<std::byte> to_bytes(const FindAnswer& v) {
    return encode_to_vector(v, encode_find_answer);
}
inline std::vector<std::byte> to_bytes(const InitializationFile& v) {
    return encode_to_vector(v, encode_initialization_file);
}
inline std::vector<std::byte> to_bytes(const ConfigurationList& v) {
    return encode_to_vector(v, encode_configuration_list);
}
inline std::vector<std::byte> to_bytes(const ConfigurationStatus& v) {
    return encode_to_vector(v, encode_configuration_status);
}
inline std::vector<std::byte> to_bytes(const UploadRequest& v) {
    return encode_to_vector(v, encode_upload_request);
}
inline std::vector<std::byte> to_bytes(const UploadStatus& v) {
    return encode_to_vector(v, encode_upload_status);
}
inline std::vector<std::byte> to_bytes(const DownloadRequest& v) {
    return encode_to_vector(v, encode_download_request);
}
inline std::vector<std::byte> to_bytes(const DownloadStatus& v) {
    return encode_to_vector(v, encode_download_status);
}
inline std::vector<std::byte> to_bytes(const DownloadList& v) {
    return encode_to_vector(v, encode_download_list);
}
inline std::vector<std::byte> to_bytes(const DownloadAnswer& v) {
    return encode_to_vector(v, encode_download_answer);
}

}  // namespace adl::test
