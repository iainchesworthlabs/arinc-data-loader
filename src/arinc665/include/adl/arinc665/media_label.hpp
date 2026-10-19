#pragma once

// The text of the label on a member of a media set (ARINC 665-3 section 3.3): the
// items it carries, those laid out as lines of text, and a check that the
// required items are present. Printing it is left to whatever uses it.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/part_number.hpp"

namespace adl::arinc665 {

// A date as a media label writes it: "DD XXX YY", for example "14 APR 98"
// (Table 3.3.1-2).
struct LabelDate {
    std::uint8_t day = 1;    // 1 to 31
    std::uint8_t month = 1;  // 1 to 12
    std::uint8_t year = 0;   // the last two digits, 0 to 99

    friend bool operator==(const LabelDate&, const LabelDate&) = default;
};

// The text on the label of one member of a media set (section 3.3). The seven
// items of Table 3.3.1-1 are required; the creation date is the one optional
// item of Table 3.3.1-2 that has a format, and the others are text as given.
struct MediaLabel {
    std::string nomenclature;  // the title: target hardware or system, and the type of software
    PartNumber media_set_part_number;
    std::uint8_t media_sequence_number = 1;
    std::uint8_t number_of_media_set_members = 1;
    std::vector<PartNumber> software_part_numbers;  // the loads on the set
    std::string supplier;                           // the name or CAGE code
    std::string serial_number;
    std::string acceptance_stamp;
    std::optional<LabelDate> creation_date;
    std::vector<std::string> other_items;  // validity date, copyright notice, ...

    friend bool operator==(const MediaLabel&, const MediaLabel&) = default;
};

// The label's text, one item to a line, in the order of importance of Table
// 3.3.1-1 and with the identification Table 3.3.1-3 recommends ("Set PN:",
// "Set SN:", "Software PNs:", "Disk x of y", "Mfg. Date:"). Layout, type size
// and colour (section 3.3.2) are for whatever prints it.
[[nodiscard]] std::string format_label(const MediaLabel& label);

// A date as "DD XXX YY"; empty if the day or the month is out of range.
[[nodiscard]] std::string format_label_date(const LabelDate& date);

// The places a label lacks an item Table 3.3.1-1 requires, or has a media set
// part number or sequence number the standard does not allow.
[[nodiscard]] Findings check_label(const MediaLabel& label);

}  // namespace adl::arinc665
