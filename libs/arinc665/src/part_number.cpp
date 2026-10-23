// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/part_number.hpp"

#include <algorithm>
#include <array>

namespace adl::arinc665
{

namespace
{

bool is_upper_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool is_upper_hex(char c)
{
    return (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9');
}

// Section 2.1.1: SSSS-SSSS leaves out the letters easily taken for digits.
bool is_product_character(char c)
{
    return is_upper_alnum(c) && c != 'I' && c != 'O' && c != 'Q' && c != 'Z';
}

bool is_manufacturer_code(std::string_view code)
{
    return code.size() == 3 && std::all_of(code.begin(), code.end(), is_upper_alnum);
}

// "SSSS-SSSS".
bool is_product_id(std::string_view id)
{
    if (id.size() != 9 || id[4] != '-')
    {
        return false;
    }
    return std::all_of(id.begin(), id.begin() + 4, is_product_character) &&
           std::all_of(id.begin() + 5, id.end(), is_product_character);
}

void add(Findings& findings, std::string clause, std::string message)
{
    findings.push_back({std::move(clause), std::move(message)});
}

} // namespace

std::string check_characters(std::string_view manufacturer_code, std::string_view product_id)
{
    unsigned sum = 0;
    for (const std::string_view part : {manufacturer_code, product_id})
    {
        for (const char c : part)
        {
            if (c != '-')
            {
                sum ^= static_cast<unsigned char>(c);
            }
        }
    }
    constexpr std::string_view digits = "0123456789ABCDEF";
    return {digits[(sum >> 4) & 0xFU], digits[sum & 0xFU]};
}

Result<PartNumber> make_load_part_number(std::string_view manufacturer_code, std::string_view product_id)
{
    if (!is_manufacturer_code(manufacturer_code) || !is_product_id(product_id))
    {
        return std::unexpected{Error::invalid_value};
    }
    std::string text{manufacturer_code};
    text += check_characters(manufacturer_code, product_id);
    text += '-';
    text += product_id;
    return PartNumber{text};
}

Findings check_load_part_number(std::string_view part_number)
{
    Findings findings;
    if (part_number.find(' ') != std::string_view::npos)
    {
        add(findings, "2.1.1", "the part number has an embedded blank");
    }
    // MMMCC-SSSS-SSSS is 15 characters with a delimiter after the fifth and tenth.
    if (part_number.size() != 15 || part_number[5] != '-' || part_number[10] != '-')
    {
        add(findings, "2.1.1", "the part number is not in the form MMMCC-SSSS-SSSS");
        return findings;
    }

    const auto manufacturer = part_number.substr(0, 3);
    const auto check = part_number.substr(3, 2);
    const auto product = part_number.substr(6, 9);

    if (!is_manufacturer_code(manufacturer))
    {
        add(findings, "2.1.1", "the manufacturer's code is not three upper-case alphanumeric characters");
    }
    if (!is_product_id(product))
    {
        add(findings, "2.1.1",
            "the product identifier is not SSSS-SSSS of upper-case alphanumerics other than I, O, "
            "Q and Z");
    }
    if (!std::all_of(check.begin(), check.end(), is_upper_hex))
    {
        add(findings, "2.1.3", "the check characters are not two upper-case hexadecimal digits");
    }
    else if (is_manufacturer_code(manufacturer) && check != check_characters(manufacturer, product))
    {
        add(findings, "2.1.3", "the check characters are not the ones worked out from the rest of the part number");
    }
    return findings;
}

Findings check_media_set_part_number(std::string_view part_number)
{
    Findings findings;
    if (part_number.empty())
    {
        add(findings, "3.1", "the media set part number is empty");
        return findings;
    }
    if (part_number.size() > 15)
    {
        add(findings, "3.1", "the media set part number is longer than 15 characters");
    }
    if (part_number.find(' ') != std::string_view::npos)
    {
        add(findings, "3.1", "the media set part number has an embedded blank");
    }
    if (part_number.back() == '-')
    {
        add(findings, "3.1", "the media set part number ends in a hyphen");
    }
    if (part_number.find_first_of("IQZ") != std::string_view::npos)
    {
        add(findings, "3.1", "the media set part number uses I, Q or Z, which the standard recommends against");
    }
    return findings;
}

} // namespace adl::arinc665
