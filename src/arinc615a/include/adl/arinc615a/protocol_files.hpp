#pragma once

// The protocol files of ARINC 615A-3 section 6.4, which the target and the data loader
// trade as ordinary TFTP transfers. Each has an encoder that writes into a buffer the
// caller supplies and a decoder that checks every length and returns views over the bytes
// it was given.
//
// All of them start with the file length (32 bits, in bytes, counting itself) and the
// Protocol Version ("A4"), and end exactly where the file length says. The numbers are big
// endian. A text is an 8-bit length, then that many bytes ending in a NUL: see
// docs/arinc615a-codec.md for how this library reads the standard's "length" and for the
// other places it had to choose.
//
// A decoded file views the bytes that were decoded, and so do its lists: those bytes must
// outlive it.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "adl/arinc615a/error.hpp"
#include "adl/arinc615a/list.hpp"
#include "adl/arinc615a/status_code.hpp"

namespace adl::arinc615a {

// The twelve protocol files, by the extension of their names: <THW_ID_POS>.LCI and so on.
// The names are case sensitive.
enum class ProtocolFileKind {
    lci,  // load configuration initialization: starts an Information operation
    lcl,  // load configuration list: what the target holds
    lcs,  // load configuration status
    lui,  // load uploading initialization: starts an Upload operation
    lur,  // load uploading request: the loads to upload
    lus,  // load uploading status
    lnd,  // load downloading, media defined: starts a Download operation
    lno,  // load downloading, operator defined: starts a Download operation
    lnr,  // load downloading request: the files to download (media defined)
    lns,  // load downloading status
    lnl,  // load downloading list: the files that could be downloaded (operator defined)
    lna,  // load downloading answer: the files the operator chose
};

// ".LCI" and so on.
[[nodiscard]] std::string_view extension(ProtocolFileKind kind) noexcept;

// The kind of a file name, found by its extension (which is case sensitive and must follow
// at least one character of <THW_ID_POS>), or nothing for any other name.
[[nodiscard]] std::optional<ProtocolFileKind> kind_of_file_name(
    std::string_view file_name) noexcept;

// What a file's Protocol Version field says, for a data loader that is to refuse an
// unknown version with an ABORT (section 6.4). Error::truncated if there is no field;
// the version is not checked.
[[nodiscard]] Result<std::array<char, 2>> peek_protocol_version(
    std::span<const std::byte> file) noexcept;

// ---- .LCI .LUI .LND .LNO ---------------------------------------------------------------

// The target's answer to the start of an operation (section 6.4.1): the status is
// accepted, denied (the description says why) or not supported.
struct InitializationFile {
    StatusCode status = StatusCode::accepted;
    std::string_view description;  // 0 to 254 characters; empty is "none"

    friend bool operator==(const InitializationFile&, const InitializationFile&) = default;
};

// ---- .LCL ------------------------------------------------------------------------------

// A part of a piece of hardware (section 6.4.2).
struct HardwarePart {
    std::string_view part_number;  // 1 to 254 characters
    std::string_view amendment;    // 0 to 254; empty when there is none
    std::string_view designation;  // 1 to 254: "FMGEC 1", "FMS 1 OPS"

    friend bool operator==(const HardwarePart&, const HardwarePart&) = default;
};

// A piece of hardware the target reports, with its parts.
struct HardwareItem {
    std::string_view literal_name;   // 1 to 254; the FIND answer's Literal Name
    std::string_view serial_number;  // 1 to 254
    List<HardwarePart> parts;

    friend bool operator==(const HardwareItem&, const HardwareItem&) = default;
};

struct ConfigurationList {
    List<HardwareItem> hardware;

    friend bool operator==(const ConfigurationList&, const ConfigurationList&) = default;
};

// ---- .LCS ------------------------------------------------------------------------------

// The target's progress report in an Information operation (section 6.4.3). The counter
// goes up by one with each status file the target sends, from 0, and wraps; the exception
// timer is the seconds the target will stay silent; the estimated time is the seconds
// left, or estimated_time_unknown (limits.hpp). For a status other than 2 or 4 the
// standard has both fields 0; the decoders accept what they find.
struct ConfigurationStatus {
    std::uint16_t counter = 0;
    StatusCode status = StatusCode::accepted;
    std::uint16_t exception_timer = 0;
    std::uint16_t estimated_time = 0;
    std::string_view description;

    friend bool operator==(const ConfigurationStatus&, const ConfigurationStatus&) = default;
};

// ---- .LUR ------------------------------------------------------------------------------

// One load to upload: its header file and its part number (section 6.4.4).
struct LoadReference {
    std::string_view header_file_name;       // 1 to 254 characters
    std::string_view load_part_number_name;  // 1 to 254

    friend bool operator==(const LoadReference&, const LoadReference&) = default;
};

// The loads the data loader will upload; at least one.
struct UploadRequest {
    List<LoadReference> loads;

    friend bool operator==(const UploadRequest&, const UploadRequest&) = default;
};

// ---- .LUS ------------------------------------------------------------------------------

// How one load is going (section 6.4.5). The ratio is the percentage done, 0 to 100, which
// the file holds as three characters right adjusted with blanks.
struct LoadProgress {
    std::string_view header_file_name;
    std::string_view load_part_number_name;
    std::uint8_t ratio = 0;
    StatusCode status = StatusCode::accepted;
    std::string_view description;

    friend bool operator==(const LoadProgress&, const LoadProgress&) = default;
};

// The target's progress report in an Upload operation. Before and during the list
// transfer there are no loads.
struct UploadStatus {
    StatusCode status = StatusCode::accepted;
    std::string_view description;
    std::uint16_t counter = 0;
    std::uint16_t exception_timer = 0;
    std::uint16_t estimated_time = 0;
    std::uint8_t list_ratio = 0;
    List<LoadProgress> loads;

    friend bool operator==(const UploadStatus&, const UploadStatus&) = default;
};

// ---- .LNR ------------------------------------------------------------------------------

// A file to download, and up to 255 bytes the target wants kept with it (section 6.4.6).
struct FileRequest {
    std::string_view file_name;
    std::span<const std::byte> user_defined_data;

    [[nodiscard]] friend bool operator==(const FileRequest& a, const FileRequest& b) noexcept {
        return a.file_name == b.file_name &&
               std::ranges::equal(a.user_defined_data, b.user_defined_data);
    }
};

struct DownloadRequest {
    List<FileRequest> files;

    friend bool operator==(const DownloadRequest&, const DownloadRequest&) = default;
};

// ---- .LNS ------------------------------------------------------------------------------

// How one file is going (section 6.4.7).
struct FileProgress {
    std::string_view file_name;
    StatusCode status = StatusCode::accepted;
    std::string_view description;

    friend bool operator==(const FileProgress&, const FileProgress&) = default;
};

struct DownloadStatus {
    StatusCode status = StatusCode::accepted;
    std::string_view description;
    std::uint16_t counter = 0;
    std::uint16_t exception_timer = 0;
    std::uint16_t estimated_time = 0;
    std::uint8_t list_ratio = 0;
    List<FileProgress> files;

    friend bool operator==(const DownloadStatus&, const DownloadStatus&) = default;
};

// ---- .LNL ------------------------------------------------------------------------------

// A file the target could give (section 6.4.8).
struct FileListing {
    std::string_view file_name;    // 1 to 254 characters
    std::string_view description;  // 0 to 254; empty when there is none

    friend bool operator==(const FileListing&, const FileListing&) = default;
};

struct DownloadList {
    List<FileListing> files;

    friend bool operator==(const DownloadList&, const DownloadList&) = default;
};

// ---- .LNA ------------------------------------------------------------------------------

// The files the operator chose from a DownloadList (section 6.4.9): their names.
struct DownloadAnswer {
    List<std::string_view> file_names;

    friend bool operator==(const DownloadAnswer&, const DownloadAnswer&) = default;
};

// ---- Encoding and decoding -------------------------------------------------------------

// Each encoder writes the file at the start of `out` and returns its length; it fails with
// Error::buffer_too_small if the file does not fit, Error::field_too_large for a text, a
// list or a number that does not fit its field, and Error::invalid_value for what the
// standard forbids (an empty required text, a ratio over 100, a status code the field
// cannot hold). encoded_size() runs the same checks and says how big a buffer to supply.
// Each decoder returns Error::truncated for a file that ends early, Error::bad_length for
// a file length that disagrees with the bytes, Error::bad_version unless the version is
// "A4", and the errors of the fields after that.

[[nodiscard]] Result<std::size_t> encode_initialization_file(const InitializationFile& file,
                                                             std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const InitializationFile& file) noexcept;
[[nodiscard]] Result<InitializationFile> decode_initialization_file(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_configuration_list(const ConfigurationList& file,
                                                            std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const ConfigurationList& file) noexcept;
[[nodiscard]] Result<ConfigurationList> decode_configuration_list(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_configuration_status(const ConfigurationStatus& file,
                                                              std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const ConfigurationStatus& file) noexcept;
[[nodiscard]] Result<ConfigurationStatus> decode_configuration_status(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_upload_request(const UploadRequest& file,
                                                        std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const UploadRequest& file) noexcept;
[[nodiscard]] Result<UploadRequest> decode_upload_request(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_upload_status(const UploadStatus& file,
                                                       std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const UploadStatus& file) noexcept;
[[nodiscard]] Result<UploadStatus> decode_upload_status(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_download_request(const DownloadRequest& file,
                                                          std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const DownloadRequest& file) noexcept;
[[nodiscard]] Result<DownloadRequest> decode_download_request(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_download_status(const DownloadStatus& file,
                                                         std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const DownloadStatus& file) noexcept;
[[nodiscard]] Result<DownloadStatus> decode_download_status(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_download_list(const DownloadList& file,
                                                       std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const DownloadList& file) noexcept;
[[nodiscard]] Result<DownloadList> decode_download_list(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] Result<std::size_t> encode_download_answer(const DownloadAnswer& file,
                                                         std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encoded_size(const DownloadAnswer& file) noexcept;
[[nodiscard]] Result<DownloadAnswer> decode_download_answer(
    std::span<const std::byte> bytes) noexcept;

}  // namespace adl::arinc615a
