// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc615a/protocol_files.hpp"

#include "adl/arinc615a/limits.hpp"
#include "wire.hpp"

#include <array>

namespace adl::arinc615a
{

namespace
{

using detail::Reader;
using detail::Writer;

// ---- The frame every file shares: file length, then the protocol version ---------------

void begin_file(Writer& writer) noexcept
{
    writer.u32(0); // the file length, put right at the end
    writer.chars(protocol_version);
}

void end_file(Writer& writer) noexcept
{
    if (writer.size() > 0xFFFFFFFFU)
    {
        writer.fail(Error::field_too_large);
        return;
    }
    writer.patch_u32(0, static_cast<std::uint32_t>(writer.size()));
}

template <typename File, typename Body>
Result<std::size_t> encode_file(const File& file, std::span<std::byte> out, Body body) noexcept
{
    Writer writer{out};
    begin_file(writer);
    body(writer, file);
    end_file(writer);
    return writer.finish();
}

template <typename File, typename Body>
Result<std::size_t> measure_file(const File& file, Body body) noexcept
{
    Writer writer{std::span<std::byte>{}};
    begin_file(writer);
    body(writer, file);
    end_file(writer);
    return writer.measure();
}

template <typename File, typename Body>
Result<File> decode_file(std::span<const std::byte> bytes, Body body) noexcept
{
    Reader reader{bytes};
    const std::uint32_t length = reader.u32();
    const auto version = reader.bytes(2);
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    if (static_cast<char>(version[0]) != protocol_version[0] || static_cast<char>(version[1]) != protocol_version[1])
    {
        return std::unexpected{Error::bad_version};
    }
    if (length > bytes.size())
    {
        return std::unexpected{Error::truncated};
    }
    if (length != bytes.size())
    {
        return std::unexpected{Error::bad_length};
    }
    File file{};
    body(reader, file);
    reader.expect_end();
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    return file;
}

// ---- Fields ----------------------------------------------------------------------------

StatusCode read_status(Reader& reader, StatusField field) noexcept
{
    const auto code = static_cast<StatusCode>(reader.u16());
    if (reader.ok() && !allowed_in(field, code))
    {
        reader.fail(Error::invalid_value);
    }
    return code;
}

void write_status(Writer& writer, StatusField field, StatusCode code) noexcept
{
    if (!allowed_in(field, code))
    {
        writer.fail(Error::invalid_value);
    }
    writer.u16(static_cast<std::uint16_t>(code));
}

// ---- Lists -----------------------------------------------------------------------------

template <typename T, T (*Read)(Reader&) noexcept>
T read_entry(std::span<const std::byte>& rest) noexcept
{
    Reader reader{rest};
    T entry = Read(reader);
    rest = rest.subspan(reader.position());
    return entry;
}

// Reads `count` entries, checking each, and keeps the bytes they took.
template <typename T, T (*Read)(Reader&) noexcept>
List<T> read_list(Reader& reader, std::size_t count) noexcept
{
    const auto start = reader.rest();
    const std::size_t first = reader.position();
    for (std::size_t i = 0; i < count && reader.ok(); ++i)
    {
        static_cast<void>(Read(reader));
    }
    if (!reader.ok())
    {
        return {};
    }
    return List<T>::from_wire(start.first(reader.position() - first), count, &read_entry<T, Read>);
}

// ---- Entries ---------------------------------------------------------------------------

LoadReference read_load_reference(Reader& reader) noexcept
{
    LoadReference entry;
    entry.header_file_name = reader.length_text(true);
    entry.load_part_number_name = reader.length_text(true);
    return entry;
}

void write_load_reference(Writer& writer, const LoadReference& entry) noexcept
{
    writer.length_text(entry.header_file_name, true);
    writer.length_text(entry.load_part_number_name, true);
}

LoadProgress read_load_progress(Reader& reader) noexcept
{
    LoadProgress entry;
    entry.header_file_name = reader.length_text(true);
    entry.load_part_number_name = reader.length_text(true);
    entry.ratio = reader.percent();
    entry.status = read_status(reader, StatusField::item);
    entry.description = reader.length_text(false);
    return entry;
}

void write_load_progress(Writer& writer, const LoadProgress& entry) noexcept
{
    writer.length_text(entry.header_file_name, true);
    writer.length_text(entry.load_part_number_name, true);
    writer.percent(entry.ratio);
    write_status(writer, StatusField::item, entry.status);
    writer.length_text(entry.description, false);
}

FileRequest read_file_request(Reader& reader) noexcept
{
    FileRequest entry;
    entry.file_name = reader.length_text(true);
    const std::size_t length = reader.u8();
    entry.user_defined_data = reader.bytes(length);
    return entry;
}

void write_file_request(Writer& writer, const FileRequest& entry) noexcept
{
    writer.length_text(entry.file_name, true);
    if (entry.user_defined_data.size() > max_user_data_bytes)
    {
        writer.fail(Error::field_too_large);
        return;
    }
    writer.u8(static_cast<std::uint8_t>(entry.user_defined_data.size()));
    writer.bytes(entry.user_defined_data);
}

FileProgress read_file_progress(Reader& reader) noexcept
{
    FileProgress entry;
    entry.file_name = reader.length_text(true);
    entry.status = read_status(reader, StatusField::item);
    entry.description = reader.length_text(false);
    return entry;
}

void write_file_progress(Writer& writer, const FileProgress& entry) noexcept
{
    writer.length_text(entry.file_name, true);
    write_status(writer, StatusField::item, entry.status);
    writer.length_text(entry.description, false);
}

FileListing read_file_listing(Reader& reader) noexcept
{
    FileListing entry;
    entry.file_name = reader.length_text(true);
    entry.description = reader.length_text(false);
    return entry;
}

void write_file_listing(Writer& writer, const FileListing& entry) noexcept
{
    writer.length_text(entry.file_name, true);
    writer.length_text(entry.description, false);
}

std::string_view read_file_name(Reader& reader) noexcept
{
    return reader.length_text(true);
}

HardwarePart read_hardware_part(Reader& reader) noexcept
{
    HardwarePart entry;
    entry.part_number = reader.length_text(true);
    entry.amendment = reader.length_text(false);
    entry.designation = reader.length_text(true);
    return entry;
}

void write_hardware_part(Writer& writer, const HardwarePart& entry) noexcept
{
    writer.length_text(entry.part_number, true);
    writer.length_text(entry.amendment, false);
    writer.length_text(entry.designation, true);
}

HardwareItem read_hardware_item(Reader& reader) noexcept
{
    HardwareItem entry;
    entry.literal_name = reader.length_text(true);
    entry.serial_number = reader.length_text(true);
    const std::size_t count = reader.u16();
    entry.parts = read_list<HardwarePart, read_hardware_part>(reader, count);
    return entry;
}

void write_hardware_item(Writer& writer, const HardwareItem& entry) noexcept
{
    writer.length_text(entry.literal_name, true);
    writer.length_text(entry.serial_number, true);
    writer.count(entry.parts.size());
    for (const HardwarePart part : entry.parts)
    {
        write_hardware_part(writer, part);
    }
}

// ---- The common run of a file: a count, then that many entries -------------------------

template <typename T, T (*Read)(Reader&) noexcept>
List<T> read_counted(Reader& reader) noexcept
{
    const std::size_t count = reader.u16();
    return read_list<T, Read>(reader, count);
}

template <typename T, typename Write>
void write_counted(Writer& writer, const List<T>& list, Write write) noexcept
{
    writer.count(list.size());
    for (const T entry : list)
    {
        write(writer, entry);
    }
}

} // namespace

// ---- Names -----------------------------------------------------------------------------

std::string_view extension(ProtocolFileKind kind) noexcept
{
    switch (kind)
    {
    case ProtocolFileKind::lci:
        return ".LCI";
    case ProtocolFileKind::lcl:
        return ".LCL";
    case ProtocolFileKind::lcs:
        return ".LCS";
    case ProtocolFileKind::lui:
        return ".LUI";
    case ProtocolFileKind::lur:
        return ".LUR";
    case ProtocolFileKind::lus:
        return ".LUS";
    case ProtocolFileKind::lnd:
        return ".LND";
    case ProtocolFileKind::lno:
        return ".LNO";
    case ProtocolFileKind::lnr:
        return ".LNR";
    case ProtocolFileKind::lns:
        return ".LNS";
    case ProtocolFileKind::lnl:
        return ".LNL";
    case ProtocolFileKind::lna:
        return ".LNA";
    }
    return {};
}

std::optional<ProtocolFileKind> kind_of_file_name(std::string_view file_name) noexcept
{
    constexpr std::array all = {ProtocolFileKind::lci, ProtocolFileKind::lcl, ProtocolFileKind::lcs,
                                ProtocolFileKind::lui, ProtocolFileKind::lur, ProtocolFileKind::lus,
                                ProtocolFileKind::lnd, ProtocolFileKind::lno, ProtocolFileKind::lnr,
                                ProtocolFileKind::lns, ProtocolFileKind::lnl, ProtocolFileKind::lna};
    for (const ProtocolFileKind candidate : all)
    {
        const std::string_view suffix = extension(candidate);
        // Something must come before the extension: the <THW_ID_POS>.
        if (file_name.size() > suffix.size() && file_name.ends_with(suffix))
        {
            return candidate;
        }
    }
    return std::nullopt;
}

Result<std::array<char, 2>> peek_protocol_version(std::span<const std::byte> file) noexcept
{
    Reader reader{file};
    static_cast<void>(reader.u32());
    const auto version = reader.bytes(2);
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    return std::array<char, 2>{static_cast<char>(version[0]), static_cast<char>(version[1])};
}

// ---- .LCI .LUI .LND .LNO ---------------------------------------------------------------

namespace
{

void write_initialization(Writer& writer, const InitializationFile& file) noexcept
{
    write_status(writer, StatusField::acceptance, file.status);
    writer.length_text(file.description, false);
}

} // namespace

Result<std::size_t> encode_initialization_file(const InitializationFile& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_initialization);
}

Result<std::size_t> encoded_size(const InitializationFile& file) noexcept
{
    return measure_file(file, write_initialization);
}

Result<InitializationFile> decode_initialization_file(std::span<const std::byte> bytes) noexcept
{
    return decode_file<InitializationFile>(bytes,
                                           [](Reader& reader, InitializationFile& file) noexcept
                                           {
                                               file.status = read_status(reader, StatusField::acceptance);
                                               file.description = reader.length_text(false);
                                           });
}

// ---- .LCL ------------------------------------------------------------------------------

namespace
{

void write_configuration_list(Writer& writer, const ConfigurationList& file) noexcept
{
    write_counted(writer, file.hardware, write_hardware_item);
}

} // namespace

Result<std::size_t> encode_configuration_list(const ConfigurationList& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_configuration_list);
}

Result<std::size_t> encoded_size(const ConfigurationList& file) noexcept
{
    return measure_file(file, write_configuration_list);
}

Result<ConfigurationList> decode_configuration_list(std::span<const std::byte> bytes) noexcept
{
    return decode_file<ConfigurationList>(bytes, [](Reader& reader, ConfigurationList& file) noexcept
                                          { file.hardware = read_counted<HardwareItem, read_hardware_item>(reader); });
}

// ---- .LCS ------------------------------------------------------------------------------

namespace
{

void write_configuration_status(Writer& writer, const ConfigurationStatus& file) noexcept
{
    writer.u16(file.counter);
    write_status(writer, StatusField::operation, file.status);
    writer.u16(file.exception_timer);
    writer.u16(file.estimated_time);
    writer.length_text(file.description, false);
}

} // namespace

Result<std::size_t> encode_configuration_status(const ConfigurationStatus& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_configuration_status);
}

Result<std::size_t> encoded_size(const ConfigurationStatus& file) noexcept
{
    return measure_file(file, write_configuration_status);
}

Result<ConfigurationStatus> decode_configuration_status(std::span<const std::byte> bytes) noexcept
{
    return decode_file<ConfigurationStatus>(bytes,
                                            [](Reader& reader, ConfigurationStatus& file) noexcept
                                            {
                                                file.counter = reader.u16();
                                                file.status = read_status(reader, StatusField::operation);
                                                file.exception_timer = reader.u16();
                                                file.estimated_time = reader.u16();
                                                file.description = reader.length_text(false);
                                            });
}

// ---- .LUR ------------------------------------------------------------------------------

namespace
{

void write_upload_request(Writer& writer, const UploadRequest& file) noexcept
{
    if (file.loads.empty())
    {
        writer.fail(Error::invalid_value); // "any .LUR file should contain at least one"
    }
    write_counted(writer, file.loads, write_load_reference);
}

} // namespace

Result<std::size_t> encode_upload_request(const UploadRequest& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_upload_request);
}

Result<std::size_t> encoded_size(const UploadRequest& file) noexcept
{
    return measure_file(file, write_upload_request);
}

Result<UploadRequest> decode_upload_request(std::span<const std::byte> bytes) noexcept
{
    return decode_file<UploadRequest>(bytes,
                                      [](Reader& reader, UploadRequest& file) noexcept
                                      {
                                          file.loads = read_counted<LoadReference, read_load_reference>(reader);
                                          if (reader.ok() && file.loads.empty())
                                          {
                                              reader.fail(Error::invalid_value);
                                          }
                                      });
}

// ---- .LUS ------------------------------------------------------------------------------

namespace
{

void write_upload_status(Writer& writer, const UploadStatus& file) noexcept
{
    write_status(writer, StatusField::operation, file.status);
    writer.length_text(file.description, false);
    writer.u16(file.counter);
    writer.u16(file.exception_timer);
    writer.u16(file.estimated_time);
    writer.percent(file.list_ratio);
    write_counted(writer, file.loads, write_load_progress);
}

} // namespace

Result<std::size_t> encode_upload_status(const UploadStatus& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_upload_status);
}

Result<std::size_t> encoded_size(const UploadStatus& file) noexcept
{
    return measure_file(file, write_upload_status);
}

Result<UploadStatus> decode_upload_status(std::span<const std::byte> bytes) noexcept
{
    return decode_file<UploadStatus>(bytes,
                                     [](Reader& reader, UploadStatus& file) noexcept
                                     {
                                         file.status = read_status(reader, StatusField::operation);
                                         file.description = reader.length_text(false);
                                         file.counter = reader.u16();
                                         file.exception_timer = reader.u16();
                                         file.estimated_time = reader.u16();
                                         file.list_ratio = reader.percent();
                                         file.loads = read_counted<LoadProgress, read_load_progress>(reader);
                                     });
}

// ---- .LNR ------------------------------------------------------------------------------

namespace
{

void write_download_request(Writer& writer, const DownloadRequest& file) noexcept
{
    write_counted(writer, file.files, write_file_request);
}

} // namespace

Result<std::size_t> encode_download_request(const DownloadRequest& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_download_request);
}

Result<std::size_t> encoded_size(const DownloadRequest& file) noexcept
{
    return measure_file(file, write_download_request);
}

Result<DownloadRequest> decode_download_request(std::span<const std::byte> bytes) noexcept
{
    return decode_file<DownloadRequest>(bytes, [](Reader& reader, DownloadRequest& file) noexcept
                                        { file.files = read_counted<FileRequest, read_file_request>(reader); });
}

// ---- .LNS ------------------------------------------------------------------------------

namespace
{

void write_download_status(Writer& writer, const DownloadStatus& file) noexcept
{
    write_status(writer, StatusField::operation, file.status);
    writer.length_text(file.description, false);
    writer.u16(file.counter);
    writer.u16(file.exception_timer);
    writer.u16(file.estimated_time);
    writer.percent(file.list_ratio);
    write_counted(writer, file.files, write_file_progress);
}

} // namespace

Result<std::size_t> encode_download_status(const DownloadStatus& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_download_status);
}

Result<std::size_t> encoded_size(const DownloadStatus& file) noexcept
{
    return measure_file(file, write_download_status);
}

Result<DownloadStatus> decode_download_status(std::span<const std::byte> bytes) noexcept
{
    return decode_file<DownloadStatus>(bytes,
                                       [](Reader& reader, DownloadStatus& file) noexcept
                                       {
                                           file.status = read_status(reader, StatusField::operation);
                                           file.description = reader.length_text(false);
                                           file.counter = reader.u16();
                                           file.exception_timer = reader.u16();
                                           file.estimated_time = reader.u16();
                                           file.list_ratio = reader.percent();
                                           file.files = read_counted<FileProgress, read_file_progress>(reader);
                                       });
}

// ---- .LNL ------------------------------------------------------------------------------

namespace
{

void write_download_list(Writer& writer, const DownloadList& file) noexcept
{
    write_counted(writer, file.files, write_file_listing);
}

} // namespace

Result<std::size_t> encode_download_list(const DownloadList& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_download_list);
}

Result<std::size_t> encoded_size(const DownloadList& file) noexcept
{
    return measure_file(file, write_download_list);
}

Result<DownloadList> decode_download_list(std::span<const std::byte> bytes) noexcept
{
    return decode_file<DownloadList>(bytes, [](Reader& reader, DownloadList& file) noexcept
                                     { file.files = read_counted<FileListing, read_file_listing>(reader); });
}

// ---- .LNA ------------------------------------------------------------------------------

namespace
{

void write_download_answer(Writer& writer, const DownloadAnswer& file) noexcept
{
    write_counted(writer, file.file_names,
                  [](Writer& w, std::string_view name) noexcept { w.length_text(name, true); });
}

} // namespace

Result<std::size_t> encode_download_answer(const DownloadAnswer& file, std::span<std::byte> out) noexcept
{
    return encode_file(file, out, write_download_answer);
}

Result<std::size_t> encoded_size(const DownloadAnswer& file) noexcept
{
    return measure_file(file, write_download_answer);
}

Result<DownloadAnswer> decode_download_answer(std::span<const std::byte> bytes) noexcept
{
    return decode_file<DownloadAnswer>(bytes, [](Reader& reader, DownloadAnswer& file) noexcept
                                       { file.file_names = read_counted<std::string_view, read_file_name>(reader); });
}

} // namespace adl::arinc615a
