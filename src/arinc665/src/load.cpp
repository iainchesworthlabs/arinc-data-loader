#include "adl/arinc665/load.hpp"

#include <algorithm>

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/integrity_checker.hpp"
#include "adl/arinc665/naming.hpp"
#include "wire.hpp"

namespace adl::arinc665 {

namespace {

// Figure C-1: the pointer to the Load Check Value Length is the last of the
// eight pointers, in words 18 and 19.
constexpr std::size_t check_value_pointer_offset = std::size_t{18} * 2;

std::size_t check_value_offset(std::span<const std::byte> header_bytes) {
    std::size_t pointer = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        pointer = (pointer << 8) |
                  std::to_integer<std::size_t>(header_bytes[check_value_pointer_offset + i]);
    }
    return pointer * 2;
}

// The files after the header, in the order the Load Check Value and the Load CRC
// take them: data files, then support files.
template <typename Update>
void for_each_file_bytes(const std::vector<FileContent>& data_files,
                         const std::vector<FileContent>& support_files, Update update) {
    for (const FileContent& file : data_files) {
        update(std::span<const std::byte>{file.bytes});
    }
    for (const FileContent& file : support_files) {
        update(std::span<const std::byte>{file.bytes});
    }
}

// The Load Check Value of a load whose header is `header_bytes`: the header up to
// the check value fields, then the files.
Result<CheckValue> load_check_value(CheckValueType type, std::span<const std::byte> header_bytes,
                                    const std::vector<FileContent>& data_files,
                                    const std::vector<FileContent>& support_files) {
    auto checker = IntegrityChecker::make(type);
    if (!checker) {
        return std::unexpected{checker.error()};
    }
    checker->update(header_bytes.first(check_value_offset(header_bytes)));
    for_each_file_bytes(data_files, support_files,
                        [&checker](std::span<const std::byte> bytes) { checker->update(bytes); });
    return checker->check_value();
}

// The Load CRC of a load whose header is `header_bytes`: the header but for its
// last two words, then the files.
std::uint32_t load_crc(std::span<const std::byte> header_bytes,
                       const std::vector<FileContent>& data_files,
                       const std::vector<FileContent>& support_files) {
    Crc32 crc;
    crc.update(header_bytes.first(header_bytes.size() - 4));
    for_each_file_bytes(data_files, support_files,
                        [&crc](std::span<const std::byte> bytes) { crc.update(bytes); });
    return crc.value();
}

template <typename Listed>
Result<void> add_file(const LoadFileSpec& spec, std::vector<Listed>& listed,
                      std::vector<FileContent>& contents) {
    Listed entry;
    entry.file_name = spec.name;
    entry.part_number = spec.part_number;
    entry.length = spec.content.size();
    entry.crc = detail::crc16(spec.content);
    if (spec.check_type != CheckValueType::none) {
        auto value = compute_check_value(spec.check_type, spec.content);
        if (!value) {
            return std::unexpected{value.error()};
        }
        entry.check_value = std::move(*value);
    }
    listed.push_back(std::move(entry));
    contents.push_back({spec.name, spec.content});
    return {};
}

// Whether `content` is the file that `listed` describes.
Result<void> verify_file(const UserSuppliedFile& listed, const FileContent& content) {
    if (listed.file_name != content.name) {
        return std::unexpected{Error::inconsistent};
    }
    if (listed.length != content.bytes.size()) {
        return std::unexpected{Error::bad_length};
    }
    if (listed.crc != detail::crc16(content.bytes)) {
        return std::unexpected{Error::bad_crc};
    }
    if (listed.check_value) {
        const auto recomputed = compute_check_value(listed.check_value->type, content.bytes);
        // A type this library cannot calculate cannot be checked, and is left.
        if (recomputed && *recomputed != *listed.check_value) {
            return std::unexpected{Error::bad_check_value};
        }
    }
    return {};
}

template <typename Listed>
Result<void> verify_files(const std::vector<Listed>& listed,
                          const std::vector<FileContent>& contents) {
    if (listed.size() != contents.size()) {
        return std::unexpected{Error::inconsistent};
    }
    for (std::size_t i = 0; i < listed.size(); ++i) {
        if (auto result = verify_file(listed[i], contents[i]); !result) {
            return result;
        }
    }
    return {};
}

}  // namespace

Result<Load> build_load(const LoadSpec& spec) {
    HeaderFile header;
    header.part_flags = spec.part_flags;
    header.load_part_number = spec.part_number;
    header.load_type = spec.load_type;
    header.target_hardware_ids = spec.target_hardware_ids;
    header.target_hardware_ids_with_positions = spec.target_hardware_ids_with_positions;
    header.user_defined_data = spec.user_defined_data;

    Load load;
    load.header_file_name = spec.header_file_name;
    for (const LoadFileSpec& file : spec.data_files) {
        if (auto result = add_file(file, header.data_files, load.data_files); !result) {
            return std::unexpected{result.error()};
        }
    }
    for (const LoadFileSpec& file : spec.support_files) {
        if (auto result = add_file(file, header.support_files, load.support_files); !result) {
            return std::unexpected{result.error()};
        }
    }

    // The Load Check Value goes inside the header file and is calculated from it,
    // so the header is first written with a value of the right size in its place,
    // and written again once the value is known.
    if (spec.load_check_type != CheckValueType::none) {
        if (!IntegrityChecker::make(spec.load_check_type)) {
            return std::unexpected{Error::unsupported_check_type};
        }
        header.load_check_value = CheckValue{
            spec.load_check_type, std::vector<std::byte>(check_value_length(spec.load_check_type))};
    }
    auto bytes = encode_header_file(header);
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    if (spec.load_check_type != CheckValueType::none) {
        auto value =
            load_check_value(spec.load_check_type, *bytes, load.data_files, load.support_files);
        if (!value) {
            return std::unexpected{value.error()};
        }
        header.load_check_value = std::move(*value);
        bytes = encode_header_file(header);
        if (!bytes) {
            return std::unexpected{bytes.error()};
        }
    }

    // The Load CRC is the last to be known: it covers the Header File CRC, which
    // covers the Load Check Value.
    header.load_crc = load_crc(*bytes, load.data_files, load.support_files);
    bytes = encode_header_file(header);
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    load.header = std::move(header);
    load.header_bytes = std::move(*bytes);
    return load;
}

Result<void> verify_load(const Load& load) {
    const auto header = decode_header_file(load.header_bytes);
    if (!header) {
        return std::unexpected{header.error()};
    }
    if (auto result = verify_files(header->data_files, load.data_files); !result) {
        return result;
    }
    if (auto result = verify_files(header->support_files, load.support_files); !result) {
        return result;
    }

    if (header->load_check_value) {
        const auto value = load_check_value(header->load_check_value->type, load.header_bytes,
                                            load.data_files, load.support_files);
        // As for a file, a type that cannot be calculated cannot be checked.
        if (value && *value != *header->load_check_value) {
            return std::unexpected{Error::bad_check_value};
        }
    }
    if (header->load_crc != load_crc(load.header_bytes, load.data_files, load.support_files)) {
        return std::unexpected{Error::bad_crc};
    }
    return {};
}

Findings check_load(const Load& load) {
    Findings findings = check_header_file(load.header);
    Findings name = check_header_file_name(load.header_file_name);
    findings.insert(findings.end(), name.begin(), name.end());
    return findings;
}

}  // namespace adl::arinc665
