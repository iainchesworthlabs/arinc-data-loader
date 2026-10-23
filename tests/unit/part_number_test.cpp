#include "adl/arinc665/part_number.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace adl::arinc665;

namespace
{

bool mentions(const Findings& findings, std::string_view clause)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

} // namespace

TEST_CASE("a part number keeps its text", "[part-number]")
{
    const PartNumber part_number{"ACM47-1234-5678"};

    CHECK(part_number.view() == "ACM47-1234-5678");
    CHECK(part_number.size() == 15);
    CHECK_FALSE(part_number.empty());
    CHECK(std::string(part_number.begin(), part_number.end()) == "ACM47-1234-5678");
    CHECK(PartNumber{}.empty());
    CHECK(PartNumber{"A"} == PartNumber{"A"});
    CHECK_FALSE(PartNumber{"A"} == PartNumber{"B"});
}

// Appendix E works the example out step by step.
TEST_CASE("the check characters are the exclusive-or of the characters, in hex", "[part-number]")
{
    CHECK(check_characters("ACM", "1234-5678") == "47");
    CHECK(check_characters("ACM", "12345678") == "47"); // delimiters do not count
    CHECK(check_characters("ACM", "0001-0001") == "4F");
    CHECK(check_characters("AAA", "AAAA-AAAA") == "41"); // an odd number of equal characters leaves one
    CHECK(check_characters("AAA", "AAAA-AAA0") == "30");
}

TEST_CASE("a load part number is made from the manufacturer's code and product identifier", "[part-number]")
{
    const auto part_number = make_load_part_number("ACM", "1234-5678");

    REQUIRE(part_number.has_value());
    CHECK(part_number->view() == "ACM47-1234-5678");
    CHECK(check_load_part_number(part_number->view()).empty());
}

TEST_CASE("a part number is refused for a code or identifier of the wrong form", "[part-number]")
{
    for (const auto& [code, id] :
         {std::pair{"AC", "1234-5678"}, std::pair{"acm", "1234-5678"}, std::pair{"ACM", "12345678"},
          std::pair{"ACM", "1234_5678"}, std::pair{"ACM", "12I4-5678"}, // I, O, Q and Z are left out
          std::pair{"ACM", "1234-56O8"}, std::pair{"ACM", "Q234-5678"}, std::pair{"ACM", "1234-567Z"},
          std::pair{"ACM", "1234-5678-9"}})
    {
        const auto part_number = make_load_part_number(code, id);

        REQUIRE_FALSE(part_number.has_value());
        CHECK(part_number.error() == Error::invalid_value);
    }
}

TEST_CASE("a load part number is checked against section 2.1.1", "[part-number]")
{
    CHECK(check_load_part_number("ACM47-1234-5678").empty());

    SECTION("the form")
    {
        CHECK(mentions(check_load_part_number("ACM47-12345678"), "2.1.1"));
        CHECK(mentions(check_load_part_number("ACM47_1234-5678"), "2.1.1"));
        CHECK(mentions(check_load_part_number(""), "2.1.1"));
    }
    SECTION("blanks")
    {
        CHECK(mentions(check_load_part_number("ACM47-1234 5678"), "2.1.1"));
    }
    SECTION("the characters")
    {
        CHECK(mentions(check_load_part_number("acm47-1234-5678"), "2.1.1"));
        CHECK(mentions(check_load_part_number("ACM47-1O34-5678"), "2.1.1"));
    }
    SECTION("the check characters")
    {
        const auto wrong = check_load_part_number("ACM48-1234-5678");
        CHECK(mentions(wrong, "2.1.3"));
        CHECK_FALSE(mentions(wrong, "2.1.1"));
        CHECK(mentions(check_load_part_number("ACMXY-1234-5678"), "2.1.3"));
        CHECK(mentions(check_load_part_number("ACM4f-1234-5678"), "2.1.3")); // upper case
    }
}

TEST_CASE("a media set part number is checked against section 3.1", "[part-number]")
{
    CHECK(check_media_set_part_number("ACM-SET-0001").empty());
    CHECK(check_media_set_part_number("123456789012345").empty()); // 15 is the longest

    CHECK(mentions(check_media_set_part_number("1234567890123456"), "3.1"));
    CHECK(mentions(check_media_set_part_number("ACM SET"), "3.1"));
    CHECK(mentions(check_media_set_part_number("ACM-SET-"), "3.1"));
    CHECK(mentions(check_media_set_part_number("ACM-QZ-0001"), "3.1"));
    CHECK(mentions(check_media_set_part_number(""), "3.1"));
}
