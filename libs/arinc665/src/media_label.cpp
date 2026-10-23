// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/media_label.hpp"

#include <array>
#include <string_view>

namespace adl::arinc665
{

namespace
{

constexpr std::array<std::string_view, 12> month_names{"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

void add(Findings& findings, std::string message)
{
    findings.push_back({"3.3.1", std::move(message)});
}

} // namespace

std::string format_label_date(const LabelDate& date)
{
    if (date.day < 1 || date.day > 31 || date.month < 1 || date.month > 12 || date.year > 99)
    {
        return {};
    }
    const auto two_digits = [](unsigned value)
    { return std::string{static_cast<char>('0' + value / 10), static_cast<char>('0' + value % 10)}; };
    return two_digits(date.day) + " " + std::string{month_names[date.month - 1U]} + " " + two_digits(date.year);
}

std::string format_label(const MediaLabel& label)
{
    std::string text;
    const auto line = [&text](const std::string& content)
    {
        text += content;
        text += '\n';
    };

    line(label.nomenclature);
    line("Set PN: " + std::string{label.media_set_part_number.view()});
    line("Disk " + std::to_string(label.media_sequence_number) + " of " +
         std::to_string(label.number_of_media_set_members));

    std::string software = "Software PNs:";
    for (const PartNumber& part_number : label.software_part_numbers)
    {
        software += ' ';
        software += part_number.view();
    }
    line(software);

    line(label.supplier);
    line("Set SN: " + label.serial_number);
    line(label.acceptance_stamp);
    if (label.creation_date)
    {
        line("Mfg. Date: " + format_label_date(*label.creation_date));
    }
    for (const std::string& item : label.other_items)
    {
        line(item);
    }
    return text;
}

Findings check_label(const MediaLabel& label)
{
    Findings findings;
    if (label.nomenclature.empty())
    {
        add(findings, "the label has no media set nomenclature");
    }
    for (Finding& finding : check_media_set_part_number(label.media_set_part_number.view()))
    {
        findings.push_back(std::move(finding));
    }
    if (label.media_sequence_number < 1 || label.media_sequence_number > label.number_of_media_set_members)
    {
        add(findings, "the media sequence number is not between 1 and the number of members");
    }
    if (label.software_part_numbers.empty())
    {
        add(findings, "the label lists no software part number (or refers to LOADS.LUM)");
    }
    if (label.supplier.empty())
    {
        add(findings, "the label has no supplier identification");
    }
    if (label.serial_number.empty())
    {
        add(findings, "the label has no media set serial number");
    }
    if (label.acceptance_stamp.empty())
    {
        add(findings, "the label has no product acceptance or release stamp");
    }
    if (label.creation_date && format_label_date(*label.creation_date).empty())
    {
        add(findings, "the media creation date is not a valid day, month and year");
    }
    return findings;
}

} // namespace adl::arinc665
