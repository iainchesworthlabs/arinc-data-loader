// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/naming.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace adl::arinc665;

namespace
{

bool mentions(const Findings& findings, std::string_view clause)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

} // namespace

TEST_CASE("the extension is what follows the last dot", "[naming]")
{
    CHECK(file_extension("ACM47.LUH") == "LUH");
    CHECK(file_extension("a.b.c") == "c");
    CHECK(file_extension("NOEXTENSION").empty());
    CHECK(file_extension("TRAILING.").empty());
}

TEST_CASE("the reserved extensions are those of Table 3.2.2-1", "[naming]")
{
    for (const auto extension : {"CRC", "DIR", "HDR", "LDR", "LCI", "LCL", "LCS", "LNA", "LND", "LNL",
                                 "LNO", "LNR", "LNS", "LUB", "LUH", "LUI", "LUM", "LUP", "LUR", "LUS"})
    {
        CHECK(is_reserved_extension(extension));
    }
    CHECK_FALSE(is_reserved_extension("BIN"));
    CHECK_FALSE(is_reserved_extension("luh")); // names are case-sensitive
    CHECK_FALSE(is_reserved_extension(""));
}

TEST_CASE("a file name is checked against section 2.2.2", "[naming]")
{
    CHECK(check_file_name("ACM47A.LUP").empty());
    CHECK(check_file_name("CONFIG.LDR").empty()); // the standard's own file, with an I and an O
    CHECK(check_file_name(std::string(255, 'A')).empty());

    CHECK(mentions(check_file_name(""), "2.2.2"));
    CHECK(mentions(check_file_name(std::string(256, 'A')), "2.2.2"));
    CHECK(mentions(check_file_name("."), "2.2.2"));
    CHECK(mentions(check_file_name(".."), "2.2.2"));
    CHECK(mentions(check_file_name("A B"), "2.2.2"));
    CHECK(mentions(check_file_name("A~B"), "2.2.2"));
    CHECK(mentions(check_file_name("A/B"), "2.2.2"));
    CHECK(mentions(check_file_name("A:B"), "2.2.2"));
    CHECK(mentions(check_file_name("A\\B"), "2.2.2"));
    CHECK(mentions(check_file_name(std::string{"A\tB"}), "2.2.2"));
    CHECK(mentions(check_file_name("A\xC3\xA9"), "2.2.2"));
}

TEST_CASE("a header file name ends in LUH and begins with a manufacturer's code", "[naming]")
{
    CHECK(check_header_file_name("ACM47.LUH").empty());

    CHECK(mentions(check_header_file_name("ACM47.LUP"), "2.2.2.1"));
    CHECK(mentions(check_header_file_name("ACM47"), "2.2.2.1"));
    CHECK(mentions(check_header_file_name("acm47.LUH"), "2.2.2"));
    CHECK(mentions(check_header_file_name("A.LUH"), "2.2.2"));
}

TEST_CASE("a data file may not use another use's extension, but LUP is the one to use", "[naming]")
{
    CHECK(check_data_file_name("APP.LUP").empty());
    CHECK(check_data_file_name("APP.BIN").empty());
    CHECK(check_data_file_name("APP.001").empty());
    CHECK(check_data_file_name("APP").empty());

    CHECK(mentions(check_data_file_name("APP.LUH"), "3.2.2"));
    CHECK(mentions(check_data_file_name("APP.LDR"), "3.2.2"));
    CHECK(mentions(check_data_file_name("APP.LUM"), "3.2.2"));
}

TEST_CASE("a support file may be CONFIG.LDR or a Boeing header, and not other reserved names", "[naming]")
{
    CHECK(check_support_file_name("CONFIG.LDR").empty());
    CHECK(check_support_file_name("EXCONFIG.LDR").empty());
    CHECK(check_support_file_name("ACMSSSSS.HDR").empty());
    CHECK(check_support_file_name("README.TXT").empty());

    CHECK(mentions(check_support_file_name("X.LUP"), "3.2.2"));
    CHECK(mentions(check_support_file_name("X.LUH"), "3.2.2"));
}

TEST_CASE("a batch file name ends in LUB", "[naming]")
{
    CHECK(check_batch_file_name("ACM47.LUB").empty());
    CHECK(mentions(check_batch_file_name("ACM47.LUH"), "2.3.1"));
}

TEST_CASE("a pathname starts at the root, ends in a backslash and is at most four deep", "[naming]")
{
    CHECK(check_pathname("\\").empty());
    CHECK(check_pathname("\\ACM4712345678\\").empty());
    CHECK(check_pathname("\\A\\B\\C\\D\\").empty());

    CHECK(mentions(check_pathname(""), "3.2.3.2.18"));
    CHECK(mentions(check_pathname("A\\"), "3.2.3.2.18"));
    CHECK(mentions(check_pathname("\\A"), "3.2.3.2.18"));
    CHECK(mentions(check_pathname("\\A\\\\B\\"), "3.2.3.2.18"));
    CHECK(mentions(check_pathname("\\A\\B\\C\\D\\E\\"), "3.2.1"));
}

TEST_CASE("a target hardware ID is four hexadecimal characters or begins with a manufacturer's code", "[naming]")
{
    CHECK(check_target_hardware_id("0A2B").empty());
    CHECK(check_target_hardware_id("ACME").empty());
    CHECK(check_target_hardware_id("ACM1234567890AB").empty()); // 15

    CHECK(mentions(check_target_hardware_id("ABC"), "1.5"));
    CHECK(mentions(check_target_hardware_id("ACM1234567890ABC"), "1.5"));
    CHECK(mentions(check_target_hardware_id("0a2b"), "1.5"));
    CHECK(mentions(check_target_hardware_id("!!!!"), "1.5"));
}
