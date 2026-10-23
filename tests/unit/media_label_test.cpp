// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/media_label.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace adl::arinc665;

namespace
{

MediaLabel sample_label()
{
    MediaLabel label;
    label.nomenclature = "EEC Operational Software";
    label.media_set_part_number = PartNumber{"ACM-SET-0001"};
    label.media_sequence_number = 2;
    label.number_of_media_set_members = 3;
    label.software_part_numbers = {PartNumber{"ACM47-1234-5678"}, PartNumber{"ACM4F-0001-0001"}};
    label.supplier = "Acme Avionics (CAGE 12345)";
    label.serial_number = "000042";
    label.acceptance_stamp = "QA ACCEPTED";
    label.creation_date = LabelDate{14, 4, 98};
    return label;
}

bool mentions_text(const Findings& findings, std::string_view text)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.message.find(text) != std::string::npos; });
}

} // namespace

TEST_CASE("a date is written as DD XXX YY", "[label]")
{
    CHECK(format_label_date(LabelDate{14, 4, 98}) == "14 APR 98"); // the standard's example
    CHECK(format_label_date(LabelDate{1, 1, 0}) == "01 JAN 00");
    CHECK(format_label_date(LabelDate{31, 12, 99}) == "31 DEC 99");

    CHECK(format_label_date(LabelDate{0, 1, 1}).empty());
    CHECK(format_label_date(LabelDate{32, 1, 1}).empty());
    CHECK(format_label_date(LabelDate{1, 0, 1}).empty());
    CHECK(format_label_date(LabelDate{1, 13, 1}).empty());
    CHECK(format_label_date(LabelDate{1, 1, 100}).empty());
}

TEST_CASE("a label is written with the identification Table 3.3.1-3 recommends", "[label]")
{
    const auto text = format_label(sample_label());

    CHECK(text == "EEC Operational Software\n"
                  "Set PN: ACM-SET-0001\n"
                  "Disk 2 of 3\n"
                  "Software PNs: ACM47-1234-5678 ACM4F-0001-0001\n"
                  "Acme Avionics (CAGE 12345)\n"
                  "Set SN: 000042\n"
                  "QA ACCEPTED\n"
                  "Mfg. Date: 14 APR 98\n");
}

TEST_CASE("the optional items follow the required ones", "[label]")
{
    auto label = sample_label();
    label.creation_date.reset();
    label.other_items = {"Valid until 01 JAN 30", "Copyright Acme"};

    const auto text = format_label(label);

    CHECK(text.find("Mfg. Date") == std::string::npos);
    CHECK(text.ends_with("QA ACCEPTED\nValid until 01 JAN 30\nCopyright Acme\n"));
}

TEST_CASE("a complete label has nothing wrong with it", "[label]")
{
    CHECK(check_label(sample_label()).empty());
}

TEST_CASE("a label is checked for the items Table 3.3.1-1 requires", "[label]")
{
    SECTION("each one")
    {
        auto label = sample_label();
        label.nomenclature.clear();
        CHECK(mentions_text(check_label(label), "nomenclature"));

        label = sample_label();
        label.software_part_numbers.clear();
        CHECK(mentions_text(check_label(label), "software part number"));

        label = sample_label();
        label.supplier.clear();
        CHECK(mentions_text(check_label(label), "supplier"));

        label = sample_label();
        label.serial_number.clear();
        CHECK(mentions_text(check_label(label), "serial number"));

        label = sample_label();
        label.acceptance_stamp.clear();
        CHECK(mentions_text(check_label(label), "stamp"));
    }
    SECTION("the set part number and the sequence number")
    {
        auto label = sample_label();
        label.media_set_part_number = PartNumber{"ACM SET"};
        label.media_sequence_number = 4;
        const auto findings = check_label(label);
        CHECK(mentions_text(findings, "blank"));
        CHECK(mentions_text(findings, "sequence number"));
    }
    SECTION("a date that is not one")
    {
        auto label = sample_label();
        label.creation_date = LabelDate{40, 1, 1};
        CHECK(mentions_text(check_label(label), "creation date"));
    }
}
