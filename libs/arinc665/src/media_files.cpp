#include "adl/arinc665/media_files.hpp"

#include "adl/arinc665/naming.hpp"
#include "wire.hpp"

#include <algorithm>
#include <set>

namespace adl::arinc665
{

namespace
{

// The media class of the file format version (section 1.4.1).
constexpr std::uint16_t media_class = 0xA000;

// The length (2 words), the format version, a spare word and then the 32-bit
// pointers: three for LOADS.LUM and BATCHES.LUM (the media set, the list, the
// user defined data), four for FILES.LUM (and the FILES.LUM check value).
constexpr std::size_t loads_fixed_words = 2 + 1 + 1 + 3 * 2;
constexpr std::size_t batches_fixed_words = 2 + 1 + 1 + 3 * 2;
constexpr std::size_t files_fixed_words = 2 + 1 + 1 + 4 * 2;
constexpr std::size_t crc_words = 1;

void add(Findings& findings, std::string clause, std::string message)
{
    findings.push_back({std::move(clause), std::move(message)});
}

void append(Findings& findings, Findings more)
{
    findings.insert(findings.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
}

bool identity_is_valid(const MediaSetIdentity& identity)
{
    return identity.media_sequence_number >= 1 &&
           identity.media_sequence_number <= identity.number_of_media_set_members;
}

// The start of a list file: placeholders for the length, version, spare word and
// the pointers, patched once the positions are known.
void begin_file(detail::WordWriter& w, std::size_t fixed_words)
{
    for (std::size_t i = 0; i < fixed_words; ++i)
    {
        w.u16(0);
    }
}

void write_identity(detail::WordWriter& w, const MediaSetIdentity& identity)
{
    w.string(identity.media_set_part_number.view());
    w.u8_pair(identity.media_sequence_number, identity.number_of_media_set_members);
}

// The distance in words from the start of an entry to the start of the next, in
// the pointer that opens it; 0x0000 in the last (sections 3.2.3.1.13, 3.2.3.2.14
// and 3.2.3.3.13).
void end_entry(detail::WordWriter& w, std::size_t entry_start, bool last)
{
    if (!last)
    {
        w.patch_u16(entry_start, w.fit_u16(w.words() - entry_start));
    }
}

// Ends the file: the CRC over everything before it goes in the last word.
Result<std::vector<std::byte>> finish_file(detail::WordWriter& w, std::uint16_t format_version)
{
    const std::size_t crc_word = w.words();
    w.u16(0);
    w.patch_u32(0, w.fit_u32(w.words()));
    w.patch_u16(2, format_version);
    if (!w.ok())
    {
        return std::unexpected{w.status().error()};
    }
    w.patch_u16(crc_word, detail::crc16(w.view().first(crc_word * 2)));
    return w.take();
}

// Checks what every list file starts with: whole words, a length that is the
// file's, a media format version, and a CRC that is right.
Result<std::uint16_t> check_framing(std::span<const std::byte> bytes, std::size_t fixed_words)
{
    if (bytes.size() % 2 != 0)
    {
        return std::unexpected{Error::bad_length};
    }
    const std::size_t total_words = bytes.size() / 2;
    if (total_words < fixed_words + crc_words)
    {
        return std::unexpected{Error::truncated};
    }
    detail::WordReader r{bytes};
    const std::size_t length_words = r.u32();
    if (length_words > total_words)
    {
        return std::unexpected{Error::truncated};
    }
    if (length_words != total_words)
    {
        return std::unexpected{Error::bad_length};
    }
    const std::uint16_t version = r.u16();
    if ((version & 0xF000U) != media_class || version < static_cast<std::uint16_t>(FileFormatVersion::media))
    {
        return std::unexpected{Error::bad_version};
    }
    detail::WordReader tail{bytes};
    tail.seek(total_words - crc_words);
    if (detail::crc16(bytes.first((total_words - crc_words) * 2)) != tail.u16())
    {
        return std::unexpected{Error::bad_crc};
    }
    return version;
}

MediaSetIdentity read_identity(detail::WordReader& r)
{
    MediaSetIdentity identity;
    identity.media_set_part_number = PartNumber{r.string()};
    const auto [x, y] = r.u8_pair();
    identity.media_sequence_number = x;
    identity.number_of_media_set_members = y;
    if (r.ok() && !identity_is_valid(identity))
    {
        r.fail(Error::invalid_value);
    }
    return identity;
}

// After an entry in a list: on to the next one by its pointer, or, for the last
// entry, a pointer of zero.
void follow_entry(detail::WordReader& r, std::size_t start, std::uint16_t next, bool last, std::size_t body_end)
{
    if (!r.ok())
    {
        return;
    }
    if (last)
    {
        if (next != 0)
        {
            r.fail(Error::bad_pointer);
        }
        return;
    }
    if (next == 0 || start + next < r.position() || start + next >= body_end)
    {
        r.fail(Error::bad_pointer);
        return;
    }
    r.seek(start + next);
}

// The user defined data between `pointer` and `end`, in words; none when the
// pointer is 0x0000.
std::vector<std::byte> read_user_data(detail::WordReader& r, std::size_t pointer, std::size_t end)
{
    if (pointer == 0)
    {
        return {};
    }
    if (pointer > end)
    {
        r.fail(Error::bad_pointer);
        return {};
    }
    r.seek(pointer);
    const auto data = r.bytes((end - pointer) * 2);
    return {data.begin(), data.end()};
}

Findings check_identity(const MediaSetIdentity& identity)
{
    Findings findings = check_media_set_part_number(identity.media_set_part_number.view());
    if (!identity_is_valid(identity))
    {
        add(findings, "3.2.3.1.10", "the media sequence number is not between 1 and the number of members");
    }
    return findings;
}

} // namespace

// ---- LOADS.LUM ----

Result<std::vector<std::byte>> encode_loads_list(const LoadsList& list)
{
    if (!identity_is_valid(list.identity) || list.user_defined_data.size() % 2 != 0)
    {
        return std::unexpected{Error::invalid_value};
    }
    detail::WordWriter w;
    begin_file(w, loads_fixed_words);

    const std::size_t pointer_to_media_set = w.words();
    write_identity(w, list.identity);

    const std::size_t pointer_to_loads = w.words();
    w.count(list.loads.size());
    for (std::size_t i = 0; i < list.loads.size(); ++i)
    {
        const LoadsListEntry& load = list.loads[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(load.load_part_number.view());
        w.string(load.header_file_name);
        w.u16(load.member_sequence_number);
        w.count(load.target_hardware_ids.size());
        for (const std::string& id : load.target_hardware_ids)
        {
            w.string(id);
        }
        end_entry(w, start, i + 1 == list.loads.size());
    }

    std::size_t pointer_to_user_data = 0;
    if (!list.user_defined_data.empty())
    {
        pointer_to_user_data = w.words();
        w.bytes(list.user_defined_data);
    }

    w.patch_u32(4, w.fit_u32(pointer_to_media_set));
    w.patch_u32(6, w.fit_u32(pointer_to_loads));
    w.patch_u32(8, w.fit_u32(pointer_to_user_data));
    return finish_file(w, static_cast<std::uint16_t>(list.format_version));
}

Result<LoadsList> decode_loads_list(std::span<const std::byte> bytes)
{
    const auto version = check_framing(bytes, loads_fixed_words);
    if (!version)
    {
        return std::unexpected{version.error()};
    }
    const std::size_t body_end = bytes.size() / 2 - crc_words;

    detail::WordReader fixed{bytes};
    fixed.seek(2);
    static_cast<void>(fixed.u16()); // the format version
    static_cast<void>(fixed.u16()); // spare
    const std::size_t pointer_to_media_set = fixed.u32();
    const std::size_t pointer_to_loads = fixed.u32();
    const std::size_t pointer_to_user_data = fixed.u32();
    const auto in_body = [body_end](std::size_t pointer) { return pointer >= loads_fixed_words && pointer < body_end; };
    if (!in_body(pointer_to_media_set) || !in_body(pointer_to_loads) ||
        (pointer_to_user_data != 0 && !in_body(pointer_to_user_data)))
    {
        return std::unexpected{Error::bad_pointer};
    }

    LoadsList list;
    list.format_version = static_cast<FileFormatVersion>(*version);
    detail::WordReader r{bytes.first(body_end * 2)};
    r.seek(pointer_to_media_set);
    list.identity = read_identity(r);

    r.seek(pointer_to_loads);
    const std::size_t count = r.u16();
    for (std::size_t i = 0; i < count && r.ok(); ++i)
    {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        LoadsListEntry load;
        load.load_part_number = PartNumber{r.string()};
        load.header_file_name = r.string();
        load.member_sequence_number = r.u16();
        const std::size_t id_count = r.u16();
        for (std::size_t j = 0; j < id_count && r.ok(); ++j)
        {
            load.target_hardware_ids.push_back(r.string());
        }
        list.loads.push_back(std::move(load));
        follow_entry(r, start, next, i + 1 == count, body_end);
    }

    list.user_defined_data = read_user_data(r, pointer_to_user_data, body_end);
    if (!r.ok())
    {
        return std::unexpected{r.error()};
    }
    return list;
}

Findings check_loads_list(const LoadsList& list)
{
    Findings findings = check_identity(list.identity);
    if (list.format_version != FileFormatVersion::media)
    {
        add(findings, "1.4.1", "the media file format version is not 0xA004");
    }
    std::set<std::string> part_numbers;
    for (const LoadsListEntry& load : list.loads)
    {
        append(findings, check_load_part_number(load.load_part_number.view()));
        append(findings, check_header_file_name(load.header_file_name));
        for (const std::string& id : load.target_hardware_ids)
        {
            append(findings, check_target_hardware_id(id));
        }
        if (load.member_sequence_number < 1 || load.member_sequence_number > list.identity.number_of_media_set_members)
        {
            add(findings, "3.2.3.1.18",
                "the load " + std::string{load.load_part_number.view()} + " is on a member the set does not have");
        }
        if (!part_numbers.insert(std::string{load.load_part_number.view()}).second)
        {
            add(findings, "3.2.3.1.12",
                "the load " + std::string{load.load_part_number.view()} + " is listed more than once");
        }
    }
    return findings;
}

// ---- FILES.LUM ----

Result<std::vector<std::byte>> encode_files_list(const FilesList& list)
{
    if (!identity_is_valid(list.identity) || list.user_defined_data.size() % 2 != 0)
    {
        return std::unexpected{Error::invalid_value};
    }
    detail::WordWriter w;
    begin_file(w, files_fixed_words);

    const std::size_t pointer_to_media_set = w.words();
    write_identity(w, list.identity);

    const std::size_t pointer_to_files = w.words();
    w.count(list.files.size());
    for (std::size_t i = 0; i < list.files.size(); ++i)
    {
        const FilesListEntry& file = list.files[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(file.file_name);
        w.string(file.pathname);
        w.u16(file.member_sequence_number);
        w.u16(file.crc);
        w.check_value(file.check_value);
        end_entry(w, start, i + 1 == list.files.size());
    }

    std::size_t pointer_to_user_data = 0;
    if (!list.user_defined_data.empty())
    {
        pointer_to_user_data = w.words();
        w.bytes(list.user_defined_data);
    }

    const std::size_t pointer_to_check_value = w.words();
    w.check_value(list.check_value);

    w.patch_u32(4, w.fit_u32(pointer_to_media_set));
    w.patch_u32(6, w.fit_u32(pointer_to_files));
    w.patch_u32(8, w.fit_u32(pointer_to_user_data));
    w.patch_u32(10, w.fit_u32(pointer_to_check_value));
    return finish_file(w, static_cast<std::uint16_t>(list.format_version));
}

Result<FilesList> decode_files_list(std::span<const std::byte> bytes)
{
    const auto version = check_framing(bytes, files_fixed_words);
    if (!version)
    {
        return std::unexpected{version.error()};
    }
    const std::size_t body_end = bytes.size() / 2 - crc_words;

    detail::WordReader fixed{bytes};
    fixed.seek(2);
    static_cast<void>(fixed.u16()); // the format version
    static_cast<void>(fixed.u16()); // spare
    const std::size_t pointer_to_media_set = fixed.u32();
    const std::size_t pointer_to_files = fixed.u32();
    const std::size_t pointer_to_user_data = fixed.u32();
    const std::size_t pointer_to_check_value = fixed.u32();
    const auto in_body = [body_end](std::size_t pointer) { return pointer >= files_fixed_words && pointer < body_end; };
    if (!in_body(pointer_to_media_set) || !in_body(pointer_to_files) || !in_body(pointer_to_check_value) ||
        (pointer_to_user_data != 0 && !in_body(pointer_to_user_data)))
    {
        return std::unexpected{Error::bad_pointer};
    }

    FilesList list;
    list.format_version = static_cast<FileFormatVersion>(*version);
    detail::WordReader r{bytes.first(body_end * 2)};
    r.seek(pointer_to_media_set);
    list.identity = read_identity(r);

    r.seek(pointer_to_files);
    const std::size_t count = r.u16();
    for (std::size_t i = 0; i < count && r.ok(); ++i)
    {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        FilesListEntry file;
        file.file_name = r.string();
        file.pathname = r.string();
        file.member_sequence_number = r.u16();
        file.crc = r.u16();
        file.check_value = r.check_value();
        list.files.push_back(std::move(file));
        follow_entry(r, start, next, i + 1 == count, body_end);
    }

    list.user_defined_data = read_user_data(r, pointer_to_user_data, pointer_to_check_value);

    r.seek(pointer_to_check_value);
    list.check_value = r.check_value();
    if (r.ok() && r.position() != body_end)
    {
        r.fail(Error::bad_pointer); // nothing may sit between the check value and the CRC
    }
    if (!r.ok())
    {
        return std::unexpected{r.error()};
    }
    return list;
}

Result<std::size_t> files_list_check_value_offset(std::span<const std::byte> bytes)
{
    if (bytes.size() < 2 * (files_fixed_words + crc_words))
    {
        return std::unexpected{Error::truncated};
    }
    detail::WordReader r{bytes};
    r.seek(10); // the pointer to the check value is the fourth, in words 10 and 11
    const std::size_t pointer = r.u32();
    if (!r.ok() || pointer < files_fixed_words || pointer >= bytes.size() / 2 - crc_words)
    {
        return std::unexpected{Error::bad_pointer};
    }
    return pointer * 2;
}

Findings check_files_list(const FilesList& list)
{
    Findings findings = check_identity(list.identity);
    if (list.format_version != FileFormatVersion::media)
    {
        add(findings, "1.4.1", "the media file format version is not 0xA004");
    }
    for (const FilesListEntry& file : list.files)
    {
        append(findings, check_file_name(file.file_name));
        append(findings, check_pathname(file.pathname));
        if (file.member_sequence_number < 1 || file.member_sequence_number > list.identity.number_of_media_set_members)
        {
            add(findings, "3.2.3.2.19", "the file " + file.file_name + " is on a member the set does not have");
        }
        if (file.file_name == files_list_file_name)
        {
            add(findings, "3.2.3.2.13", "FILES.LUM lists itself");
        }
    }
    return findings;
}

// ---- BATCHES.LUM ----

Result<std::vector<std::byte>> encode_batches_list(const BatchesList& list)
{
    if (!identity_is_valid(list.identity) || list.user_defined_data.size() % 2 != 0)
    {
        return std::unexpected{Error::invalid_value};
    }
    detail::WordWriter w;
    begin_file(w, batches_fixed_words);

    const std::size_t pointer_to_media_set = w.words();
    write_identity(w, list.identity);

    const std::size_t pointer_to_batches = w.words();
    w.count(list.batches.size());
    for (std::size_t i = 0; i < list.batches.size(); ++i)
    {
        const BatchesListEntry& batch = list.batches[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(batch.batch_part_number.view());
        w.string(batch.batch_file_name);
        w.u16(batch.member_sequence_number);
        end_entry(w, start, i + 1 == list.batches.size());
    }

    std::size_t pointer_to_user_data = 0;
    if (!list.user_defined_data.empty())
    {
        pointer_to_user_data = w.words();
        w.bytes(list.user_defined_data);
    }

    w.patch_u32(4, w.fit_u32(pointer_to_media_set));
    w.patch_u32(6, w.fit_u32(pointer_to_batches));
    w.patch_u32(8, w.fit_u32(pointer_to_user_data));
    return finish_file(w, static_cast<std::uint16_t>(list.format_version));
}

Result<BatchesList> decode_batches_list(std::span<const std::byte> bytes)
{
    const auto version = check_framing(bytes, batches_fixed_words);
    if (!version)
    {
        return std::unexpected{version.error()};
    }
    const std::size_t body_end = bytes.size() / 2 - crc_words;

    detail::WordReader fixed{bytes};
    fixed.seek(2);
    static_cast<void>(fixed.u16()); // the format version
    static_cast<void>(fixed.u16()); // spare
    const std::size_t pointer_to_media_set = fixed.u32();
    const std::size_t pointer_to_batches = fixed.u32();
    const std::size_t pointer_to_user_data = fixed.u32();
    const auto in_body = [body_end](std::size_t pointer)
    { return pointer >= batches_fixed_words && pointer < body_end; };
    if (!in_body(pointer_to_media_set) || !in_body(pointer_to_batches) ||
        (pointer_to_user_data != 0 && !in_body(pointer_to_user_data)))
    {
        return std::unexpected{Error::bad_pointer};
    }

    BatchesList list;
    list.format_version = static_cast<FileFormatVersion>(*version);
    detail::WordReader r{bytes.first(body_end * 2)};
    r.seek(pointer_to_media_set);
    list.identity = read_identity(r);

    r.seek(pointer_to_batches);
    const std::size_t count = r.u16();
    for (std::size_t i = 0; i < count && r.ok(); ++i)
    {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        BatchesListEntry batch;
        batch.batch_part_number = PartNumber{r.string()};
        batch.batch_file_name = r.string();
        batch.member_sequence_number = r.u16();
        list.batches.push_back(std::move(batch));
        follow_entry(r, start, next, i + 1 == count, body_end);
    }

    list.user_defined_data = read_user_data(r, pointer_to_user_data, body_end);
    if (!r.ok())
    {
        return std::unexpected{r.error()};
    }
    return list;
}

Findings check_batches_list(const BatchesList& list)
{
    Findings findings = check_identity(list.identity);
    if (list.format_version != FileFormatVersion::media)
    {
        add(findings, "1.4.1", "the media file format version is not 0xA004");
    }
    for (const BatchesListEntry& batch : list.batches)
    {
        append(findings, check_load_part_number(batch.batch_part_number.view()));
        append(findings, check_batch_file_name(batch.batch_file_name));
        if (batch.member_sequence_number < 1 ||
            batch.member_sequence_number > list.identity.number_of_media_set_members)
        {
            add(findings, "3.2.3.3.18",
                "the batch " + std::string{batch.batch_part_number.view()} + " is on a member the set does not have");
        }
    }
    return findings;
}

} // namespace adl::arinc665
