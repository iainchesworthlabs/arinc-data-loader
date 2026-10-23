// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/naming.hpp"

#include <algorithm>
#include <array>

namespace adl::arinc665
{

namespace
{

// Table 3.2.2-1.
constexpr std::array<std::string_view, 20> reserved_extensions{"CRC", "DIR", "HDR", "LDR", "LCI", "LCL", "LCS",
                                                               "LNA", "LND", "LNL", "LNO", "LNR", "LNS", "LUB",
                                                               "LUH", "LUI", "LUM", "LUP", "LUR", "LUS"};

void add(Findings& findings, std::string clause, std::string message)
{
    findings.push_back({std::move(clause), std::move(message)});
}

bool is_upper_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool is_upper_hex(char c)
{
    return (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9');
}

void check_reserved(Findings& findings, std::string_view name, std::string_view allowed_1, std::string_view allowed_2,
                    std::string_view what)
{
    const auto extension = file_extension(name);
    if (is_reserved_extension(extension) && extension != allowed_1 && extension != allowed_2)
    {
        add(findings, "3.2.2",
            std::string{"the extension "} + std::string{extension} + " of a " + std::string{what} +
                " is reserved for another use");
    }
}

} // namespace

std::string_view file_extension(std::string_view name) noexcept
{
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string_view{} : name.substr(dot + 1);
}

bool is_reserved_extension(std::string_view extension) noexcept
{
    return std::find(reserved_extensions.begin(), reserved_extensions.end(), extension) != reserved_extensions.end();
}

Findings check_file_name(std::string_view name)
{
    Findings findings;
    if (name.empty())
    {
        add(findings, "2.2.2", "the file name is empty");
        return findings;
    }
    if (name.size() > 255)
    {
        add(findings, "2.2.2", "the file name is longer than 255 characters");
    }
    if (name == "." || name == "..")
    {
        add(findings, "2.2.2", "\".\" and \"..\" are not allowed as file names");
    }
    const auto unprintable = [](char c)
    {
        const auto value = static_cast<unsigned char>(c);
        return value < 0x21 || value > 0x7E; // controls, blank and anything beyond ASCII
    };
    if (std::any_of(name.begin(), name.end(), unprintable))
    {
        add(findings, "2.2.2", "the file name has a blank or a non-printable character");
    }
    if (name.find_first_of("~/:\\") != std::string_view::npos)
    {
        add(findings, "2.2.2", "the file name has one of the characters ~ / : \\");
    }
    return findings;
}

Findings check_header_file_name(std::string_view name)
{
    Findings findings = check_file_name(name);
    if (file_extension(name) != header_file_extension)
    {
        add(findings, "2.2.2.1", "the header file name does not end in .LUH");
    }
    if (name.size() < 3 || !std::all_of(name.begin(), name.begin() + 3, is_upper_alnum))
    {
        add(findings, "2.2.2", "the header file name does not begin with a manufacturer's code");
    }
    return findings;
}

Findings check_data_file_name(std::string_view name)
{
    Findings findings = check_file_name(name);
    check_reserved(findings, name, data_file_extension, data_file_extension, "data file");
    return findings;
}

Findings check_support_file_name(std::string_view name)
{
    Findings findings = check_file_name(name);
    check_reserved(findings, name, "LDR", "HDR", "support file");
    return findings;
}

Findings check_batch_file_name(std::string_view name)
{
    Findings findings = check_file_name(name);
    if (file_extension(name) != batch_file_extension)
    {
        add(findings, "2.3.1", "the batch file name does not end in .LUB");
    }
    return findings;
}

Findings check_pathname(std::string_view pathname)
{
    Findings findings;
    if (pathname.empty() || pathname.front() != '\\')
    {
        add(findings, "3.2.3.2.18", "the pathname does not start at the root with a backslash");
        return findings;
    }
    if (pathname.back() != '\\')
    {
        add(findings, "3.2.3.2.18", "the pathname does not end with a backslash");
    }
    const auto depth = std::count(pathname.begin(), pathname.end(), '\\') - 1;
    if (depth > 4)
    {
        add(findings, "3.2.1", "the file is more than four directories deep");
    }
    if (pathname.find("\\\\") != std::string_view::npos)
    {
        add(findings, "3.2.3.2.18", "the pathname has an empty directory name");
    }
    return findings;
}

Findings check_target_hardware_id(std::string_view id)
{
    Findings findings;
    if (id.size() < 4 || id.size() > 15)
    {
        add(findings, "1.5", "a target hardware ID is 4 to 15 characters long");
        return findings;
    }
    const bool is_429_equipment_code = id.size() == 4 && std::all_of(id.begin(), id.end(), is_upper_hex);
    const bool starts_with_manufacturer_code = std::all_of(id.begin(), id.begin() + 3, is_upper_alnum);
    if (!is_429_equipment_code && !starts_with_manufacturer_code)
    {
        add(findings, "1.5",
            "a target hardware ID is four hexadecimal characters or begins with a manufacturer's "
            "code");
    }
    return findings;
}

} // namespace adl::arinc665
