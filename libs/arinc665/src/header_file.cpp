#include "adl/arinc665/header_file.hpp"

#include <algorithm>
#include <limits>
#include <set>

#include "adl/arinc665/naming.hpp"
#include "wire.hpp"

namespace adl::arinc665 {

namespace {

// Figure C-1: the fixed part is the length (2 words), the format version, the
// part flags and eight 32-bit pointers; the Header File CRC and the 32-bit Load
// CRC are the last three words.
constexpr std::size_t fixed_words = 2 + 1 + 1 + 8 * 2;
constexpr std::size_t trailer_words = 1 + 2;

// The words of a file of `bytes` bytes: a half word at the end counts as a whole
// one (section 2.2.3.1.37).
std::uint64_t words_of(std::uint64_t bytes) {
    return bytes / 2 + bytes % 2;
}

void write_entry_end(detail::WordWriter& w, std::size_t entry_start, bool last) {
    // The pointer at the start of an entry is the distance in words to the start
    // of the next one, counting the pointer itself; 0x0000 in the last (sections
    // 2.2.3.1.32 and 2.2.3.1.45).
    if (!last) {
        w.patch_u16(entry_start, w.fit_u16(w.words() - entry_start));
    }
}

void write_data_files(detail::WordWriter& w, const std::vector<DataFile>& files) {
    w.count(files.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        const DataFile& file = files[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(file.file_name);
        w.string(file.part_number.view());
        w.u32(w.fit_u32(static_cast<std::size_t>(words_of(file.length))));
        w.u16(file.crc);
        w.u64(file.length);
        w.check_value(file.check_value);
        write_entry_end(w, start, i + 1 == files.size());
    }
}

void write_support_files(detail::WordWriter& w, const std::vector<SupportFile>& files) {
    w.count(files.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        const SupportFile& file = files[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(file.file_name);
        w.string(file.part_number.view());
        w.u32(w.fit_u32(static_cast<std::size_t>(file.length)));
        w.u16(file.crc);
        w.check_value(file.check_value);
        write_entry_end(w, start, i + 1 == files.size());
    }
}

// After an entry: where the next one starts. A list's last entry has no next
// (pointer 0x0000); any other must point forward, past the fields just read and
// inside the part of the file that holds them.
void follow_entry_pointer(detail::WordReader& r, std::size_t start, std::uint16_t next, bool last,
                          std::size_t body_end) {
    if (!r.ok()) {
        return;
    }
    if (last) {
        if (next != 0) {
            r.fail(Error::bad_pointer);
        }
        return;
    }
    if (next == 0 || start + next < r.position() || start + next >= body_end) {
        r.fail(Error::bad_pointer);
        return;
    }
    r.seek(start + next);
}

void read_data_files(detail::WordReader& r, std::vector<DataFile>& files, std::size_t body_end) {
    const std::size_t count = r.u16();
    if (r.ok() && count == 0) {
        r.fail(Error::invalid_value);  // section 2.2.3.1.31: at least one data file
    }
    for (std::size_t i = 0; i < count && r.ok(); ++i) {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        DataFile file;
        file.file_name = r.string();
        file.part_number = PartNumber{r.string()};
        const std::uint64_t length_words = r.u32();
        file.crc = r.u16();
        file.length = r.u64();
        file.check_value = r.check_value();
        if (r.ok() && length_words != words_of(file.length)) {
            r.fail(Error::bad_length);  // the length in words and in bytes must agree
        }
        follow_entry_pointer(r, start, next, i + 1 == count, body_end);
        files.push_back(std::move(file));
    }
}

void read_support_files(detail::WordReader& r, std::vector<SupportFile>& files,
                        std::size_t body_end) {
    const std::size_t count = r.u16();
    for (std::size_t i = 0; i < count && r.ok(); ++i) {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        SupportFile file;
        file.file_name = r.string();
        file.part_number = PartNumber{r.string()};
        file.length = r.u32();
        file.crc = r.u16();
        file.check_value = r.check_value();
        follow_entry_pointer(r, start, next, i + 1 == count, body_end);
        files.push_back(std::move(file));
    }
}

void add(Findings& findings, std::string clause, std::string message) {
    findings.push_back({std::move(clause), std::move(message)});
}

void append(Findings& findings, Findings more) {
    findings.insert(findings.end(), std::make_move_iterator(more.begin()),
                    std::make_move_iterator(more.end()));
}

}  // namespace

Result<std::vector<std::byte>> encode_header_file(const HeaderFile& header) {
    if (header.data_files.empty() || header.user_defined_data.size() % 2 != 0) {
        return std::unexpected{Error::invalid_value};
    }

    detail::WordWriter w;
    for (std::size_t i = 0; i < fixed_words; ++i) {
        w.u16(0);  // filled in below, once the positions are known
    }

    const std::size_t pointer_to_load_pn = w.words();
    w.string(header.load_part_number.view());

    std::size_t pointer_to_load_type = 0;
    if (header.load_type) {
        pointer_to_load_type = w.words();
        w.string(header.load_type->description);
        w.u16(header.load_type->id);
    }

    const std::size_t pointer_to_target_hardware = w.words();
    w.count(header.target_hardware_ids.size());
    for (const std::string& id : header.target_hardware_ids) {
        w.string(id);
    }

    std::size_t pointer_to_positions = 0;
    if (!header.target_hardware_ids_with_positions.empty()) {
        pointer_to_positions = w.words();
        w.count(header.target_hardware_ids_with_positions.size());
        for (const TargetHardware& hardware : header.target_hardware_ids_with_positions) {
            w.string(hardware.id);
            w.count(hardware.positions.size());
            for (const std::string& position : hardware.positions) {
                w.string(position);
            }
        }
    }

    const std::size_t pointer_to_data_files = w.words();
    write_data_files(w, header.data_files);

    std::size_t pointer_to_support_files = 0;
    if (!header.support_files.empty()) {
        pointer_to_support_files = w.words();
        write_support_files(w, header.support_files);
    }

    std::size_t pointer_to_user_data = 0;
    if (!header.user_defined_data.empty()) {
        pointer_to_user_data = w.words();
        w.bytes(header.user_defined_data);
    }

    const std::size_t pointer_to_check_value = w.words();
    w.check_value(header.load_check_value);

    const std::size_t header_crc_word = w.words();
    w.u16(0);
    w.u32(header.load_crc);

    w.patch_u32(0, w.fit_u32(w.words()));
    w.patch_u16(2, static_cast<std::uint16_t>(header.format_version));
    w.patch_u16(3, header.part_flags.bits());
    w.patch_u32(4, w.fit_u32(pointer_to_load_pn));
    w.patch_u32(6, w.fit_u32(pointer_to_target_hardware));
    w.patch_u32(8, w.fit_u32(pointer_to_data_files));
    w.patch_u32(10, w.fit_u32(pointer_to_support_files));
    w.patch_u32(12, w.fit_u32(pointer_to_user_data));
    w.patch_u32(14, w.fit_u32(pointer_to_load_type));
    w.patch_u32(16, w.fit_u32(pointer_to_positions));
    w.patch_u32(18, w.fit_u32(pointer_to_check_value));
    if (!w.ok()) {
        return std::unexpected{w.status().error()};
    }

    // The Header File CRC covers everything before itself and the Load CRC.
    const std::uint16_t crc = detail::crc16(w.view().first(header_crc_word * 2));
    w.patch_u16(header_crc_word, crc);
    return w.take();
}

Result<HeaderFile> decode_header_file(std::span<const std::byte> bytes) {
    if (bytes.size() % 2 != 0) {
        return std::unexpected{Error::bad_length};
    }
    const std::size_t total_words = bytes.size() / 2;
    if (total_words < fixed_words + trailer_words) {
        return std::unexpected{Error::truncated};
    }

    detail::WordReader fixed{bytes};
    const std::size_t length_words = fixed.u32();
    if (length_words > total_words) {
        return std::unexpected{Error::truncated};
    }
    if (length_words != total_words) {
        return std::unexpected{Error::bad_length};
    }

    // Section 1.4.1: the load class is 0x8, and version 4 is where the Load Check
    // Value fields sit before the CRCs. A later version of the class is read as
    // this one (section 1.4.3).
    const std::uint16_t version = fixed.u16();
    if ((version & 0xF000U) != 0x8000U ||
        version < static_cast<std::uint16_t>(FileFormatVersion::load)) {
        return std::unexpected{Error::bad_version};
    }
    const std::uint16_t part_flags = fixed.u16();
    const std::size_t pointer_to_load_pn = fixed.u32();
    const std::size_t pointer_to_target_hardware = fixed.u32();
    const std::size_t pointer_to_data_files = fixed.u32();
    const std::size_t pointer_to_support_files = fixed.u32();
    const std::size_t pointer_to_user_data = fixed.u32();
    const std::size_t pointer_to_load_type = fixed.u32();
    const std::size_t pointer_to_positions = fixed.u32();
    const std::size_t pointer_to_check_value = fixed.u32();

    // The CRCs are the last three words; the Header File CRC covers the rest.
    const std::size_t body_end = total_words - trailer_words;
    detail::WordReader trailer{bytes};
    trailer.seek(body_end);
    const std::uint16_t header_crc = trailer.u16();
    const std::uint32_t load_crc = trailer.u32();
    if (detail::crc16(bytes.first(body_end * 2)) != header_crc) {
        return std::unexpected{Error::bad_crc};
    }

    // A pointer is the word the field starts at, so it lies in the body, after the
    // fixed part. Four of them are always there; the others are 0x0000 when their
    // field is left out.
    const auto in_body = [body_end](std::size_t pointer) {
        return pointer >= fixed_words && pointer < body_end;
    };
    const auto in_body_or_absent = [&](std::size_t pointer) {
        return pointer == 0 || in_body(pointer);
    };
    if (!in_body(pointer_to_load_pn) || !in_body(pointer_to_target_hardware) ||
        !in_body(pointer_to_data_files) || !in_body(pointer_to_check_value) ||
        !in_body_or_absent(pointer_to_support_files) || !in_body_or_absent(pointer_to_user_data) ||
        !in_body_or_absent(pointer_to_load_type) || !in_body_or_absent(pointer_to_positions)) {
        return std::unexpected{Error::bad_pointer};
    }

    HeaderFile header;
    header.format_version = static_cast<FileFormatVersion>(version);
    header.part_flags.is_download = (part_flags & 1U) != 0;
    header.load_crc = load_crc;

    detail::WordReader r{bytes.first(body_end * 2)};

    r.seek(pointer_to_load_pn);
    header.load_part_number = PartNumber{r.string()};

    if (pointer_to_load_type != 0) {
        r.seek(pointer_to_load_type);
        LoadType type;
        type.description = r.string();
        type.id = r.u16();
        header.load_type = std::move(type);
    }

    r.seek(pointer_to_target_hardware);
    const std::size_t target_count = r.u16();
    for (std::size_t i = 0; i < target_count && r.ok(); ++i) {
        header.target_hardware_ids.push_back(r.string());
    }

    if (pointer_to_positions != 0) {
        r.seek(pointer_to_positions);
        const std::size_t hardware_count = r.u16();
        for (std::size_t i = 0; i < hardware_count && r.ok(); ++i) {
            TargetHardware hardware;
            hardware.id = r.string();
            const std::size_t position_count = r.u16();
            for (std::size_t j = 0; j < position_count && r.ok(); ++j) {
                hardware.positions.push_back(r.string());
            }
            header.target_hardware_ids_with_positions.push_back(std::move(hardware));
        }
    }

    r.seek(pointer_to_data_files);
    read_data_files(r, header.data_files, body_end);

    if (pointer_to_support_files != 0) {
        r.seek(pointer_to_support_files);
        read_support_files(r, header.support_files, body_end);
    }

    if (pointer_to_user_data != 0) {
        // The user defined data runs up to the Load Check Value.
        if (pointer_to_user_data > pointer_to_check_value) {
            return std::unexpected{Error::bad_pointer};
        }
        r.seek(pointer_to_user_data);
        const auto data = r.bytes((pointer_to_check_value - pointer_to_user_data) * 2);
        header.user_defined_data.assign(data.begin(), data.end());
    }

    r.seek(pointer_to_check_value);
    header.load_check_value = r.check_value();
    if (r.ok() && r.position() != body_end) {
        r.fail(Error::bad_pointer);  // nothing may sit between the check value and the CRCs
    }

    if (!r.ok()) {
        return std::unexpected{r.error()};
    }
    return header;
}

Findings check_header_file(const HeaderFile& header) {
    Findings findings;

    if (header.format_version != FileFormatVersion::load) {
        add(findings, "1.4.1", "the load file format version is not 0x8004");
    }
    append(findings, check_load_part_number(header.load_part_number.view()));

    if (header.target_hardware_ids.empty()) {
        add(findings, "2.2.3.1.20", "the load lists no target hardware ID");
    }
    for (const std::string& id : header.target_hardware_ids) {
        append(findings, check_target_hardware_id(id));
    }
    for (const TargetHardware& hardware : header.target_hardware_ids_with_positions) {
        if (std::find(header.target_hardware_ids.begin(), header.target_hardware_ids.end(),
                      hardware.id) == header.target_hardware_ids.end()) {
            add(findings, "2.2.3.1.26",
                "target hardware ID " + hardware.id +
                    " with positions is not in the target hardware ID list");
        }
    }

    std::set<std::string> names;
    std::set<std::string> part_numbers;
    // Section 2.2.3.1.49 asks for a support file's part number to follow the
    // format of section 2.1.1; a data file's (2.2.3.1.36) only has to be unique.
    const auto check_listed = [&](const UserSuppliedFile& file, const char* kind,
                                  const char* clause, bool check_format) {
        if (!names.insert(file.file_name).second) {
            add(findings, "2.2.1",
                "the file name " + file.file_name + " is used more than once in the load");
        }
        if (file.part_number.empty()) {
            return;
        }
        if (check_format) {
            append(findings, check_load_part_number(file.part_number.view()));
        }
        if (!part_numbers.insert(std::string{file.part_number.view()}).second) {
            add(findings, clause,
                std::string{"the "} + kind + " part number " +
                    std::string{file.part_number.view()} + " is used more than once in the load");
        }
    };
    for (const DataFile& file : header.data_files) {
        append(findings, check_data_file_name(file.file_name));
        check_listed(file, "data file", "2.2.3.1.36", false);
        if (file.length % 2 != 0) {
            add(findings, "2.2.3.2",
                "the data file " + file.file_name + " is not a whole number of 16-bit words");
        }
    }
    for (const SupportFile& file : header.support_files) {
        append(findings, check_support_file_name(file.file_name));
        check_listed(file, "support file", "2.2.3.1.49", true);
    }
    return findings;
}

}  // namespace adl::arinc665
