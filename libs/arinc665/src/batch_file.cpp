#include "adl/arinc665/batch_file.hpp"

#include "adl/arinc665/naming.hpp"
#include "adl/arinc665/part_number.hpp"
#include "wire.hpp"

namespace adl::arinc665
{

namespace
{

// Table 2.3.1-1: the length (2 words), the format version, a spare word and two
// 32-bit pointers; the Batch File CRC is the last word.
constexpr std::size_t fixed_words = 2 + 1 + 1 + 2 * 2;
constexpr std::size_t trailer_words = 1;

void add(Findings& findings, std::string clause, std::string message)
{
    findings.push_back({std::move(clause), std::move(message)});
}

} // namespace

Result<std::vector<std::byte>> encode_batch_file(const BatchFile& batch)
{
    detail::WordWriter w;
    for (std::size_t i = 0; i < fixed_words; ++i)
    {
        w.u16(0);
    }

    const std::size_t pointer_to_part_number = w.words();
    w.string(batch.batch_part_number.view());
    w.string(batch.comment);

    const std::size_t pointer_to_targets = w.words();
    w.count(batch.targets.size());
    for (std::size_t i = 0; i < batch.targets.size(); ++i)
    {
        const BatchTarget& target = batch.targets[i];
        const std::size_t start = w.words();
        w.u16(0);
        w.string(target.target_hardware_id_position);
        w.count(target.loads.size());
        for (const BatchLoad& load : target.loads)
        {
            w.string(load.header_file_name);
            w.string(load.load_part_number.view());
        }
        // The pointer at the start of a block is the distance in words to the start of
        // the next one; 0x0000 in the last (section 2.3.1.12).
        if (i + 1 != batch.targets.size())
        {
            w.patch_u16(start, w.fit_u16(w.words() - start));
        }
    }

    const std::size_t crc_word = w.words();
    w.u16(0);

    w.patch_u32(0, w.fit_u32(w.words()));
    w.patch_u16(2, static_cast<std::uint16_t>(batch.format_version));
    w.patch_u16(3, 0); // spare
    w.patch_u32(4, w.fit_u32(pointer_to_part_number));
    w.patch_u32(6, w.fit_u32(pointer_to_targets));
    if (!w.ok())
    {
        return std::unexpected{w.status().error()};
    }
    w.patch_u16(crc_word, detail::crc16(w.view().first(crc_word * 2)));
    return w.take();
}

Result<BatchFile> decode_batch_file(std::span<const std::byte> bytes)
{
    if (bytes.size() % 2 != 0)
    {
        return std::unexpected{Error::bad_length};
    }
    const std::size_t total_words = bytes.size() / 2;
    if (total_words < fixed_words + trailer_words)
    {
        return std::unexpected{Error::truncated};
    }

    detail::WordReader fixed{bytes};
    const std::size_t length_words = fixed.u32();
    if (length_words > total_words)
    {
        return std::unexpected{Error::truncated};
    }
    if (length_words != total_words)
    {
        return std::unexpected{Error::bad_length};
    }
    const std::uint16_t version = fixed.u16();
    if ((version & 0xF000U) != 0x9000U || version < static_cast<std::uint16_t>(FileFormatVersion::batch))
    {
        return std::unexpected{Error::bad_version};
    }
    static_cast<void>(fixed.u16()); // spare
    const std::size_t pointer_to_part_number = fixed.u32();
    const std::size_t pointer_to_targets = fixed.u32();

    const std::size_t body_end = total_words - trailer_words;
    detail::WordReader trailer{bytes};
    trailer.seek(body_end);
    if (detail::crc16(bytes.first(body_end * 2)) != trailer.u16())
    {
        return std::unexpected{Error::bad_crc};
    }
    const auto in_body = [body_end](std::size_t pointer) { return pointer >= fixed_words && pointer < body_end; };
    if (!in_body(pointer_to_part_number) || !in_body(pointer_to_targets))
    {
        return std::unexpected{Error::bad_pointer};
    }

    BatchFile batch;
    batch.format_version = static_cast<FileFormatVersion>(version);
    detail::WordReader r{bytes.first(body_end * 2)};
    r.seek(pointer_to_part_number);
    batch.batch_part_number = PartNumber{r.string()};
    batch.comment = r.string();

    r.seek(pointer_to_targets);
    const std::size_t target_count = r.u16();
    for (std::size_t i = 0; i < target_count && r.ok(); ++i)
    {
        const std::size_t start = r.position();
        const std::uint16_t next = r.u16();
        BatchTarget target;
        target.target_hardware_id_position = r.string();
        const std::size_t load_count = r.u16();
        for (std::size_t j = 0; j < load_count && r.ok(); ++j)
        {
            BatchLoad load;
            load.header_file_name = r.string();
            load.load_part_number = PartNumber{r.string()};
            target.loads.push_back(std::move(load));
        }
        batch.targets.push_back(std::move(target));

        if (!r.ok())
        {
            break;
        }
        if (i + 1 == target_count)
        {
            if (next != 0)
            {
                r.fail(Error::bad_pointer);
            }
        }
        else if (next == 0 || start + next < r.position() || start + next >= body_end)
        {
            r.fail(Error::bad_pointer);
        }
        else
        {
            r.seek(start + next);
        }
    }
    if (!r.ok())
    {
        return std::unexpected{r.error()};
    }
    return batch;
}

Findings check_batch_file(const BatchFile& batch)
{
    Findings findings;
    if (batch.format_version != FileFormatVersion::batch)
    {
        add(findings, "1.4.1", "the batch file format version is not 0x9004");
    }
    for (Finding& finding : check_load_part_number(batch.batch_part_number.view()))
    {
        findings.push_back(std::move(finding));
    }
    for (const BatchTarget& target : batch.targets)
    {
        for (const BatchLoad& load : target.loads)
        {
            for (Finding& finding : check_header_file_name(load.header_file_name))
            {
                findings.push_back(std::move(finding));
            }
            for (Finding& finding : check_load_part_number(load.load_part_number.view()))
            {
                findings.push_back(std::move(finding));
            }
        }
    }
    return findings;
}

} // namespace adl::arinc665
