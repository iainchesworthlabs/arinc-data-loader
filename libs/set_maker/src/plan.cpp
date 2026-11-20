// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/set_maker/plan.hpp"

#include "adl/arinc665/naming.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace adl::set_maker
{

namespace
{

using adl::arinc665::CheckValueType;

constexpr std::array<LoadTypeCode, 4> load_types = {{
    {"OPS", "Operational software", 0x0001},
    {"NDB", "Navigation database", 0x0002},
    {"ADB", "Aeronautical database", 0x0003},
    {"TST", "Test software", 0x0004},
}};

constexpr std::size_t manufacturer_code_length = 3;

std::string first_characters(std::string_view text)
{
    return std::string{text.substr(0, manufacturer_code_length)};
}

} // namespace

std::string default_header_file_name(std::string_view load_part_number)
{
    return first_characters(load_part_number) + ".LUH";
}

std::string default_batch_file_name(std::string_view batch_part_number)
{
    return first_characters(batch_part_number) + "." + std::string{adl::arinc665::batch_file_extension};
}

std::string part_root_name(std::string_view load_part_number)
{
    std::string name;
    for (const char c : load_part_number)
    {
        if (c != '-')
        {
            name += c;
        }
    }
    return name;
}

std::string part_root_directory(std::string_view load_part_number)
{
    return "\\" + part_root_name(load_part_number) + "\\";
}

std::vector<PlannedBatchTarget> derive_batch_targets(std::span<const PlannedLoad> loads)
{
    std::vector<PlannedBatchTarget> targets;
    for (const PlannedLoad& load : loads)
    {
        for (const std::string& id : load.target_hardware_ids)
        {
            auto found = std::ranges::find(targets, id, &PlannedBatchTarget::id);
            if (found == targets.end())
            {
                targets.push_back(PlannedBatchTarget{id, {}});
                found = targets.end() - 1;
            }
            if (std::ranges::find(found->load_part_numbers, load.part_number) == found->load_part_numbers.end())
            {
                found->load_part_numbers.push_back(load.part_number);
            }
        }
    }
    return targets;
}

std::span<const LoadTypeCode> load_type_codes() noexcept
{
    return load_types;
}

std::optional<adl::arinc665::LoadType> load_type_from_code(std::string_view code)
{
    for (const LoadTypeCode& entry : load_types)
    {
        if (entry.code == code)
        {
            return adl::arinc665::LoadType{std::string{entry.description}, entry.id};
        }
    }
    return std::nullopt;
}

std::string check_type_word(CheckValueType type)
{
    switch (type)
    {
    case CheckValueType::none:
        return "none";
    case CheckValueType::crc8:
        return "crc8";
    case CheckValueType::crc16:
        return "crc16";
    case CheckValueType::crc32:
        return "crc32";
    case CheckValueType::md5:
        return "md5";
    case CheckValueType::sha1:
        return "sha1";
    }
    return "type_" + std::to_string(static_cast<unsigned>(type));
}

std::optional<CheckValueType> check_type_from_word(std::string_view word) noexcept
{
    constexpr std::array<std::pair<std::string_view, CheckValueType>, 6> words = {{
        {"none", CheckValueType::none},
        {"crc8", CheckValueType::crc8},
        {"crc16", CheckValueType::crc16},
        {"crc32", CheckValueType::crc32},
        {"md5", CheckValueType::md5},
        {"sha1", CheckValueType::sha1},
    }};
    for (const auto& [name, type] : words)
    {
        if (name == word)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string hex_of(std::span<const std::byte> bytes)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::byte byte : bytes)
    {
        const auto value = std::to_integer<unsigned>(byte);
        text.push_back(digits[value >> 4U]);
        text.push_back(digits[value & 0x0FU]);
    }
    return text;
}

std::string hex4_of(std::uint16_t value)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text(4, '0');
    for (std::size_t i = 4; i-- > 0; value = static_cast<std::uint16_t>(value >> 4U))
    {
        text[i] = digits[value & 0x0FU];
    }
    return text;
}

} // namespace adl::set_maker
