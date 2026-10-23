// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The part number type, the check characters of a load part number, and checks of
// part numbers against the formats of ARINC 665-3 sections 2.1 and 3.1.

#include "adl/arinc665/error.hpp"
#include "adl/arinc665/finding.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace adl::arinc665
{

// The part number of a load, a file in it, a batch or a media set, as written in
// the standard's text form. It is held as given: loaders have to read part
// numbers that do not follow section 2.1.1, so the type does not refuse them.
// check_load_part_number() and check_media_set_part_number() say whether one
// follows the recommendations.
class PartNumber
{
public:
    using const_iterator = std::string::const_iterator;

    PartNumber() = default;
    explicit PartNumber(std::string_view text) : text_(text) {}

    [[nodiscard]] std::string_view view() const noexcept
    {
        return text_;
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return text_.size();
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return text_.empty();
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return text_.begin();
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return text_.end();
    }

    friend bool operator==(const PartNumber&, const PartNumber&) = default;

private:
    std::string text_;
};

// The two check characters (CC) of a load part number, Appendix E: the
// exclusive-or of the ASCII characters of the manufacturer's code and the
// product identifier, delimiters left out, as two upper-case hexadecimal digits.
// "ACM" and "1234-5678" give "47".
[[nodiscard]] std::string check_characters(std::string_view manufacturer_code, std::string_view product_id);

// Builds MMMCC-SSSS-SSSS (section 2.1.1) from the manufacturer's code "MMM" and
// the product identifier "SSSS-SSSS", working out the check characters. Fails
// with Error::invalid_value if either is not in the form the standard gives.
[[nodiscard]] Result<PartNumber> make_load_part_number(std::string_view manufacturer_code, std::string_view product_id);

// Section 2.1.1 and 2.1.3: the MMMCC-SSSS-SSSS format, upper-case alphanumerics
// (SSSS-SSSS without I, O, Q and Z), delimiters where they belong, no blanks,
// and check characters that are the right ones. Used for load, data file,
// support file and batch part numbers.
[[nodiscard]] Findings check_load_part_number(std::string_view part_number);

// Section 3.1: at most 15 characters, no blanks, not ending in a hyphen, and
// without I, Q and Z (which the standard recommends against).
[[nodiscard]] Findings check_media_set_part_number(std::string_view part_number);

} // namespace adl::arinc665
