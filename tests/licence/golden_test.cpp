// SPDX-License-Identifier: GPL-3.0-or-later
// The licence files that an independent writer made (tests/golden/make_golden_licence.py): the library
// reads them, and writes the same bytes. A licence a customer holds must keep reading, so a change that
// makes this fail is a change of the file format.

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/test_keys.hpp"
#include "golden_licence.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

using namespace adl::licence;
using namespace adl::test;

namespace
{

RevocationList list()
{
    RevocationList value;
    value.sequence = 7;
    value.issued_at = at(2027, 2, 1);
    value.licence_ids = {"ADL-2026-000123"};
    value.keys = {{"lic-2026-a", at(2027, 1, 15)}, {"lic-2025-a", std::nullopt}};
    return value;
}

} // namespace

TEST_CASE("licence golden: an annual Pro licence reads, and is written as the same bytes", "[licence][golden]")
{
    const std::string file = hex_text(golden::licence_annual_hex);

    // Ed25519 is deterministic, so writing the licence again gives the same file.
    REQUIRE(file == signed_text(annual()));
    REQUIRE(hex_text(golden::licence_annual_payload_hex) == *write_payload(annual()));

    const Status result = evaluate(file, on(at(2027, 1, 1)));
    REQUIRE(result.state == State::valid);
    REQUIRE(result.reason == Reason::ok);
    REQUIRE(result.value);
    REQUIRE(*result.value == annual());
    REQUIRE(result.key_id == test_keys::key_id);
}

TEST_CASE("licence golden: an Enterprise licence with every optional field", "[licence][golden]")
{
    const std::string file = hex_text(golden::licence_full_hex);
    REQUIRE(file == signed_text(full()));

    const Status result = evaluate(file, on(at(2027, 1, 1)));
    REQUIRE(result.state == State::valid);
    REQUIRE(result.value);
    REQUIRE(*result.value == full());
    REQUIRE(result.value->machines.size() == 2);
    REQUIRE(result.value->updates_until.has_value());
    REQUIRE(result.value->licensee_email == "tests@example-lab.invalid");
}

TEST_CASE("licence golden: a revocation list", "[licence][golden]")
{
    const std::string file = hex_text(golden::revocations_list_hex);
    REQUIRE(file == signed_payload(*write_revocations(list()), revocations_purpose));
    const auto read = read_revocations(file, test_keys::trusted());
    REQUIRE(read);
    REQUIRE(*read == list());
    REQUIRE(hex_text(golden::revocations_list_payload_hex) == *write_revocations(list()));
}

TEST_CASE("licence golden: files that must be refused, and why", "[licence][golden]")
{
    const Context where = on(at(2027, 1, 1));

    const Status tampered = evaluate(hex_text(golden::licence_bad_signature_hex), where);
    REQUIRE(tampered.state == State::invalid);
    REQUIRE(tampered.reason == Reason::bad_signature);

    const Status newer = evaluate(hex_text(golden::licence_newer_format_hex), where);
    REQUIRE(newer.state == State::invalid);
    REQUIRE(newer.reason == Reason::newer_version);

    const Status unknown = evaluate(hex_text(golden::licence_unknown_key_hex), where);
    REQUIRE(unknown.state == State::invalid);
    REQUIRE(unknown.reason == Reason::newer_version);

    const auto duplicate = parse_payload(hex_bytes(golden::licence_duplicate_key_payload_hex));
    REQUIRE(!duplicate);
    REQUIRE(duplicate.error() == Error::malformed);

    const auto critical = parse_payload(hex_bytes(golden::licence_critical_unknown_payload_hex));
    REQUIRE(!critical);
    REQUIRE(critical.error() == Error::newer_version);

    const auto time = parse_time(hex_text(golden::licence_time_text_hex));
    REQUIRE(time);
    REQUIRE(format_time(*time) == "2026-11-13T02:10:05Z");
}
